#ifndef Esp1wire_h
#define Esp1wire_h

#ifdef _DEBUG_TIMING
  #define elapTime(start) (String)"(" + (String)((micros() - start) / (float)1000.0) + " ms)"
#else
  #define elapTime(start) (String)""
#endif

#include "Arduino.h"

#include "Wire.h"
#include "OneWire.h"

#include "EspConfig.h"

#include "DS2482.h"
class Esp1wire {
  private:
    uint8_t     mSDA, mSCL;

  public:
    // class prototypes
    class Device;
    class TemperatureDevice;
    class SwitchDevice;
    class DeviceFilter;
    class TemperatureDeviceFilter;
    class AlarmFilter;

    enum BusmasterType {
        DS2482_100            = (byte)1
      , DS2482_800            = (byte)2
    };

    enum DeviceType {
        DeviceTypeUnsupported = (byte)0x01
      , DeviceTypeTemperature = (byte)0x02
      , DeviceTypeSwitch      = (byte)0x04
      , DeviceTypeCounter     = (byte)0x08
      , DeviceTypeBattery     = (byte)0x12  // incl. temperature sensor too
      , DeviceTypeAll         = (byte)0xFF
    };

    enum OneWireDeviceType {
        DS18S20               = (byte)0x10  // also DS1820
      , DS2406                = (byte)0x12  // also DS2407
      , DS2423                = (byte)0x1D
      , DS1822                = (byte)0x22
      , DS2438                = (byte)0x26
      , DS18B20               = (byte)0x28
      , DS1825                = (byte)0x3B
      , DS28EA00              = (byte)0x42
    };

    enum OneWireCommands {
        owcSkip               = (byte)0xCC  // broadcast
      , owcNormalSearch       = (byte)0xF0  // Query bus for all devices
      , owcAlarmSearch        = (byte)0xEC  // Query bus for devices with an alarm condition
    };

    Esp1wire();
    bool          resetSearch();
    AlarmFilter   alarmSearch(DeviceType targetSearch=DeviceTypeAll);
    bool          requestTemperatures(bool resetIgnoreAlarmFlags = false);
    bool          probeI2C(uint8_t sda = SDA, uint8_t scl = SCL);
    bool          probeGPIO(uint8_t gpio = 0);

    uint8_t                 getBusCount() { return mBusListCount; };
    DeviceFilter            getDeviceFilter(DeviceType filter=DeviceTypeAll) { return DeviceFilter(firstBus, filter); };
    TemperatureDeviceFilter getTemperatureDeviceFilter() { return TemperatureDeviceFilter(firstBus); }

#ifdef _DEBUG_TEST_DATA
    void          testData();
#endif

  protected:
    enum statusBits {
        statusParasiteRead  = (byte)0x01
      , statusParasiteOn    = (byte)0x02
      , statusResolution    = (byte)0x0C  // 2 bits
      , statusAlarm         = (byte)0x10
    };

    // class Busmaster
    class Busmaster {
      public:
        Busmaster(DS2482 *ds2482, byte i2cPort, BusmasterType busmasterType);
        String          getName();
        BusmasterType   getType();
        bool            busAddressInUse(uint8_t busAddress) { return (mI2CPort == busAddress); };

        void            target_search(OneWireDeviceType oneWireDeviceType);
        bool            selectChannel(uint8_t channel);
        bool            wireReset();
        void            wireSelect(uint8_t *address);
        void            wireWriteByte(uint8_t b);
        uint8_t         wireReadBit();
        void            wireReadBytes(uint8_t *data, uint16_t len);

        void            wireResetSearch();
        bool            wireSearch(uint8_t *address, bool alarm=false, DeviceType targetSearch=DeviceTypeAll);
        void            wireStrongPullup(bool pullup);

      private:
        DS2482          *mDS2482;
        BusmasterType   mBusmasterType;
        byte            mI2CPort;
        byte            mSelectedChannel = 0;

        // search crap
        uint8_t         mTimeout;
        uint8_t         searchExhausted = 0;
        uint8_t         searchLastDiscrepancy = 0;
        uint8_t         searchAddress[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
        uint8_t         busyWait(bool setReadPtr=false);
    };

    // class Bus
    class Bus
    {
      public:
        typedef struct __attribute__((packed)) DeviceList
        {
          Device      *device;
          DeviceList  *next;
        };

        void            registerTemperatureDevice(bool parasite, uint8_t resolution);
        bool            parasite() { return (mStatus & statusParasiteRead); };
        bool            powerSupply() { return (mStatus & statusParasiteOn); };
        uint16_t        getDeviceCount() { return mDeviceListCount; };
        uint16_t        getTemperatureDeviceCount() { return mTemperatureDeviceCount; };
        void            resetIgnoreAlarmFlags(DeviceType deviceType = DeviceTypeAll);
        uint8_t         crc8(uint8_t *address, uint8_t len);
        uint16_t        crc16(uint8_t *address, uint8_t len, uint16_t crc = 0);
        void            wireWriteBytes(uint8_t *bytes, uint8_t len);
        
        virtual DeviceList *getFirstDevice();
        virtual String    getBusInformation();
        virtual bool      busAddressInUse(uint8_t busAddress);

        // forwarded functions
        virtual bool    reset();
        virtual bool    resetSearch();
        virtual bool    alarmSearch(DeviceType targetSearch=DeviceTypeAll);
        virtual void    wireResetSearch();
        virtual void    wireSelect(uint8_t *address);
        virtual void    wireWriteByte(uint8_t b);
        virtual uint8_t wireReadBit();
        virtual void    wireReadBytes(uint8_t *data, uint16_t len);
        virtual void    setPowerSupply(bool power);

      protected:
        void            deviceDetected(uint8_t *address);
        int8_t          addressCompare(uint8_t *addr1, uint8_t *addr2);
        DeviceType      getDeviceType(uint8_t *address);
        void            alarmSearchHandleFound(uint8_t *address);

        DeviceList      *firstDevice = NULL, *lastDevice = NULL, *currDevice = NULL;
        uint16_t        mDeviceListCount = 0;
        uint16_t        mTemperatureDeviceCount = 0;
        uint8_t         mStatus;
    };

    // class BusIC
    class BusIC : public Bus
    {
      private:
        Busmaster       *mBusmaster;
        uint8_t         mChannel = 0;

      public:
        BusIC(Busmaster *busmaster);
        BusIC(Busmaster *busmaster, uint8_t channel);

        DeviceList *getFirstDevice() override;
        String getBusInformation() override;
        bool busAddressInUse(uint8_t busAddress) override { return mBusmaster->busAddressInUse(busAddress); };

        // forwarded functions
        bool reset() override;
        bool resetSearch() override;
        bool alarmSearch(DeviceType targetSearch) override;
        bool selectChannel();
        void wireResetSearch() override;
        void wireSelect(uint8_t *address) override;
        void wireWriteByte(uint8_t b) override;
        uint8_t wireReadBit() override;
        void wireReadBytes(uint8_t *data, uint16_t len) override;
        void setPowerSupply(bool power) override;
    };

    // class BusGPIO
    class BusGPIO : public Bus
    {
      private:
        OneWire         *mOneWire;
        uint8_t         mGPIOPort;
      public:
        BusGPIO(OneWire *oneWire, uint8_t gpioPort);

        DeviceList *getFirstDevice() override;
        String getBusInformation() override;
        bool busAddressInUse(uint8_t busAddress) override { return (mGPIOPort == busAddress); };

        // forwarded functions
        bool reset() override;
        bool resetSearch() override;
        void wireResetSearch() override;
        bool alarmSearch(DeviceType targetSearch) override;
        void wireSelect(uint8_t *address) override;
        void wireWriteByte(uint8_t b) override;
        uint8_t wireReadBit() override;
        void wireReadBytes(uint8_t *data, uint16_t len) override;
        void setPowerSupply(bool power) override;
    };

  protected:
    typedef struct __attribute__((packed)) BusList
    {
      Bus     *bus;
      BusList *next;
    };

  public:
    // class Device
    class Device
    {
      public:
        Device(Bus *bus, uint8_t *address, DeviceType deviceType);
        String        getName();
        DeviceType    getDeviceType() { return mDeviceType; };
        String        getOneWireDeviceID();
        uint8_t       getOneWireDeviceType() { return mAddress[0]; };
        bool          getIgnoreAlarmSearch() { return (mStatus & statusAlarm); };
        void          setIgnoreAlarmSearch(bool ignore) { if (ignore) mStatus |= statusAlarm; else mStatus &= ~statusAlarm; };

      protected:
        enum OneWireTemperatureCommands {
          owtcStartConversion   = (byte)0x44  // Tells device to take a temperature reading and put it on the scratchpad
        , owtcCopyScratch       = (byte)0x48  // Copy EEPROM
        , owtcWriteScratch      = (byte)0x4E  // Write to EEPROM
        , owtcReadPowerSupply   = (byte)0xB4  // Determine if device needs parasite power
        , owtcRecallScratch     = (byte)0xB8  // Reload from last known
        , owtcReadScratch       = (byte)0xBE  // Read EEPROM
        };

        enum OneWireTemperatureResolutions {
          owtrResolution9bit    = (byte)0x1F
        , owtrResolution10bit   = (byte)0x3F
        , owtrResolution11bit   = (byte)0x5F
        , owtrResolution12bit   = (byte)0x7F
        };

        enum OneWireSwitchCommands {
          owscWriteStatus       = (byte)0x55  // write status memory (8 bytes + crc16)
        , owscReadStatus        = (byte)0xAA  // read status memory (8 bytes + crc16)
        , owscChannelAccess     = (byte)0xF5  // read/write channel access byte/config
        };

        enum OneWireCounterCommands {
          owccReadMemoryCounter = (byte)0xA5  // read status memory (8 bytes + crc16)
        };

        enum OneWireBatteryCommands {
          owbcStartConversionT  = (byte)0x44  // Tells device to take a temperature reading and put it on the scratchpad
        , owbcWriteScratch      = (byte)0x4E  // Write EEPROM
        , owbcStartConversionV  = (byte)0xB4  // Tells device to take a voltage reading and put it on the scratchpad
        , owbcRecallMemory      = (byte)0xB8  // load to scratch
        , owbcReadScratch       = (byte)0xBE  // Read EEPROM
        };

        uint8_t     mAddress[8];
        uint8_t     mStatus = 0;

        Bus         *mBus;
        DeviceType  mDeviceType;

        bool        parasite() { return ((mDeviceType == DeviceTypeTemperature) && (mStatus & statusParasiteOn)); };
    };

    // class TemperatureDevice
    class TemperatureDevice : public Device
    {
      public:
        enum TemperatureResolution {
            resolutionUnknown = (byte)0xFF
          , resolution9bit    = (byte)0x00
          , resolution10bit   = (byte)0x04
          , resolution11bit   = (byte)0x08
          , resolution12bit   = (byte)0x0C
        };

        TemperatureDevice();
        bool                    getAlarmTemperatures(int8_t *lowTemperature, int8_t *highTemperature);
        bool                    setAlarmTemperatures(int8_t lowTemperature, int8_t highTemperature);
        bool                    readTemperatureC(float *temperature);
        bool                    requestTemperatureC(float *temperature);
        bool                    powerSupply() { return parasite(); };
        TemperatureResolution   readResolution();

        void  readConfig();

      protected:
        // Scratchpad locations
        enum ScratchPadFields {
          spfLSB          = (byte)0
        , spfMSB          = (byte)1
        , spfHighAlarm    = (byte)2
        , spfLowAlarm     = (byte)3
        , spfConfig       = (byte)4
        , spfInternal     = (byte)5
        , spfCntRemain    = (byte)6
        , spfCntPerC      = (byte)7
        , spfCRC          = (byte)8
        };

        bool                    readScratch(uint8_t data[9]);
        bool                    writeScratch(uint8_t data[9]);
        bool                    readPowerSupply();
    };

    // class SwitchDevice
    class SwitchDevice : public Device {
    public:
      enum ConditionalSearchPolarity {
        ConditionalSearchPolarityLow  = (byte)0x00
      , ConditionalSearchPolarityHigh = (byte)0x01
      };
      
      enum ConditionalSearchSourceSelect {
        SourceSelectActivityLatch   = (byte)0x02
      , SourceSelectChannelFlipFlop = (byte)0x04
      , SourceSelectPIOStatus       = (byte)0x06
      };

      enum ConditionalSearchChannelSelect {
        ChannelSelectDisabled = (byte)0x00
      , ChannelSelectA        = (byte)0x08
      , ChannelSelectB        = (byte)0x10
      , ChannelSelectBoth     = (byte)0x18
      };

      enum ChannelFlipFlop {
        ChannelFlipFlopA            = (byte)0x20
      , ChannelFlipFlopB            = (byte)0x40
      , ChannelFlipFlopBoth         = (byte)0x60
      };

      typedef struct __attribute__((packed)) SwitchChannelStatus
      {
        uint8_t noChannels;
        bool parasite;
        bool latchA, senseA, flipFlopQA; 
        bool latchB, senseB, flipFlopQB;
      };

      typedef struct __attribute__((packed)) SwitchMemoryStatus
      {
        ConditionalSearchPolarity       csPolarity;
        ConditionalSearchSourceSelect   csSourceSelect;
        ConditionalSearchChannelSelect  csChannelSelect; 
        ChannelFlipFlop                 channelFlipFlop;
        bool                            parasite;
      };

      SwitchDevice();
      bool getChannelInfo(SwitchChannelStatus *channelStatus);
      bool getMemoryStatus(SwitchMemoryStatus *memoryStatus);
      bool setConditionalSearch(ConditionalSearchPolarity csPolarity, ConditionalSearchSourceSelect csSourceSelect, ConditionalSearchChannelSelect csChannelSelect, ChannelFlipFlop channelFlipFlop);
      bool resetAlarm(SwitchChannelStatus *channelStatus);

      void readConfig();

    protected:
      // Status locations
      enum StatusMemoryFields {
        smfStatus       = (byte)7
      , smfCRC0         = (byte)8
      , smfCRC1         = (byte)9
      };

      enum StatusMemoryByteFields {
        smbfPolarity    = (byte)0x01
      , smbfSrcSelA     = (byte)0x02
      , smbfSrcSelB     = (byte)0x04
      , smbfChSelPioA   = (byte)0x08
      , smbfChSelPioB   = (byte)0x10
      , smbfPioA        = (byte)0x20
      , smbfPioB        = (byte)0x40
      , smbfPowerSupply = (byte)0x80
      };

      // Channel Configuration Byte
      enum ChannelConfigByteFields {
      // ccbfCRC[0|1] see ChannelConfigCRC (2 bits)
      // ccbfCHS[0|1] see ChannelConfigCHS  (2 bits)
      // ccbfIC ccbfTOG ccbfIM see ChannelConfigImTog (3 bits)
        ccbfALR         = (byte)0x80
      };

      enum ChannelInfoByteFields {
        cibfFlipFlopQA  = (byte)0x01
      , cibfFlipFlopQB  = (byte)0x02
      , cibfSenseLevelA = (byte)0x04
      , cibfSenseLevelB = (byte)0x08
      , cibfActivLatchA = (byte)0x10
      , cibfActivLatchB = (byte)0x20
      , cibfNoChannels  = (byte)0x40
      , cibfPowerSupply = (byte)0x80
      };

      enum ChannelConfigCHS {
        ccCHSChA        = (byte)0x04
      , ccCHSChB        = (byte)0x08
      , ccCHSChBoth     = (byte)0x0C
      };

      enum ChannelConfigCRC {
        ccCRCNone       = (byte)0x00
      , ccCRCByte       = (byte)0x01
      , ccCRC8Bytes     = (byte)0x02
      , ccCRC32Bytes    = (byte)0x04
      };

      enum ChannelConfigImTog {
        ccImTogReadOne  = (byte)0x40
      , ccImTogReadBoth = (byte)0x41
      , ccImTogWriteOne = (byte)0x00
      };

      enum ChannelConfigByte {
        ccbDefault      = (byte)0x00
      , ccbReserved     = (byte)0xFF
      };

      bool              readStatus(SwitchMemoryStatus *memoryStatus);
      bool              writeStatus(uint8_t data[1]);
      bool              channelAccessInfo(SwitchChannelStatus *channelStatus, bool resetAlarm=false);
    };

    // class CounterDevice
    class CounterDevice : public Device {
    public:
      CounterDevice();

      bool            getCounter(uint32_t *counter1, uint32_t *counter2);
    protected:
      bool            counter(uint32_t *counter1, uint32_t *counter2);
    };
    
    // class BatteryDevice
    class BatteryDevice : public Device
    {
    public:
      bool                    readTemperatureC(float *temperature);
      bool                    requestTemperatureC(float *temperature);
      bool                    requestVDD(float *voltage, float *current, float *capacity, float resistorSens=0.025);
      bool                    requestVAD(float *voltage, float *current, float *capacity, float resistorSens=0.025);

    protected:
      enum      InputSelect {
        InputSelectVDD        = (byte)0x08
      , InputSelectVAD        = (byte)0x00
      };
      
      enum ScratchPadPage0Fields {
        spp0fStatusConfig     = (byte)0
      , spp0fLSBT             = (byte)1
      , spp0fMSBT             = (byte)2
      , spp0fLSBV             = (byte)3
      , spp0fMSBV             = (byte)4
      , spp0fLSBC             = (byte)5
      , spp0fMSBC             = (byte)6
      , spp0fThreshold        = (byte)7
      };

      enum ScratchPadPage1Fields {
        spp1fEtm0             = (byte)0
      , spp1fEtm1             = (byte)1
      , spp1fEtm2             = (byte)2
      , spp1fEtm3             = (byte)3
      , spp1fICA              = (byte)4
      , spp1fLSBO             = (byte)5
      , spp1fMSBO             = (byte)6
      };

      bool requestBattery(InputSelect inputSelect, float *voltage, float *current, float *capacity, float resistorSens=0.025);
    };
    
    // class DeviceFilter
    class DeviceFilter {
      public:
        DeviceFilter(BusList *buslist, DeviceType deviceType);
        bool            hasNext();
        Device          *getNextDevice();

      private:
        bool            mStarted = false;
        BusList         *mCurrentBusList;
        Bus::DeviceList *mCurrentDeviceList;
        DeviceType      mDeviceType;

        // list helper
        bool            nextBus();
        void            initBus(bool next);
        bool            deviceMatchFilter();
        bool            nextDevice();
    };

    // class TemperatureDeviceFilter
    class TemperatureDeviceFilter : protected DeviceFilter {
      public:
        TemperatureDeviceFilter(BusList *buslist) : DeviceFilter(buslist, DeviceTypeTemperature) {};
        bool              hasNext();
        TemperatureDevice *getNextDevice();
    };

    // class AlarmFilter
    class AlarmFilter {
      public:
        AlarmFilter(Bus::DeviceList *first) { firstList = first; };
        bool              hasNext();
        Device            *getNextDevice();
      protected:
        Bus::DeviceList   *firstList, *currList;
        bool              mStarted = false;
    };

  protected:
    // data and managent function AlarmFilter
    Bus::DeviceList   *alarmFirst, *alarmLast;
    void addAlarmFilter(Device *device);
    
    // class HelperDevice
    class HelperDevice : public Device {
    public:
      int8_t            compareAddress(uint8_t *address);

      static int8_t     compareAddress(uint8_t *addr1, uint8_t *addr2);
      static String     getOneWireDeviceID(uint8_t *address);
      static bool       isConversionComplete(Bus *bus);
    };
    
    // class HelperTemperatureDevice
    class HelperTemperatureDevice : public TemperatureDevice
    {
      public:
        static bool requestTemperatures(Bus *bus);
        static bool requestTemperature(Bus *bus, byte *address);
        static bool readScratch(Bus *bus, uint8_t *address, uint8_t data[9]);
        static bool writeScratch(Bus *bus, uint8_t *address, uint8_t data[9]);
        static TemperatureResolution resolution(uint8_t *address, uint8_t data[9]);
        static bool powerSupply(Bus *bus, uint8_t *address);
    };

    // HelperSwitchDevice
    class HelperSwitchDevice : public SwitchDevice {
    public:
      static bool readStatus(Bus *bus, uint8_t *address, SwitchMemoryStatus *memoryStatus);
      static bool writeStatus(Bus *bus, uint8_t *address, uint8_t data[1]);
      static bool channelAccessInfo(Bus *bus, uint8_t *address, SwitchChannelStatus *channelStatus, bool resetAlarm=false);
    };

    // HelperCounterDevice 
    class HelperCounterDevice : public CounterDevice {
    public:
      static bool counter(Bus *bus, byte *address, uint32_t *counter, uint8_t noCounter);
    };
    
    // class HelperBatteryDevice
    class HelperBatteryDevice : public BatteryDevice
    {
    public:
      static bool readTemperatureC(Bus *bus, byte *address, float *temperature);
      static bool requestTemperatureC(Bus *bus, byte *address, float *temperature);
      static bool requestBattery(Bus *bus, byte *address, InputSelect inputSelect, float *voltage, float *current, float *capacity, float resistorSens=0.025);
    protected:
      static bool readBattery(Bus *bus, byte *address, float *voltage, float *current, float *capacity, float resistorSens);
      static bool readScratch(Bus *bus, byte *address, uint8_t page, uint8_t data[8]);
    };

  private:

    BusList         *firstBus = NULL, *lastBus = NULL;
    uint8_t         mBusListCount = 0;

    bool            addBusmaster(DS2482 *ds2482, byte i2cPort, BusmasterType busmasterType);
    bool            addGPIO(OneWire *oneWire, byte gpioPort);
    bool            addBus(Bus *bus);
    void            freeAlarmFilter();
    bool            busAddressInUse(uint8_t busAddress);
};

extern Esp1wire esp1wire;

#endif  // Esp1wire_h
