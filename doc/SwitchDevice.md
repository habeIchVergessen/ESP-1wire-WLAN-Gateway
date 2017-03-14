**class [Esp1wire](./Esp1wire.md)::SwitchDevice : [Device](./Device.md)**

***public***

| | method | description |
| --- | --- | --- |
| bool | **getChannelInfo([SwitchChannelStatus](#switchchannelstatus) \*channelStatus)** | read channel status |
| bool | **getMemoryStatus([SwitchMemoryStatus](#switchmemorystatus) \*memoryStatus)** | read memory status |
| bool | **setConditionalSearch([ConditionalSearchPolarity](#conditionalsearchpolarity) csPolarity, [ConditionalSearchSourceSelect](#conditionalsearchsourceselect) csSourceSelect, [ConditionalSearchChannelSelect](#conditionalsearchchannelselect) csChannelSelect, [ChannelFlipFlop](#channelflipflop) channelFlipFlop)** | configure conditional search |
| bool | **resetAlarm([SwitchChannelStatus](#switchchannelstatus) \*channelStatus)** | read current status and reset alarm afterwards |

***SwitchChannelStatus***

| | name | description |
| --- | --- | --- |
| uint8_t | noChannels | number of channels |
| bool | parasite | parasite mode |
| bool | latchA | value of latch.A |
| bool | senseA | value of sense.A |
| bool | flipFlopQA | value of FlipFlopQ.A |
| bool | latchB | value of latch.B |
| bool | senseB | value of sense.B |
| bool | flipFlopQB | value of FlipFlopQ.B |

***SwitchMemoryStatus***

| | name | description |
| --- | --- | --- |
| ConditionalSearchPolarity | csPolarity | |
| ConditionalSearchSourceSelect | csSourceSelect | |
| ConditionalSearchChannelSelect | csChannelSelect | |
| ChannelFlipFlop | channelFlipFlop | |
| bool | parasite | parasite mode |

***ConditionalSearchPolarity***

| name | description |
| --- | --- |
| ConditionalSearchPolarityLow  | answer on low value |
| ConditionalSearchPolarityHigh | answer on high value |
      
***ConditionalSearchChannelSelect***

| name | description |
| --- | --- |
| ChannelSelectDisabled | disable conditional search |
| ChannelSelectA | answer conditional search for channel A |
| ChannelSelectB | answer conditional search for channel B |
| ChannelSelectBoth | answer conditional search for both channels |

***ConditionalSearchSourceSelect***

| name | description |
| --- | --- |
| SourceSelectActivityLatch | answer conditional search for any activity |
| SourceSelectChannelFlipFlop | not tested yet |
| SourceSelectPIOStatus | not tested yet |

***ChannelFlipFlop***

| name | description |
| --- | --- |
| ChannelFlipFlopA | enable pullup on channel A |
| ChannelFlipFlopB | enable pullup on channel B |
| ChannelFlipFlopBoth | enable pullup on both channels |
