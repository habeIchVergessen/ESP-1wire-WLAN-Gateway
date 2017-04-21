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

    enum BusmasterType : byte {
        DS2482_100            = 1
      , DS2482_800            = 2
    };

    enum DeviceType : byte {
        DeviceTypeUnsupported = 0x01
      , DeviceTypeTemperature = 0x02
      , DeviceTypeSwitch      = 0x04
      , DeviceTypeCounter     = 0x08
      , DeviceTypeBattery     = 0x12  // incl. temperature sensor too
      , DeviceTypeAll         = 0xFF
    };

    enum OneWireDeviceType : byte {
        DS1990                = 0x01  // also DS2401
      , DS18S20               = 0x10  // also DS1820
      , DS2406                = 0x12  // also DS2407
      , DS2423                = 0x1D
      , DS1822                = 0x22
      , DS2438                = 0x26
      , DS18B20               = 0x28
      , DS2408                = 0x29
      , DS1825                = 0x3B
      , DS28EA00              = 0x42
    };

    enum OneWireCommands : byte {
        owcSkip               = 0xCC  // broadcast
      , owcNormalSearch       = 0xF0  // Query bus for all devices
      , owcAlarmSearch        = 0xEC  // Query bus for devices with an alarm condition
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
    enum statusBits : byte {
        statusParasiteRead  = 0x01
      , statusParasiteOn    = 0x02
      , statusResolution    = 0x0C  // 2 bits
      , statusAlarm         = 0x10
      , statusBattVdd       = 0x20  // read VDD too
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
        bool            wireSearch(uint8_t *address, bool alarm=false);
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
        virtual bool    alarmSearchIntern(DeviceType targetSearch);
        virtual void    wireResetSearch();
        virtual void    wireSelect(uint8_t *address);
        virtual void    wireWriteByte(uint8_t b);
        virtual uint8_t wireReadBit();
        virtual void    wireReadBytes(uint8_t *data, uint16_t len);
        virtual void    setPowerSupply(bool power);

      protected:
        Device          *deviceDetected(uint8_t *address);
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
        bool alarmSearchIntern(DeviceType targetSearch) override;
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
        bool alarmSearchIntern(DeviceType targetSearch) override;
        void wireSelect(uint8_t *address) override;
        void wireWriteByte(uint8_t b) override;
        uint8_t wireReadBit() override;
        void wireReadBytes(uint8_t *data, uint16_t len) override;
        void setPowerSupply(bool power) override;
        void search();
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
        enum OneWireTemperatureCommands : byte {
          owtcStartConversion   = 0x44  // Tells device to take a temperature reading and put it on the scratchpad
        , owtcCopyScratch       = 0x48  // Copy EEPROM
        , owtcWriteScratch      = 0x4E  // Write to EEPROM
        , owtcReadPowerSupply   = 0xB4  // Determine if device needs parasite power
        , owtcRecallScratch     = 0xB8  // Reload from last known
        , owtcReadScratch       = 0xBE  // Read EEPROM
        };

        enum OneWireTemperatureResolutions : byte {
          owtrResolution9bit    = 0x1F
        , owtrResolution10bit   = 0x3F
        , owtrResolution11bit   = 0x5F
        , owtrResolution12bit   = 0x7F
        };

        enum OneWireSwitchCommands : byte {
        // DS2406
          owscWriteStatus       = 0x55  // write status memory (8 bytes + crc16)
        , owscReadStatus        = 0xAA  // read status memory (8 bytes + crc16)
        , owscChannelAccess     = 0xF5  // read/write channel access byte/config
        // DS2408
        , owscChannelAccessWrite= 0x5A  // channel access write
        , owscWriteCondSearch   = 0xCC  // write cond. search
        , owscResetActLatches   = 0xC3  // reset Activity Latches
        , owscReadPioRegisters  = 0xF0  // read PIO reagisters
        };

        enum OneWireCounterCommands : byte {
          owccReadMemoryCounter = 0xA5  // read status memory (8 bytes + crc16)
        };

        enum OneWireBatteryCommands : byte {
          owbcStartConversionT  = 0x44  // Tells device to take a temperature reading and put it on the scratchpad
        , owbcWriteScratch      = 0x4E  // Write EEPROM
        , owbcStartConversionV  = 0xB4  // Tells device to take a voltage reading and put it on the scratchpad
        , owbcRecallMemory      = 0xB8  // load to scratch
        , owbcReadScratch       = 0xBE  // Read EEPROM
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
        enum TemperatureResolution : byte {
            resolutionUnknown = 0xFF
          , resolution9bit    = 0x00
          , resolution10bit   = 0x04
          , resolution11bit   = 0x08
          , resolution12bit   = 0x0C
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
        enum ScratchPadFields : byte {
          spfLSB          = 0
        , spfMSB          = 1
        , spfHighAlarm    = 2
        , spfLowAlarm     = 3
        , spfConfig       = 4
        , spfInternal     = 5
        , spfCntRemain    = 6
        , spfCntPerC      = 7
        , spfCRC          = 8
        };

        bool                    readScratch(uint8_t data[9]);
        bool                    writeScratch(uint8_t data[9]);
        bool                    readPowerSupply();
    };

    // class SwitchDevice
    class SwitchDevice : public Device {
    public:
      enum ConditionalSearchPolarity : byte {
        ConditionalSearchPolarityLow  = 0x00
      , ConditionalSearchPolarityHigh = 0x01
      };
      
      enum ConditionalSearchSourceSelect : byte {
        SourceSelectActivityLatch   = 0x02
      , SourceSelectChannelFlipFlop = 0x04
      , SourceSelectPIOStatus       = 0x06
      };

      enum ConditionalSearchChannelSelect : byte {
        ChannelSelectDisabled = 0x00
      , ChannelSelectA        = 0x08
      , ChannelSelectB        = 0x10
      , ChannelSelectBoth     = 0x18
      };

      enum ChannelFlipFlop : byte {
        ChannelFlipFlopA            = 0x20
      , ChannelFlipFlopB            = 0x40
      , ChannelFlipFlopBoth         = 0x60
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

      // DS2408
      bool readChannelAccess(uint8_t data[1]);
      bool writeChannelAccess(uint8_t data[1]);
      bool setConditionalSearch(uint8_t data[3]);
      
      void readConfig();

    protected:
      // Status locations
      enum StatusMemoryFields : byte {
        smfStatus       = 7
      , smfCRC0         = 8
      , smfCRC1         = 9
      };

      enum StatusMemoryByteFields : byte {
        smbfPolarity    = 0x01
      , smbfSrcSelA     = 0x02
      , smbfSrcSelB     = 0x04
      , smbfChSelPioA   = 0x08
      , smbfChSelPioB   = 0x10
      , smbfPioA        = 0x20
      , smbfPioB        = 0x40
      , smbfPowerSupply = 0x80
      };

      // Channel Configuration Byte
      enum ChannelConfigByteFields : byte {
      // ccbfCRC[0|1] see ChannelConfigCRC (2 bits)
      // ccbfCHS[0|1] see ChannelConfigCHS  (2 bits)
      // ccbfIC ccbfTOG ccbfIM see ChannelConfigImTog (3 bits)
        ccbfALR         = 0x80
      };

      enum ChannelInfoByteFields : byte {
        cibfFlipFlopQA  = 0x01
      , cibfFlipFlopQB  = 0x02
      , cibfSenseLevelA = 0x04
      , cibfSenseLevelB = 0x08
      , cibfActivLatchA = 0x10
      , cibfActivLatchB = 0x20
      , cibfNoChannels  = 0x40
      , cibfPowerSupply = 0x80
      };

      enum ChannelConfigCHS : byte {
        ccCHSChA        = 0x04
      , ccCHSChB        = 0x08
      , ccCHSChBoth     = 0x0C
      };

      enum ChannelConfigCRC : byte {
        ccCRCNone       = 0x00
      , ccCRCByte       = 0x01
      , ccCRC8Bytes     = 0x02
      , ccCRC32Bytes    = 0x04
      };

      enum ChannelConfigImTog : byte {
        ccImTogReadOne  = 0x40
      , ccImTogReadBoth = 0x41
      , ccImTogWriteOne = 0x00
      };

      enum ChannelConfigByte : byte {
        ccbDefault      = 0x00
      , ccbReserved     = 0xFF
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
      bool                    getRequestVdd() { return (mStatus & statusBattVdd); };
      void                    setRequestVdd(bool vdd) { if (vdd) mStatus |= statusBattVdd; else mStatus &= ~statusBattVdd; };

      void                    readConfig();

    protected:
      enum      InputSelect : byte {
        InputSelectVDD        = 0x08
      , InputSelectVAD        = 0x00
      };
      
      enum ScratchPadPage0Fields : byte {
        spp0fStatusConfig     = 0
      , spp0fLSBT             = 1
      , spp0fMSBT             = 2
      , spp0fLSBV             = 3
      , spp0fMSBV             = 4
      , spp0fLSBC             = 5
      , spp0fMSBC             = 6
      , spp0fThreshold        = 7
      };

      enum ScratchPadPage1Fields : byte {
        spp1fEtm0             = 0
      , spp1fEtm1             = 1
      , spp1fEtm2             = 2
      , spp1fEtm3             = 3
      , spp1fICA              = 4
      , spp1fLSBO             = 5
      , spp1fMSBO             = 6
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

    // class Scheduler
    class Scheduler {
    public:
      enum ScheduleAction : byte {
        scheduleRequestTemperatues  = 0x00
      , scheduleRequestBatteries    = 0x01
      , scheduleReadCounter         = 0x02
      , scheduleAlarmSearch         = 0x03
      , scheduleResetSearch         = 0x04
      };

      typedef void (*SchedulerCallback) (DeviceType filter);

      ~Scheduler();
      void      registerCallback(ScheduleAction action, SchedulerCallback callback);
      void      addSchedule(uint16_t interval, ScheduleAction action, DeviceType filter=DeviceTypeAll);
      void      runSchedules();
      void      loadSchedules();
      void      saveSchedules();
      uint8_t   getSchedulesCount() { return mSchedulesCount; };
      bool      getSchedule(uint8_t idx, uint16_t *interval, ScheduleAction *action, DeviceType *filter);
      void      updateSchedule(uint8_t idx, uint16_t interval, ScheduleAction action, DeviceType filter=DeviceTypeAll);
      void      removeSchedule(uint8_t idx);
      
    protected:
      typedef struct __attribute__((packed)) ScheduleList
      {
        uint32_t        lastExecution = 0;
        uint32_t        interval;
        ScheduleAction  action;
        DeviceType      filter;
        ScheduleList    *next;
      };

      ScheduleList      *first = NULL, *last = NULL;
      SchedulerCallback schedulerCallbacks[5] = { NULL, NULL, NULL, NULL, NULL };
      uint8_t           mSchedulesCount = 0;
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
      static bool writeStatus(Bus *bus, uint8_t *address, uint8_t data[3]);
      static bool channelAccessInfo(Bus *bus, uint8_t *address, SwitchChannelStatus *channelStatus, bool resetAlarm=false);
      static bool readChannelAccess(Bus *bus, uint8_t *address, uint8_t data[1]);
      static bool writeChannelAccess(Bus *bus, uint8_t *address, uint8_t data[1]);
      static bool setConditionalSearch(Bus *bus, uint8_t *address, uint8_t data[3]);
    protected:
      static bool readStatusDS2406(Bus *bus, uint8_t *address, SwitchMemoryStatus *memoryStatus);
      static bool writeStatusDS2406(Bus *bus, uint8_t *address, uint8_t data[1]);
      static bool writeStatusDS2408(Bus *bus, uint8_t *address, uint8_t data[3]);
      static bool channelAccessInfoDS2406(Bus *bus, uint8_t *address, SwitchChannelStatus *channelStatus, bool resetAlarm=false);
      static bool channelAccessInfoDS2408(Bus *bus, uint8_t *address, SwitchChannelStatus *channelStatus, bool resetAlarm=false);
      static bool readChannelAccessDS2408(Bus *bus, uint8_t *address, uint8_t data[1]);
      static bool writeChannelAccessDS2408(Bus *bus, uint8_t *address, uint8_t data[1]);
      static bool resetActivityLatchesDS2408(Bus *bus, uint8_t *address);
      static bool setConditionalSearchDS2408(Bus *bus, uint8_t *address, uint8_t data[3]);
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
