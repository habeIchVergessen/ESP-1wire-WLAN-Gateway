#ifndef _ESPDEBUG_H
#define _ESPDEBUG_H

#include <Arduino.h>

#ifdef ESP8266
  #include <FS.h>

  #define DBG_PRINTER espDebug
#else
  #define DBG_PRINTER Serial
#endif

#define DBG_PRINT(...) { DBG_PRINTER.print(__VA_ARGS__); }
#define DBG_PRINTF(...) { DBG_PRINTER.printf(__VA_ARGS__); }
#define DBG_PRINTLN(...) { DBG_PRINTER.println(__VA_ARGS__); }
#define DBG_WRITE(...) { DBG_PRINTER.write(__VA_ARGS__); }

#if DBG_PRINTER==espDebug
  #define DBG_FORCE_OUTPUT() { DBG_PRINTER.sendWriteBuffer(); }
#else
  #define DBG_FORCE_OUTPUT() { }
#endif

#ifdef ESP8266
  typedef void (*HandleInputCallback) (Stream *input);

  #include <WiFiServer.h>
  #include <WiFiClient.h>
  #include <ESP8266WiFi.h>
#endif

class EspDebug : public Stream {
  public:  
    EspDebug();
    ~EspDebug();

    void begin(uint16_t dbgServerPort=9001);
    void loop();
    void sendWriteBuffer();
    void bufferedWrite(boolean enable=true);
    void enableSerialOutput(bool enable=true) { m_serialOut = enable; };

#ifdef ESP8266
    void registerInputCallback(HandleInputCallback inputCallback) { m_inputCallback = inputCallback; };
#endif

    // Stream overrides
    int available() override;
    int read() override;
    int peek() override;
    void flush() override;
        
    // Print overrides
    size_t write(uint8_t) override;
    size_t write(const uint8_t*, size_t) override;
    inline size_t write(String data) {
        return write((uint8_t*)data.c_str(), data.length());
    }

  protected:
    void sendBuffer();
    bool dbgClientClosed();
    
  private:
#ifdef ESP8266
    static const uint16_t m_bufferSize = 256;
#else
    static const uint16_t m_bufferSize = 64;
#endif
    byte m_buffer[m_bufferSize];
    uint16_t m_inPos = 0;
    boolean m_bufferedWrite = true;
    boolean m_setupLog = true;
    boolean m_serialOut = false;

#ifdef ESP8266
    HandleInputCallback m_inputCallback = NULL;

    WiFiServer m_DbgServer = NULL;
    WiFiClient m_DbgClient;
#endif
};

extern EspDebug espDebug;

#endif	// _ESPDEBUG_H
