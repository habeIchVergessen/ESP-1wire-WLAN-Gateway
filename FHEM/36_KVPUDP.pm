# $Id: 00_KVPUDP.pm 7911 2015-12-10 21:11:31Z habeIchVergessen $
################################################################
#
#  Copyright notice
#
#  (c) 2014 Copyright: Dr. Boris Neubert
#  e-mail: omega at online dot de
#
#  This file is part of fhem.
#
#  Fhem is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 2 of the License, or
#  (at your option) any later version.
#
#  Fhem is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with fhem.  If not, see <http://www.gnu.org/licenses/>.
#
################################################################################

package main;

use strict;
use warnings;

use IO::Socket::Multicast;
use Time::HiRes qw(gettimeofday);

my %matchlist = (
    "1:KeyValueProtocol" => "^OK\\sVALUES\\s",
   );
my $mCastAddr = '239.0.0.57';
my $mCastPort = 12345;

sub KVPUDP_SendHTTP ($$$);

#####################################
sub
KVPUDP_Initialize($)
{
  my ($hash) = @_;
  
# Provider
  #$hash->{WriteFn} = "KVPUDP_Write";
  $hash->{ReadFn}  = "KVPUDP_Read";
  #$hash->{Clients} = ":KeyValueProtocol";
  #$hash->{MatchList} = \%matchlist;
  #$hash->{ReadyFn} = "KVPUDP_Ready";

# Consumer
  $hash->{DefFn}         = "KVPUDP_Define";
  $hash->{UndefFn}       = "KVPUDP_Undef";
  $hash->{ReadyFn}       = "KVPUDP_Ready";
  #$hash->{GetFn}        = "KVPUDP_Get";
  $hash->{SetFn}         = "KVPUDP_Set";
  $hash->{AttrFn}        = "KVPUDP_Attr";
  $hash->{FingerprintFn} = "KVPUDP_Fingerprint";
  $hash->{AttrList}	 = "PEERS ChipIDList";
  $hash->{STATE}         = "Initialized";
}

#####################################
sub
KVPUDP_Define($$)
{
  my ($hash, $def) = @_;
  my $name = $hash->{NAME};
  my $type = $hash->{TYPE};

  if (defined($modules{KVPUDP}{defptr}{Instance})) {
    return "KVPUDP can be instantiated once per fhem instance (" . $modules{KVPUDP}{defptr}{Instance} . ")";
  }
  
  eval "use LWP::UserAgent";
  return "\nERROR: Please install LWP::UserAgent" if($@);

  eval "use HTTP::Request::Common";
  return "\nERROR: Please install HTTP::Request::Common" if($@);

  Log3 $hash, 3, "$name: Opening multicast socket...";
  my $socket = IO::Socket::Multicast->new(
    Domain    => AF_INET,
    Proto     => 'udp',
    LocalPort => $mCastPort,
    ReuseAddr => 1,	# true
  );

  if (!defined($socket)) {
    return "error create multicast socket";
  }

  $socket->mcast_add($mCastAddr);
  
  $hash->{TCPDev}= $socket;
  $hash->{FD} = $socket->fileno();
  $hash->{Clients} = ":KeyValueProtocol";
  $hash->{MatchList} = \%matchlist;

  delete($readyfnlist{"$name"});
  $selectlist{"$name"} = $hash;
  $hash->{STATE} = "Opened";

  $modules{KVPUDP}{defptr}{Instance} = $name;

  return undef;
}

#####################################
sub
KVPUDP_Fingerprint($$) {
}

#####################################
sub
KVPUDP_Undef($$)
{
  my ($hash, $arg) = @_;
  
  my $socket= $hash->{TCPDev};
  $socket->mcast_drop($mCastAddr);
  $socket->close;

  delete $modules{KVPUDP}{defptr}{Instance};

  return undef;
}
#####################################
sub
KVPUDP_Ready($) {
  my ($hash) = @_;

  my $name = $hash->{NAME};

  Log3 $hash, 3, "$name: Ready";

  return 1;
}

#####################################
sub
KVPUDP_DoInit($)
{
  my $hash = shift;
 
  $hash->{STATE} = "Initialized" if(!$hash->{STATE});

  return undef;
}

#####################################
sub KVPUDP_PeerIgnored(@) {
  my (@params) = @_;
  my $hash = shift @params;
  my $name = $hash->{NAME};

  if (!defined($hash->{PEERS})) {
    return undef;
  }

  my $Peer = shift @params;
  my $ChipIDList = (@params >= 1 ? shift @params : AttrVal($name, "ChipIDList", undef));

  foreach my $peer (sort keys %{ $hash->{PEERS} }) {
    next if (defined($Peer) && $peer ne $Peer);

    my $chipID = $hash->{PEERS}{$peer}{ChipID};
    if (defined($ChipIDList) && $ChipIDList ne "" && defined($chipID) && $ChipIDList !~ m/(^|,)${chipID}(,|$)/) {
      $hash->{PEERS}{$peer}{Ignored} = 'Y';
    } else {
      delete $hash->{PEERS}{$peer}{Ignored};
    }
  }

  return undef;
}

#####################################
sub KVPUDP_RemoveDuplicatedChipID($$$) {
  my ($hash, $peer, $chipID) = @_;

  foreach my $remote (keys %{ $hash->{PEERS} }) {
    next if ($remote eq $peer);

    if (defined($hash->{PEERS}{$remote}{ChipID}) && $hash->{PEERS}{$remote}{ChipID} eq $chipID) {
      Log3 $hash, 3, "KVPUDP_RemoveDuplicatedChipID: ChipID $chipID got new ip $peer ($remote)";
      delete $hash->{PEERS}{$remote};
    }
  }
}

#####################################
# called from the global loop, when the select for hash->{FD} reports data
sub KVPUDP_Read($) 
{
  my ($hash) = @_;
  my $name= $hash->{NAME};
  my $socket= $hash->{TCPDev};
  my $data;
  return unless $socket->recv($data, 128);

  my $remote = $socket->peerhost();
  my $serviceMsg = ($data eq "REFRESH CONFIG REQUEST");

  if (!defined($hash->{PEERS}{$remote}) || $serviceMsg) {
    $hash->{MatchList} = \%matchlist;
    $hash->{PEERS}{$remote} = {IP => $remote} if (!defined($hash->{PEERS}{$remote}));

    my $req = HTTP::Request->new(GET => "http://" . $remote . "/config?Version=&ChipID=&MAC=&Dictionary=");

    my $dummy;
    my ($resp, $elapsedTime) = KVPUDP_SendHTTP(30, $req, $dummy);
    if ($resp->is_success) {
      Log3 $hash, 3, "KVPUDP_Read: got config $remote";
      my $message = $resp->decoded_content;
      Log3 $name, 3, "got config: " . $message . " (" . $elapsedTime . " ms)";
      while (my ($key, $val, $toParse) = $message =~ m/(\w+):(\w+[\w\.,=-]*)(,(\w+:.*)|$)/g ) {
	$hash->{PEERS}{$remote}{$key} = $val;
	$message = $toParse;
      }
    } else {
      Log3 $hash, 3, "KVPUDP_Read: error reading config $remote (" . $resp->status_line . ")";
    }
    KVPUDP_PeerIgnored($hash, $remote);
    KVPUDP_RemoveDuplicatedChipID($hash, $remote, $hash->{PEERS}{$remote}{ChipID}) if (defined($hash->{PEERS}{$remote}{ChipID}));
  }

  $hash->{RAWMSG} = $data;
  $hash->{MSGCNT}++;
  $hash->{TIME} = TimeNow();

  $hash->{PEERS}{$remote}{RAWMSG} = $hash->{RAWMSG};
  $hash->{PEERS}{$remote}{TIME} = $hash->{TIME};
  $hash->{PEERS}{$remote}{MSGCNT}++;

  my %addvals = (IP => $remote);
  # add mapping
  if (defined($hash->{PEERS}{$remote}{"Dictionary"})) {
    $addvals{Mapping} = $hash->{PEERS}{$remote}{"Dictionary"};
  }

  Log3 $hash, 3, "$name: Received " . length($data) . " bytes from '" . $remote . "''";
  Dispatch($hash, $data, \%addvals) if (!$serviceMsg && !defined($hash->{PEERS}{$remote}{Ignored}));  # dispatch result to KeyValueProtocol
}

sub KVPUDP_WriteCmd ($@) {
  my ($peer, @params) = @_;
  my $cmds = "";

  while (my $param = shift @params) {
    my ($val, $cmd) = $param =~ m/^(-{0,1}\d*,{0,1}\d*)([a-zA-Z]{1})$/;

    if (!defined($cmd)) {
      return "unknown command '$param'";
    }
    $cmds .= (length($cmds) ? "&" : "") . "$cmd=$val";
  }

  if (length($cmds) == 0) {
    return "no commands to process";
  }

  if (defined($peer->{ChipID})) {
    $cmds="ChipID=" . $peer->{ChipID} . "&" . $cmds;
  }

  my $req = HTTP::Request->new(POST => "http://" . $peer->{IP} . "/config");
  $req->content($cmds);

  my $dummy;
  my ($resp, $elapsedTime) = KVPUDP_SendHTTP(1, $req, $dummy);

  if ($resp->is_success) {
    return "set raw: peer response " . $resp->code . " (" . $elapsedTime . " ms)";
  } else {
    return "set raw: peer response " . $resp->code . " " . $resp->decoded_content;
  }
}

sub KVPUDP_Flash($@) {
  my ($peer, @params) = @_;

  my $file = shift @params;
  
  return "set flash: file $file not found!" if ($file && !-e $file);

  my ($prog) = $peer->{Version} =~ m/^(\w+)/;
  return "set flash: couldn't read program name from version info! please specify a filename." if (!$prog && !$file);

  ($prog) = (split(/[\/\\]/, $file))[-1] =~ m/^(\w+)/ if (!$prog && $file);

  if (!-e $file) {
    $file = './FHEM/firmware/' . $prog . ".bin";
    return "set flash: file $file not found!" if (!-e $file);
  }

  my $req = POST("http://" . $peer->{IP} . "/ota/" . $peer->{ChipID} . ".bin",
      Content_Type => 'multipart/form-data',
      Content => [ file => [$file, $prog . '.bin'] ]);

  my $dummy;
  my ($resp, $elapsedTime) = KVPUDP_SendHTTP(60, $req, $dummy);

  if ($resp->is_success) {
    return "set flash: peer response " . $resp->code . " (" . $elapsedTime . " ms)";
  } else {
    return "set flash: peer response " . $resp->code . " " . $resp->decoded_content;
  }
}

sub KVPUDP_SendHTTP ($$$) {
  my ($timeout, $req) = @_;

  my $ua = LWP::UserAgent->new;
  $ua->timeout($timeout);

  my $startReq = gettimeofday;
  my $resp = $ua->request($req);
  my $elapsedTime = int((gettimeofday - $startReq) * 1000);

  return ($resp, $elapsedTime);
}

sub KVPUDP_GetPeerList($) {
  my ($hash) = @_;

  my $result = "";

  foreach my $peer (sort keys %{ $hash->{PEERS} }) {
    $result .= "\n\t" . $peer . ": ";;
    if ($hash->{PEERS}{$peer}{Version}) {
      $result .= $hash->{PEERS}{$peer}{Version}
    }
    if ($hash->{PEERS}{$peer}{ChipID}) {
      $result .= "\@" . $hash->{PEERS}{$peer}{ChipID}
    }
    if ($hash->{PEERS}{$peer}{TIME}) {
      $result .= " (" . $hash->{PEERS}{$peer}{TIME} . ")"
    }
  }

  return $result;
}

sub KVPUDP_Set($@) {
  my ($hash, @params) = @_;

  my $name = shift @params;
  my $cmd = shift @params;

  my $list = "raw flash";

  if ($cmd =~ m/(raw|flash)/) {
    my ($peer) = shift @params;

    my $result;

    if ($peer eq "?") {
      $result = "set $cmd <peer IP>";
      $result .= " <list of commands>" if ($cmd eq "raw");
      $result .= " [name of firmware file; otherwise look for ./FHEM/firmware/<PROGNAME from Version>.bin]" if ($cmd eq "flash");
      return $result;
    }

    if (!defined($peer)) {
      return "set $cmd requires as first argument a peer (IP)!" . KVPUDP_GetPeerList($hash);
    }

    if (!defined($hash->{PEERS}{$peer})) {
      return "set $cmd: argument peer is unknown!" . KVPUDP_GetPeerList($hash);
    }

    $result = KVPUDP_WriteCmd($hash->{PEERS}{$peer}, @params) if ($cmd eq "raw");
    $result = KVPUDP_Flash($hash->{PEERS}{$peer}, @params) if ($cmd eq "flash");

    return $result;
  } else {
    return "Unknown argument $cmd, choose one of " . $list;
  }  

  return undef;
}

sub KVPUDP_Attr(@) {
  my ($cmd,$name,$aName,$aVal) = @_;
  my $hash = $defs{$name};

  if ($aName eq "ChipIDList") {
    return "use deleteattr to clear ChipIDList" if ($cmd eq "set" && $aVal eq "");

    KVPUDP_PeerIgnored($hash, undef, $aVal);
  }

  return undef;
}

#############################
1;
#############################


=pod
=begin html

<a name="KVPUDP"></a>
<h3>KVPUDP</h3>
<ul>
  <br>

  <a name="KVPUDP"></a>
  <b>Define</b>
  <ul>
    <code>define &lt;name&gt; KVPUDP</code><br>
    <br>
    Defines a Hexabus. You need one Hexabus to receive multicast messages from <a href="#KVPUDPDevice">Hexabus devices</a>.
    Have a look at the <a href="https://github.com/mysmartgrid/hexabus/wiki">Hexabus wiki</a> for more information on Hexabus.
    <br><br>
    You need the perl modules IO::Socket::Multicast6 and Digest::CRC. Under Debian and its derivatives they are installed with <code>apt-get install libio-socket-multicast6-perl libdigest-crc-perl</code>.
  </ul>  

</ul>


=end html
