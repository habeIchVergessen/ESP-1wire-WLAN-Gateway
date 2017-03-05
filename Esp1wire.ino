#include "Esp1wire.h"

Esp1wire::Esp1wire() {
}

#ifdef _DEBUG_TEST_DATA
void Esp1wire::testData() {
// test crap

  uint8_t data[2] = { 0xFF, 0x5E };
  int16_t fpTemperature =
    (((int16_t) data[0]) << 11) |
    (((int16_t) data[1]) << 3);
  float temperature = (float)fpTemperature * 0.0078125;
  Serial.println("temperature = " + String(temperature));
}
#endif

bool Esp1wire::probeI2C(uint8_t sda, uint8_t scl) {
  mSDA = sda;
  mSCL = scl;

#ifdef _DEBUG_SETUP
  Serial.println("\nscanning i2c bus:");
#endif
  Wire.begin(mSDA, mSCL);

  uint8_t busCount = getBusCount();
  unsigned long i2cStart = micros();

  // probe I2C busmaster
  for (byte i = 0; i <= 3; i++) {
    byte addr = (0x18 | i);
    Wire.beginTransmission(addr);
    byte error = Wire.endTransmission();

    if (error == 0) {
      DS2482 *ds2482 = new DS2482(addr);

      if (ds2482->reset())
        addBusmaster(ds2482, addr, (ds2482->selectChannel(0) ? DS2482_800 : DS2482_100));
      else
        free(ds2482);
    }
  }
#ifdef _DEBUG_SETUP
  Serial.println("done " + elapTime(i2cStart));
#endif

  return (busCount < getBusCount());
}

bool Esp1wire::probeGPIO(uint8_t gpio) {
#ifdef _DEBUG_SETUP
  Serial.print("\nprobe gpio #" + (String)gpio + ": ");
#endif

  uint8_t busCount = getBusCount();

  OneWire *oneWire = new OneWire(gpio);

  // probe gpio
  if (oneWire->reset())
    addBus(new BusGPIO(oneWire, gpio));
  else
    free(oneWire);

#ifdef _DEBUG_SETUP
  Serial.println((busCount < getBusCount()) ? F("ok") : F("failed"));
#endif
  return (busCount < getBusCount());
}

bool Esp1wire::addBusmaster(DS2482 *ds2482, byte i2cPort, BusmasterType busmasterType) {
  Busmaster *busmaster = new Busmaster(ds2482, i2cPort, busmasterType);

#ifdef _DEBUG_SETUP
  Serial.print("found " +  busmaster->getName() + " @ 0x"); Serial.println(i2cPort, HEX);
#endif
  if (busmasterType == DS2482_800) {
    for (uint8_t ch = 7; ch >= 0; ch--) {
      if (busmaster->selectChannel(ch))
        addBus(new BusIC(busmaster, ch));
    }
  } else
    addBus(new BusIC(busmaster));

  return true;
}

bool Esp1wire::addBus(Bus *bus) {
  BusList *busList = new BusList();
  busList->bus = bus;
  busList->next = NULL;

  if (firstBus == NULL)
    firstBus = lastBus = busList;
  else {
    lastBus->next = busList;
    lastBus = busList;
  }

  mBusListCount++;
}

bool Esp1wire::resetSearch() {
  BusList *curr = firstBus;

  freeAlarmFilter();

  uint8_t bus = 1;
  while (curr != NULL) {
#if defined(_DEBUG_TIMING) || defined(_DEBUG_SETUP)
    Serial.print("\nbus #" + (String)bus + ": scanning ");
    unsigned long start = micros();
#endif
    curr->bus->resetSearch();

#if defined(_DEBUG_TIMING) || defined(_DEBUG_SETUP)
    Serial.println(" " + elapTime(start));
#endif
    curr = curr->next;
    bus++;
  }
}

Esp1wire::AlarmFilter Esp1wire::alarmSearch(DeviceType targetSearch) {
  BusList *curr = firstBus;

  freeAlarmFilter();
  alarmFilter = new AlarmFilter();

  uint8_t bus = 1;
  while (curr != NULL) {
#if defined(_DEBUG_TIMING) || defined(_DEBUG_SETUP)
    Serial.print("bus #" + (String)bus + ": alarm search ");
    unsigned long start = micros();
#endif
    curr->bus->alarmSearch(alarmFilter, targetSearch);

#if defined(_DEBUG_TIMING) || defined(_DEBUG_SETUP)
    Serial.println(" " + elapTime(start));
#endif
    curr = curr->next;
    bus++;
  }

  return *alarmFilter;
}

bool Esp1wire::requestTemperatures (bool resetIgnoreAlarmFlags) {
  BusList *curr = firstBus;

  uint8_t bus = 1;
  while (curr != NULL) {
    if (curr->bus->getTemperatureDeviceCount() > 0) {
      if (resetIgnoreAlarmFlags)
        curr->bus->resetIgnoreAlarmFlags(DeviceTypeTemperature);
#if defined(_DEBUG_TIMING) || defined(_DEBUG)
      Serial.print("\nbus #" + (String)bus + ": request temperatures ");
      unsigned long start = micros();
#endif
      HelperTemperatureDevice::requestTemperatures(curr->bus);
#if defined(_DEBUG_TIMING) || defined(_DEBUG)
      Serial.println(" " + elapTime(start));
#endif
    }

    curr = curr->next;
    bus++;
  }
}

bool Esp1wire::requestBatteries() {
  BusList *curr = firstBus;

  uint8_t bus = 1;
  while (curr != NULL) {
    if (curr->bus->getTemperatureDeviceCount() > 0) {
#if defined(_DEBUG_TIMING) || defined(_DEBUG)
      Serial.print("\nbus #" + (String)bus + ": request batteries ");
      unsigned long start = micros();
#endif
      HelperBatteryDevice::requestBatteries(curr->bus);
#if defined(_DEBUG_TIMING) || defined(_DEBUG)
      Serial.println(" " + elapTime(start));
#endif
    }

    curr = curr->next;
    bus++;
  }
}

void Esp1wire::freeAlarmFilter() {
  if (alarmFilter != NULL) {
    free(alarmFilter);
    alarmFilter = NULL;
  }
}

// class Esp1wire Busmaster
Esp1wire::Busmaster::Busmaster(DS2482 *ds2482, byte i2cPort, BusmasterType busmasterType) {
  mDS2482         = ds2482;
  mI2CPort        = i2cPort;
  mBusmasterType  = busmasterType;
}

bool Esp1wire::Busmaster::selectChannel(uint8_t channel) {
  bool result;

  if ((result = mDS2482->selectChannel(channel)))
    mSelectedChannel = channel;

  return result;
}

bool Esp1wire::Busmaster::wireReset() {
  return mDS2482->wireReset();
}

void Esp1wire::Busmaster::wireSelect(uint8_t *address) {
  mDS2482->wireSelect(address);
}

void Esp1wire::Busmaster::wireWriteByte(uint8_t b) {
  mDS2482->wireWriteByte(b);
}

uint8_t Esp1wire::Busmaster::wireReadBit() {
  return mDS2482->wireReadBit();
}

void Esp1wire::Busmaster::wireReadBytes(uint8_t *data, uint16_t len) {
  for (uint16_t i = 0; i < len; i++)
    data[i] = mDS2482->wireReadByte();
}

void Esp1wire::Busmaster::wireResetSearch() {
  mDS2482->wireResetSearch();

  // search crap
  searchExhausted = 0;
  searchLastDiscrepancy = 0;

  for (uint8_t i = 0; i < 8; i++)
    searchAddress[i] = 0;
}

uint8_t Esp1wire::Busmaster::busyWait(bool setReadPtr) {
  uint8_t status;
  int loopCount = 1000;
  while ((status = mDS2482->wireReadStatus(setReadPtr)) & DS2482_STATUS_BUSY)
  {
    if (--loopCount <= 0)
    {
      mTimeout = 1;
      break;
    }
    delayMicroseconds(20);
  }
  return status;
}

bool Esp1wire::Busmaster::wireSearch(uint8_t *address, bool alarm, DeviceType targetSearch) {
  uint8_t i;
  uint8_t direction;
  uint8_t last_zero = 0;

  // use target search for switches
  if (alarm)
    switch (targetSearch) {
      case DeviceTypeSwitch:
        searchAddress[0] = DS2406;
        searchLastDiscrepancy = 64;
        break;
    }

  if (searchExhausted || !wireReset())
    return false;

  busyWait(true);
  wireWriteByte((!alarm ? owcNormalSearch : owcAlarmSearch));

  for (i = 1; i < 65; i++)
  {
    int romByte = (i - 1) >> 3;
    int romBit = 1 << ((i - 1) & 7);

    if (i < searchLastDiscrepancy)
      direction = searchAddress[romByte] & romBit;
    else
      direction = i == searchLastDiscrepancy;

    busyWait();
    Wire.beginTransmission(mI2CPort);
    Wire.write(0x78);
    Wire.write(direction ? 0x80 : 0);
    Wire.endTransmission();
    uint8_t status = busyWait();

    uint8_t id = status & DS2482_STATUS_SBR;
    uint8_t comp_id = status & DS2482_STATUS_TSB;
    direction = status & DS2482_STATUS_DIR;

    if (id && comp_id)
      return false;
    else {
      if (!id && !comp_id && !direction)
        last_zero = i;
    }

    if (direction)
      searchAddress[romByte] |= romBit;
    else
      searchAddress[romByte] &= (uint8_t)~romBit;
  }

  searchLastDiscrepancy = last_zero;

  if (last_zero == 0)
    searchExhausted = 1;
  for (i = 0; i < 8; i++)
    address[i] = searchAddress[i];

  return true;
}

String Esp1wire::Busmaster::getName() {
  return (String)(mBusmasterType == DS2482_800 ? F("DS2482-800") : F("DS2482-100"));
}

Esp1wire::BusmasterType Esp1wire::Busmaster::getType() {
  return mBusmasterType;
}

void Esp1wire::Busmaster::wireStrongPullup(bool pullup) {
  if (pullup)
    mDS2482->configure(DS2482_CONFIG_SPU | DS2482_CONFIG_APU);
}

// class Esp1wire Bus
void Esp1wire::Bus::registerTemperatureDevice(bool parasite, uint8_t resolution) {
  mTemperatureDeviceCount++;

  if (parasite)
    mStatus |= statusParasiteRead;

  if (resolution != TemperatureDevice::resolutionUnknown && resolution > TemperatureDevice::resolution9bit)
    mStatus |= resolution;
}

void Esp1wire::Bus::deviceDetected(uint8_t *address) {
  DeviceType deviceType = getDeviceType(address);

  if (this->crc8(address, 7) != address[7]) {
#ifdef _DEBUG_SETUP
    Serial.print(HelperDevice::getOneWireDeviceID(address) + " (crc error)");
#endif
    return;
  }

  // add first device
  if (firstDevice == NULL) {
    firstDevice = lastDevice = new DeviceList();
    firstDevice->device = new Device(this, address, deviceType);
    firstDevice->next = NULL;
    mDeviceListCount++;

#ifdef _DEBUG_SETUP
    Serial.print(".");
#endif
    return;
  }

  // probe last device
  int8_t addrComp = ((HelperDevice*)lastDevice->device)->compareAddress(address);
  if ( addrComp < 0) {
    DeviceList *deviceList = new DeviceList();
    deviceList->device = new Device(this, address, deviceType);
    deviceList->next = NULL;
    lastDevice->next = deviceList;
    lastDevice = deviceList;
    mDeviceListCount++;

#ifdef _DEBUG_SETUP
    Serial.print(".");
#endif
    return;
  }

  // probe whole list
  DeviceList *currDevice = firstDevice, *prevDevice = firstDevice;
  while (currDevice != NULL) {
    addrComp = ((HelperDevice*)currDevice->device)->compareAddress(address);

    // device already known
    if (addrComp == 0) {
#ifdef _DEBUG_SETUP
      Serial.print("-");
#endif
      break;
    }

    // probe next
    if (addrComp < 0) {
      if (currDevice != firstDevice)
        prevDevice = currDevice;
      currDevice = currDevice->next;
      continue;
    }

    // insert here
#ifdef _DEBUG_SETUP
    Serial.print("+");
#endif
    DeviceList *deviceList = new DeviceList();
    deviceList->device = new Device(this, address, deviceType);
    deviceList->next = currDevice;

    if (prevDevice == firstDevice)
      firstDevice = deviceList;
    else
      prevDevice->next = deviceList;
  }
}

Esp1wire::DeviceType Esp1wire::Bus::getDeviceType(uint8_t *address) {
  DeviceType deviceType = DeviceTypeUnsupported;

  switch (address[0]) {
    case DS18S20:
    case DS1822:
    case DS18B20:
    case DS1825:
    case DS28EA00:
      deviceType = DeviceTypeTemperature;
      break;
    case DS2406:
      deviceType = DeviceTypeSwitch;
      break;
    case DS2423:
      deviceType = DeviceTypeCounter;
      break;
    case DS2438:
      deviceType = DeviceTypeBattery;
      break;
  }

  return deviceType;
}

void Esp1wire::Bus::resetIgnoreAlarmFlags(DeviceType deviceType) {
  DeviceList *temp = firstDevice;

  while (temp != NULL) {
    if (temp->device->getDeviceType() & deviceType)
      temp->device->setIgnoreAlarmSearch(false);
    temp = temp->next;
  }
}

uint8_t Esp1wire::Bus::crc8(uint8_t *address, uint8_t len) {
  return OneWire::crc8(address, len);
}

uint16_t Esp1wire::Bus::crc16(uint8_t *address, uint8_t len, uint16_t crc) {
  return OneWire::crc16(address, len, crc);
}

void Esp1wire::Bus::wireWriteBytes(uint8_t *bytes, uint8_t len) {
  for (uint8_t i = 0; i < len; i++)
    wireWriteByte(bytes[i]);
}

// class Esp1wire BusIC
Esp1wire::BusIC::BusIC(Busmaster *busmaster) {
  mBusmaster = busmaster;
}

Esp1wire::BusIC::BusIC(Busmaster *busmaster, uint8_t channel) {
  mBusmaster = busmaster;
  mChannel = channel;
}

bool Esp1wire::BusIC::reset() {
  return mBusmaster->wireReset();
}

bool Esp1wire::BusIC::resetSearch() {
  selectChannel();
  wireResetSearch();

  uint8_t address[8];

  while (mBusmaster->wireSearch(address))
    deviceDetected(address);

  return true;
}

bool Esp1wire::BusIC::alarmSearch(AlarmFilter *alarmFilter, DeviceType targetSearch) {
  selectChannel();
  wireResetSearch();

  uint8_t address[8];
  DeviceList *currList = firstDevice;    // set to first element

  while (mBusmaster->wireSearch(address, true, targetSearch)) {
    while (currList != NULL) {
      int8_t addrComp = ((HelperDevice*)currList->device)->compareAddress(address);
      if (addrComp == 0) {
        ((HelperAlarmFilter*)alarmFilter)->addDevice(currList->device);
        break;
      }
      if (addrComp < 0) {
        currList = currList->next;
        continue;
      }
      if (addrComp > 0) {
        Serial.println("BusIC::alarmSearch: new device found " + HelperDevice::getOneWireDeviceID(address));
        break;
      }
    }
  }

  return true;
}

bool Esp1wire::BusIC::selectChannel() {
  if (mBusmaster->getType() != DS2482_800)
    return false;

  return mBusmaster->selectChannel(mChannel);
}

void Esp1wire::BusIC::wireResetSearch() {
  mBusmaster->wireResetSearch();
}

void Esp1wire::BusIC::wireSelect(uint8_t *address) {
  return mBusmaster->wireSelect(address);
}

void Esp1wire::BusIC::wireWriteByte(uint8_t b) {
  mBusmaster->wireWriteByte(b);
}

uint8_t Esp1wire::BusIC::wireReadBit() {
  return mBusmaster->wireReadBit();
}

void Esp1wire::BusIC::wireReadBytes(uint8_t *data, uint16_t len) {
  mBusmaster->wireReadBytes(data, len);
}

String Esp1wire::BusIC::getBusInformation() {
  return (String)F("BusIC: ") + mBusmaster->getName() + (String)F(" ") + (String)(mBusmaster->getType() == DS2482_800 ? (String)F(" ch: ") + (String)mChannel : (String)F(""));
}

Esp1wire::Bus::DeviceList* Esp1wire::BusIC::getFirstDevice() {
  return firstDevice;
}

void Esp1wire::BusIC::setPowerSupply(bool power) {
  if (!power) {
    // no action here (handled by reset)
    mStatus &= ~statusParasiteOn;
  } else {
    mBusmaster->wireStrongPullup(true);
    mStatus |= statusParasiteOn;
  }
}

// class Esp1wire BusGPIO
Esp1wire::BusGPIO::BusGPIO(OneWire *oneWire, uint8_t gpio) {
  mOneWire  = oneWire;
  mGPIOPort = gpio;
}

bool Esp1wire::BusGPIO::reset() {
  return (mOneWire->reset() == 1);
}

bool Esp1wire::BusGPIO::resetSearch() {
  if (!mOneWire->reset())
    return false;
  wireResetSearch();

  uint8_t address[8];

  while (mOneWire->search(address))
    deviceDetected(address);

  return true;
}

bool Esp1wire::BusGPIO::alarmSearch(AlarmFilter *alarmFilter, DeviceType targetSearch) {
  if (!mOneWire->reset())
    return false;
  wireResetSearch();

  // use target search for switches
  switch (targetSearch) {
    case DeviceTypeSwitch:
      mOneWire->target_search(DS2406);
      break;
  }

  uint8_t address[8];
  DeviceList *currList = firstDevice;

  while (mOneWire->search(address, true))
    while (currList != NULL) {
      int8_t addrComp = ((HelperDevice*)currList->device)->compareAddress(address);
      if (addrComp == 0) {
        ((HelperAlarmFilter*)alarmFilter)->addDevice(currList->device);
        break;
      }
      if (addrComp < 0) {
        currList = currList->next;
        continue;
      }
      if (addrComp > 0) {
        Serial.println("BusGPIO::alarmSearch: new device found " + HelperDevice::getOneWireDeviceID(address));
        break;
      }
    }

  return true;
}

void Esp1wire::BusGPIO::wireResetSearch() {
  mOneWire->reset_search();
}

void Esp1wire::BusGPIO::wireSelect(uint8_t *address) {
  mOneWire->select(address);
}

void Esp1wire::BusGPIO::wireWriteByte(uint8_t b) {
  mOneWire->write(b, powerSupply());
}

uint8_t Esp1wire::BusGPIO::wireReadBit() {
  return mOneWire->read_bit();
}

void Esp1wire::BusGPIO::wireReadBytes(uint8_t *data, uint16_t len) {
  mOneWire->read_bytes(data, len);
}

String Esp1wire::BusGPIO::getBusInformation() {
  return (String)F("BusGPIO: gpio: #") + (String)mGPIOPort;
}

Esp1wire::Bus::DeviceList* Esp1wire::BusGPIO::getFirstDevice() {
  return firstDevice;
}

void Esp1wire::BusGPIO::setPowerSupply(bool power) {
  if (!power) {
    mOneWire->depower();
    mStatus &= ~statusParasiteOn;
  } else  // power stays on after next write
    mStatus |= statusParasiteOn;
}

// class Esp1wire Device
Esp1wire::Device::Device(Bus *bus, uint8_t *address, DeviceType deviceType) {
  mBus = bus;
  memcpy(mAddress, address, sizeof(mAddress));
  mDeviceType = deviceType;

  if (deviceType == DeviceTypeTemperature) {
    uint8_t data[9];
    TemperatureDevice::TemperatureResolution tempRes = TemperatureDevice::resolutionUnknown;
    if (HelperTemperatureDevice::readScratch(mBus, mAddress, data)) {
      tempRes = HelperTemperatureDevice::resolution(mAddress, data);
      if (tempRes != TemperatureDevice::resolutionUnknown && tempRes > TemperatureDevice::resolution9bit)
        mStatus |= tempRes;
    }
    bool powerSupply = HelperTemperatureDevice::powerSupply(mBus, mAddress);
    mStatus |= statusParasiteRead;
    if (powerSupply)
      mStatus |= statusParasiteOn;
#ifdef _DEBUG
    Serial.println("Device::Device: temp res: " + String(tempRes, HEX) + " para: " + String(powerSupply, HEX));
#endif
    bus->registerTemperatureDevice(powerSupply, tempRes);
  }
  if (deviceType == DeviceTypeSwitch) {
#ifdef _DEBUG
    Serial.print("Device::Device: switch ");
#endif
    SwitchDevice::SwitchMemoryStatus memoryStatus;
    if (HelperSwitchDevice::readStatus(mBus, mAddress, &memoryStatus) && memoryStatus.parasite) {
        mStatus |= statusParasiteOn;
#ifdef _DEBUG
    Serial.print("parasite ");
#endif
    }
    SwitchDevice::SwitchChannelStatus channelStatus;
    if (HelperSwitchDevice::channelAccessInfo(mBus, mAddress, &channelStatus)) {
#ifdef _DEBUG
    Serial.print("ch " + String(channelStatus.noChannels) + " ");
#endif
    }
  }
  if (deviceType == DeviceTypeCounter) {
    uint32_t counter;
#ifdef _DEBUG
    Serial.print("Device::Device: counter ");
    if (HelperCounterDevice::counter(mBus, mAddress, &counter, 1))
      Serial.print(" #1 " + String(counter));
    if (HelperCounterDevice::counter(mBus, mAddress, &counter, 2))
      Serial.print(" #2 " + String(counter));
    Serial.println();
#endif
  }
  if (deviceType == DeviceTypeBattery) {
    bool powerSupply = false;
    mStatus |= statusParasiteRead;
    if (powerSupply)
      mStatus |= statusParasiteOn;
#ifdef _DEBUG
    Serial.println("Device::Device: battery");
#endif
    bus->registerTemperatureDevice(powerSupply, TemperatureDevice::resolution12bit);
  }
}

String Esp1wire::Device::getName() {
  if (mDeviceType == DeviceTypeUnsupported)
    return (String)F("unsupported type 0x") + String(mAddress[0], HEX);

  switch (mAddress[0]) {
    case DS18S20:
      return F("DS18S20");
      break;
    case DS1822:
      return F("DS1822");
      break;
    case DS18B20:
      return F("DS18B20");
      break;
    case DS1825:
      return F("DS1825");
      break;
    case DS28EA00:
      return F("DS28EA00");
      break;
    case DS2406:
      return F("DS2406");
      break;
    case DS2423:
      return F("DS2423");
      break;
    case DS2438:
      return F("DS2438");
      break;
  }

  return "Device::getName: 0x" + String(mAddress[0], HEX);
}

String Esp1wire::Device::getOneWireDeviceID() {
  return HelperDevice::getOneWireDeviceID(mAddress);
}

// class HelperDevice
String Esp1wire::HelperDevice::getOneWireDeviceID(uint8_t *address) {
  String SerialNumber = "";

  for (uint8_t i = 0; i < 8; i++)
    SerialNumber += (String)(i == 1 || i == 7 ? "." : "") + (String)(address[i] < 0x10 ? "0" : "") + String(address[i], HEX);

  return SerialNumber;
}

int8_t Esp1wire::HelperDevice::compareAddress(uint8_t *address) {
  for (byte i = 0; i < 8; i++) {
    if (mAddress[i] < address[i])
      return -1;
    if (mAddress[i] > address[i])
      return 1;
  }

  return 0;
}

// class TemperatureDevice
bool Esp1wire::TemperatureDevice::readScratch(uint8_t data[9]) {
  return HelperTemperatureDevice::readScratch(mBus, mAddress, data);
}

bool Esp1wire::TemperatureDevice::writeScratch(uint8_t data[9]) {
  return HelperTemperatureDevice::writeScratch(mBus, mAddress, data);
}

bool Esp1wire::TemperatureDevice::readPowerSupply() {
  if (mDeviceType != DeviceTypeTemperature)
    return false;

  if (mStatus & statusParasiteOn)
    return true;

  if (mStatus & statusParasiteRead)
    return false;

  bool powerSupply = HelperTemperatureDevice::powerSupply(mBus, mAddress);
  mStatus |= statusParasiteRead;
  if (powerSupply)
    mStatus |= statusParasiteOn;

  return (mStatus & statusParasiteOn);
}

bool Esp1wire::TemperatureDevice::getAlarmTemperatures(int8_t *lowTemperature, int8_t *highTemperature) {
  if (mDeviceType != DeviceTypeTemperature)
    return false;

  uint8_t data[9];

  if (!readScratch(data))
    return false;

  *lowTemperature = (int8_t)data[spfLowAlarm];
  *highTemperature = (int8_t)data[spfHighAlarm];

  return true;
}

bool Esp1wire::TemperatureDevice::setAlarmTemperatures(int8_t lowTemperature, int8_t highTemperature) {
  if (mDeviceType != DeviceTypeTemperature)
    return false;

  uint8_t data[9];

  if (!readScratch(data))
    return false;

  // keep equal values
  if (lowTemperature == (int8_t)data[spfLowAlarm] && highTemperature == (int8_t)data[spfHighAlarm])
    return false;
  
  if (lowTemperature > 125)
    data[spfLowAlarm] = 125;
  else if (lowTemperature < -55)
    data[spfLowAlarm] = -55;
  else
    data[spfLowAlarm] = lowTemperature;

  if (highTemperature > 125)
    data[spfHighAlarm] = 125;
  else if (highTemperature < -55)
    data[spfHighAlarm] = -55;
  else
    data[spfHighAlarm] = highTemperature;

  return writeScratch(data);
}

bool Esp1wire::TemperatureDevice::readTemperatureC(float *temperature) {
  if (mDeviceType == DeviceTypeBattery)
    return HelperBatteryDevice::readTemperatureC(mBus, mAddress, temperature);
    
  if (mDeviceType != DeviceTypeTemperature)
    return false;

  uint8_t data[9];

  if (!readScratch(data))
    return false;

  int16_t fpTemperature =
    (((int16_t) data[spfMSB]) << 11) |
    (((int16_t) data[spfLSB]) << 3);

  if (mAddress[0] == DS18S20) {
    fpTemperature = ((fpTemperature & 0xfff0) << 3) - 16 +
                    (
                      ((data[spfCntPerC] - data[spfCntRemain]) << 7) /
                      data[spfCntPerC]
                    );
  }

  *temperature = (float)fpTemperature * 0.0078125;

  return true;
}

bool Esp1wire::TemperatureDevice::requestTemperatureC(float *temperature) {
  if (mDeviceType & DeviceTypeTemperature)
    return false;

  bool result;

  if ((result = HelperTemperatureDevice::requestTemperatures(mBus, mAddress)))
    result = readTemperatureC(temperature);

  return result;
}

Esp1wire::TemperatureDevice::TemperatureResolution Esp1wire::TemperatureDevice::readResolution() {
  TemperatureResolution res = resolutionUnknown;

  uint8_t data[9];
  if (mDeviceType != DeviceTypeTemperature || !readScratch(data))
    return res;

  return HelperTemperatureDevice::resolution(mAddress, data);
}

// class SwitchDevice
bool Esp1wire::SwitchDevice::readStatus(SwitchMemoryStatus *memoryStatus) {
  if (mDeviceType != DeviceTypeSwitch)
    return false;

  return HelperSwitchDevice::readStatus(mBus, mAddress, memoryStatus);
}

bool Esp1wire::SwitchDevice::getChannelInfo(SwitchChannelStatus *channelStatus) {
  if (mDeviceType != DeviceTypeSwitch)
    return false;

  if (!channelAccessInfo(channelStatus))
    return false;

  return true;
}

bool Esp1wire::SwitchDevice::channelAccessInfo(SwitchChannelStatus *channelStatus, bool resetAlarm) {
  if (mDeviceType != DeviceTypeSwitch)
    return false;

  return HelperSwitchDevice::channelAccessInfo(mBus, mAddress, channelStatus, resetAlarm);
}

bool Esp1wire::SwitchDevice::getMemoryStatus(SwitchMemoryStatus *memoryStatus) {
  if (mDeviceType != DeviceTypeSwitch)
    return false;

  return HelperSwitchDevice::readStatus(mBus, mAddress, memoryStatus);
}
bool Esp1wire::SwitchDevice::setConditionalSearch(ConditionalSearchPolarity csPolarity, ConditionalSearchSourceSelect csSourceSelect, ConditionalSearchChannelSelect csChannelSelect, ChannelFlipFlop channelFlipFlop) {
  if (mDeviceType != DeviceTypeSwitch)
    return false;

  uint8_t data[1] =  { channelFlipFlop | csChannelSelect | csSourceSelect | csPolarity };
  return HelperSwitchDevice::writeStatus(mBus, mAddress, data);
}

bool Esp1wire::SwitchDevice::resetAlarm(SwitchChannelStatus *channelStatus) {
  if (mDeviceType != DeviceTypeSwitch)
    return false;

  if (!getChannelInfo(channelStatus))
    return false;

  SwitchChannelStatus resetStatus;
  return channelAccessInfo(&resetStatus, true);
}

// class CounterDevice
bool Esp1wire::CounterDevice::getCounter(uint32_t *counter1, uint32_t *counter2) {
  if (mDeviceType != DeviceTypeSwitch)
    return false;

  return counter(counter1, counter2);
}

bool Esp1wire::CounterDevice::counter(uint32_t *counter1, uint32_t *counter2) {
  return HelperCounterDevice::counter(mBus, mAddress, counter1, 1) && HelperCounterDevice::counter(mBus, mAddress, counter2, 2);
}

// class BatteryDevice
bool Esp1wire::BatteryDevice::readTemperatureC(float *temperature) {
  if (mDeviceType != DeviceTypeBattery)
    return false;

  return HelperBatteryDevice::readTemperatureC(mBus, mAddress, temperature);
}
bool Esp1wire::BatteryDevice::requestTemperatureC(float *temperature) {
  if (mDeviceType != DeviceTypeBattery)
    return false;

  return HelperBatteryDevice::requestTemperatureC(mBus, mAddress, temperature);
}

bool Esp1wire::BatteryDevice::readBattery(float *voltage, float *current, float *capacity, float resistorSens) {
  if (mDeviceType != DeviceTypeBattery)
    return false;

  return HelperBatteryDevice::readBattery(mBus, mAddress, voltage, current, capacity, resistorSens);
}

bool Esp1wire::BatteryDevice::requestBattery(float *voltage, float *current, float *capacity, float resistorSens) {
  if (mDeviceType != DeviceTypeBattery)
    return false;

  return HelperBatteryDevice::requestBattery(mBus, mAddress, voltage, current, capacity, resistorSens);
}

// class HelperTemperatureDevice
bool Esp1wire::HelperTemperatureDevice::requestTemperatures(Bus *bus) {
  if (!bus->reset())
    return false;
  bus->wireWriteByte(owcSkip);

  // enable parasite
  if (bus->parasite())
    bus->setPowerSupply(true);

  bus->wireWriteByte(owtcStartConversion);
  unsigned long now = millis(), delms = 750; // TODO delms depends on resolution (max yet)
  while (!isConversionComplete(bus) && (millis() - delms < now))
    ;

  // disable parasite
  if (bus->parasite())
    bus->setPowerSupply(false);

  return true;
}

bool Esp1wire::HelperTemperatureDevice::requestTemperatures(Bus *bus, uint8_t *address) {
  if (!bus->reset())
    return false;
  bus->wireSelect(address);

  // enable parasite
  if (bus->parasite())
    bus->setPowerSupply(true);

  bus->wireWriteByte(owtcStartConversion);
  unsigned long now = millis(), delms = 750; // TODO delms depends on resolution (max yet)
  while (!isConversionComplete(bus) && (millis() - delms < now))
    ;

  // disable parasite
  if (bus->parasite())
    bus->setPowerSupply(false);

  return true;
}

bool Esp1wire::HelperTemperatureDevice::isConversionComplete(Bus *bus) {
  if (bus->parasite())  // no completion in parasite mode
    return false;

  uint8_t b = bus->wireReadBit();

  return (b == 1);
}

bool Esp1wire::HelperTemperatureDevice::readScratch(Bus *bus, uint8_t *address, uint8_t data[9]) {
  if (!bus->reset())
    return false;

  bus->wireSelect(address);
  bus->wireWriteByte(owtcReadScratch);
  bus->wireReadBytes(data, 9);
  bool reset = bus->reset();

  return reset && (bus->crc8(data, 8) == data[spfCRC]);
}

bool Esp1wire::HelperTemperatureDevice::writeScratch(Bus *bus, uint8_t *address, uint8_t data[9]) {
  if (!bus->reset())
    return false;

  bus->wireSelect(address);
  bus->reset();
  bus->wireSelect(address);
  bus->wireWriteByte(owtcWriteScratch);
  bus->wireWriteByte(data[spfHighAlarm]); // high alarm temp
  bus->wireWriteByte(data[spfLowAlarm]); // low alarm temp

  // DS1820 and DS18S20 have no configuration register
  if (address[0] != DS18S20)
    bus->wireWriteByte(data[spfConfig]);

  bus->reset();

  // save the newly written values to eeprom
  bus->wireSelect(address);

  // enable parasite
  if (bus->parasite())
    bus->setPowerSupply(true);

  bus->wireWriteByte(owtcCopyScratch);

  delay(20);  // <--- added 20ms delay to allow 10ms long EEPROM write operation (as specified by datasheet)

  if (bus->parasite())
    delay(10); // 10ms delay

  // disable parasite
  if (bus->parasite())
    bus->setPowerSupply(false);

  bus->reset();
}

Esp1wire::TemperatureDevice::TemperatureResolution Esp1wire::HelperTemperatureDevice::resolution(uint8_t *address, uint8_t data[9]) {
  TemperatureResolution res = resolutionUnknown;

  if (address[0] == DS18S20)
    return resolution12bit;

  switch (data[spfConfig]) {
    case owtrResolution9bit:
      res = resolution9bit;
      break;
    case owtrResolution10bit:
      res = resolution10bit;
      break;
    case owtrResolution11bit:
      res = resolution11bit;
      break;
    case owtrResolution12bit:
      res = resolution12bit;
      break;
  }

  return res;
}

bool Esp1wire::HelperTemperatureDevice::powerSupply(Bus *bus, uint8_t *address) {
  bool result = false;

  if (bus->reset()) {
    bus->wireSelect(address);
    bus->wireWriteByte(owtcReadPowerSupply);
    result = (bus->wireReadBit() == 0);
    bus->reset();
  }

  return result;
}

// class HelperSwitchDevice
bool Esp1wire::HelperSwitchDevice::readStatus(Bus *bus, uint8_t *address, SwitchMemoryStatus *memoryStatus) {
  uint8_t cmd[3] = {
    owscReadStatus    // command
    , 0, 0            // address TA1 & TA2
  }, data[10];

  if (!bus->reset())
    return false;

  bus->wireSelect(address);
  bus->wireWriteBytes(cmd, sizeof(cmd));
  bus->wireReadBytes(data, 10);
  bool result = bus->reset();

  // write send commands to crc
  uint16_t crc16 = bus->crc16(cmd, sizeof(cmd));
  // apply received data to crc
  crc16 = ~bus->crc16(data, 8, crc16);

  result = result && ((crc16 & 0xFF) == data[smfCRC0]) && ((crc16 >> 8) == data[smfCRC1]);

  if (result) {
    memoryStatus->csPolarity      = (ConditionalSearchPolarity)(data[smfStatus] & smbfPolarity);
    memoryStatus->csSourceSelect  = (ConditionalSearchSourceSelect)(data[smfStatus] & (smbfSrcSelA | smbfSrcSelB));
    memoryStatus->csChannelSelect = (ConditionalSearchChannelSelect)(data[smfStatus] & (smbfChSelPioA | smbfChSelPioB));
    memoryStatus->channelFlipFlop = (ChannelFlipFlop)(data[smfStatus] & (smbfPioA | smbfPioB));
    memoryStatus->parasite        = (data[smfStatus] & smbfPowerSupply) == 0;
  }
  
  return result;
}

bool Esp1wire::HelperSwitchDevice::writeStatus(Bus *bus, uint8_t *address, uint8_t data[1]) {
  bool result = false;

  uint8_t cmd[3] = {
    owscWriteStatus  // command
  , 7, 0             // address TA1 & TA2
  }, crc[2];

  uint16_t crc16 = bus->crc16(cmd, sizeof(cmd));  // write send commands to crc
  crc16 = ~bus->crc16(data, 1, crc16);            // apply send data to crc

  if (!bus->reset())
    return false;

  bus->wireSelect(address);
  bus->wireWriteBytes(cmd, sizeof(cmd));
  bus->wireWriteBytes(data, 1);
  bus->wireReadBytes(crc, 2);

  if (((crc16 & 0xFF) == crc[0]) && ((crc16 >> 8) == crc[1])) {
    bus->wireWriteByte(0xFF);
    result = true;
  }
  bus->reset();

  return result;
}

bool Esp1wire::HelperSwitchDevice::channelAccessInfo(Bus *bus, uint8_t *address, SwitchChannelStatus *channelStatus, bool resetAlarm) {
  uint8_t cmd[3] = {
    owscChannelAccess       // command
  , ccbDefault, ccbReserved // channel control bytes
  }, crc[2], data[2];

  // init channel config byte
  cmd[1] |= ccImTogReadOne | ccCHSChA | ccCRCByte;

  if (resetAlarm)
    cmd[1] |= ccbfALR;
    
  if (!bus->reset())
    return false;

  bus->wireSelect(address);
  bus->wireWriteBytes(cmd, sizeof(cmd));
  bus->wireReadBytes(data, 2);  // channel info byte + dummy byte (otherwise no crc)
  bus->wireReadBytes(crc, 2);   // read crc
  bool result = bus->reset();

  // commands to crc
  uint16_t crc16 = bus->crc16(cmd, sizeof(cmd));
  // data to crc
  crc16 = ~bus->crc16(data, sizeof(data), crc16);

  result = result && ((crc16 & 0xFF) == crc[0]) && ((crc16 >> 8) == crc[1]);

  if (result) {
    channelStatus->parasite   = (data[0] & cibfNoChannels) == 0;
    channelStatus->noChannels = (uint8_t)(data[0] & cibfNoChannels ? 2 : 1);
    channelStatus->latchA     = (data[0] & cibfActivLatchA);
    channelStatus->senseA     = (data[0] & cibfSenseLevelA ? 1 : 0);
    channelStatus->flipFlopQA = (data[0] & cibfFlipFlopQA ? 1 : 0);
  
    if (channelStatus->noChannels == 2) {
      channelStatus->latchB     = (data[0] & cibfActivLatchB);
      channelStatus->senseB     = (data[0] & cibfSenseLevelB);
      channelStatus->flipFlopQB = (data[0] & cibfFlipFlopQB);
    }
  }
    
  return result;
}

// class HelperCounterDevice
bool Esp1wire::HelperCounterDevice::counter(Bus *bus, byte *address, uint32_t *counter, uint8_t noCounter) {
  uint8_t cmd[3] = {
    owccReadMemoryCounter   // command
  , 0xC0, 0x01              // address to first memory bank with counter
  }
  , data[32], cnt[4], zeros[4], crc[2];      // 32 byte memory + 4 byte counter + 4 byte zero bits + 2 byte crc

  // switch to last page
  if (noCounter == 2)
    cmd[1] |= 0x20;
    
  if (!bus->reset())
    return false;

  bus->wireSelect(address);
  bus->wireWriteBytes(cmd, sizeof(cmd));
  bus->wireReadBytes(data, sizeof(data));
  bus->wireReadBytes(cnt, sizeof(cnt));
  bus->wireReadBytes(zeros, sizeof(zeros));
  bus->wireReadBytes(crc, 2);   // read crc
  bool result = bus->reset();

  // commands to crc
  uint16_t crc16 = bus->crc16(cmd, sizeof(cmd));
  // data to crc
  crc16 = bus->crc16(data, sizeof(data), crc16);
  crc16 = bus->crc16(cnt, sizeof(cnt), crc16);
  crc16 = ~bus->crc16(zeros, sizeof(zeros), crc16);

  result = result && ((crc16 & 0xFF) == crc[0]) && ((crc16 >> 8) == crc[1]);

  if (result)
    *counter = (((uint32_t)cnt[3]) << 24) + (((uint32_t)cnt[2]) << 16) + (((uint32_t)cnt[1]) << 8) + (uint32_t)cnt[0];

  return result;
};

// class HelperBatteryDevice
bool Esp1wire::HelperBatteryDevice::readTemperatureC(Bus *bus, byte *address, float *temperature) {
  uint8_t data[8];

  if (!readScratch(bus, address, 0, data))
    return false;

  int16_t fpTemperature =
    (((int16_t) data[spp0fMSBT]) << 8) |
    (((int16_t) data[spp0fLSBT]));

  *temperature = (float)fpTemperature * 0.00390625;

  return true;
}

bool Esp1wire::HelperBatteryDevice::requestTemperatureC(Bus *bus, byte *address, float *temperature) {
  bool result = HelperTemperatureDevice::requestTemperatures(bus, address);

  if (result)
    result = readTemperatureC(bus, address, temperature);

  return result;
}

bool Esp1wire::HelperBatteryDevice::readBattery(Bus *bus, byte *address, float *voltage, float *current, float *capacity, float resistorSens) {
  uint8_t data[8];
  float rSens = (float)1 / resistorSens;
  
  // read offset and ica
  if (!readScratch(bus, address, 1, data))
    return false;

  int16_t fpOffset = 
    (((int16_t) data[spp1fMSBO]) << 11) |
    (((int16_t) data[spp1fLSBO]) << 3);

  *capacity = (float)data[spp1fICA] * 0.0004882 * rSens;
  float fOffset = (float)fpOffset * 0.0002441;

  // read voltage and current
  if (!readScratch(bus, address, 0, data))
    return false;

  int16_t fpVoltage =
    (((int16_t) data[spp0fMSBV]) << 8) |
    (((int16_t) data[spp0fLSBV]));

  *voltage = (float)fpVoltage * 0.01;

  int16_t fpCurrent =
    (((int16_t) data[spp0fMSBC]) << 8) |
    (((int16_t) data[spp0fLSBC]));

  *current = (float)fpCurrent * 0.0002441 * rSens; // fOffset ?
  
  return true;
}

bool Esp1wire::HelperBatteryDevice::requestBatteries(Bus *bus) {
  if (!bus->reset())
    return false;
  bus->wireWriteByte(owcSkip);

  // enable parasite
  if (bus->parasite())
    bus->setPowerSupply(true);

  bus->wireWriteByte(owbcStartConversionV);
  unsigned long now = millis(), delms = 750; // TODO delms
  while (!HelperTemperatureDevice::isConversionComplete(bus) && (millis() - delms < now))
    ;

  // disable parasite
  if (bus->parasite())
    bus->setPowerSupply(false);

  return true;
}

bool Esp1wire::HelperBatteryDevice::requestBattery(Bus *bus, byte *address, float *voltage, float *current, float *capacity, float resistorSens) {
  if (!bus->reset())
    return false;
  bus->wireSelect(address);

  // enable parasite
  if (bus->parasite())
    bus->setPowerSupply(true);

  bus->wireWriteByte(owbcStartConversionV);
  unsigned long now = millis(), delms = 750; // TODO delms depends on resolution (max yet)
  while (!HelperTemperatureDevice::isConversionComplete(bus) && (millis() - delms < now))
    ;

  // disable parasite
  if (bus->parasite())
    bus->setPowerSupply(false);

  return readBattery(bus, address, voltage, current, capacity, resistorSens);
}

bool Esp1wire::HelperBatteryDevice::readScratch(Bus *bus, byte *address, uint8_t page, uint8_t data[8]) {
  uint8_t cmd[2] = {
    owbcReadScratch
  , page
  }, crc[1];

  if (!bus->reset())
    return false;

  bus->wireSelect(address);
  bus->wireWriteBytes(cmd, sizeof(cmd));
  bus->wireReadBytes(data, sizeof(data));
  bus->wireReadBytes(crc, 1);   // read crc
  bool result = bus->reset();

  return (bus->crc8(data, sizeof(data)) == crc[0]);
}
        
// class DeviceFilter
Esp1wire::DeviceFilter::DeviceFilter(BusList* busList, DeviceType deviceType) {
  mCurrentBusList = busList;
  mDeviceType = deviceType;
}

bool Esp1wire::DeviceFilter::hasNext() {
  if (mCurrentBusList == NULL)
    return false;

  if (!mStarted) {                    // init list
    mStarted = true;
    initBus(false);

    if (deviceMatchFilter())          // point to matching device
      return true;
  }

  while (mCurrentBusList != NULL) {   // process bus list
    if (nextDevice()) {               // current bus has more devices
      if (deviceMatchFilter())        // point to matching device
        return true;
    } else if (nextBus()) {           // no more devices on current bus
      if (mCurrentDeviceList == NULL) // skip empty bus
        continue;
      if (deviceMatchFilter())        // device matches filter
        return true;
    } else {                          // cleanup internals
      mCurrentBusList = NULL;
      mCurrentDeviceList = NULL;
      break;
    }
  }

  return false;
}

bool Esp1wire::DeviceFilter::nextBus() {
  bool next = (mCurrentBusList != NULL && mCurrentBusList->next != NULL);

  if (next)
    initBus(next);

  return next;
}

void Esp1wire::DeviceFilter::initBus(bool next) {
  if (next)
    mCurrentBusList = mCurrentBusList->next;

  if (mCurrentBusList != NULL && mCurrentBusList->bus != NULL) {
    mCurrentDeviceList = mCurrentBusList->bus->getFirstDevice();

    //    Serial.println("DeviceFilter::initBus: " + mCurrentBusList->bus->getBusInformation());
  }
}

bool Esp1wire::DeviceFilter::deviceMatchFilter() {
  return (mDeviceType == DeviceTypeAll || (mCurrentDeviceList->device->getDeviceType() & mDeviceType) == mDeviceType);
}

bool Esp1wire::DeviceFilter::nextDevice() {
  bool next = (mCurrentDeviceList != NULL && mCurrentDeviceList->next != NULL);

  if (next)
    mCurrentDeviceList = mCurrentDeviceList->next;

  return next;
}

Esp1wire::Device* Esp1wire::DeviceFilter::getNextDevice() {
  return mCurrentDeviceList->device;
}

// class TemperatureDeviceFilter
bool Esp1wire::TemperatureDeviceFilter::hasNext() {
  return DeviceFilter::hasNext();
}

Esp1wire::TemperatureDevice* Esp1wire::TemperatureDeviceFilter::getNextDevice() {
  return (Esp1wire::TemperatureDevice*)DeviceFilter::getNextDevice();
}

// class AlarmFilter
Esp1wire::AlarmFilter::~AlarmFilter() {
  while (alarmList != NULL) {
    currList = alarmList->next;
    delete alarmList;
    alarmList = currList;
  }

  alarmList = currList = lastList = NULL;
};

bool Esp1wire::AlarmFilter::hasNext() {
  if (!mStarted) {             // first call
    mStarted = true;
    currList = alarmList;

    return (currList != NULL);
  }

  if (currList->next == NULL)
    return false;

  currList = currList->next;  // select next

  return true;
}

Esp1wire::Device* Esp1wire::AlarmFilter::getNextDevice() {
  if (currList == NULL)
    return NULL;

  return currList->device;
}

// class HelperAlarmFilter
void Esp1wire::HelperAlarmFilter::addDevice(Device *device) {
  if (device->getIgnoreAlarmSearch())
    return;

  if (alarmList == NULL) {
    alarmList = lastList = new Bus::DeviceList();
    alarmList->device = device;
    alarmList->next = NULL;
  } else {
    Bus::DeviceList *newList = new Bus::DeviceList();
    newList->device = device;
    newList->next = NULL;

    lastList->next = newList;
    lastList = newList;
  }
}

