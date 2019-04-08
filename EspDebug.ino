#include "EspDebug.h"

EspDebug::EspDebug() {
}

EspDebug::~EspDebug() {
#ifdef ESP8266
  m_DbgServer = NULL;
#endif
}


// Stream overrides
int EspDebug::available() {
  int result = -1;
  
#ifdef ESP8266
  result = m_DbgClient.available();
#endif

  return result;
}

int EspDebug::read() {
  int result = -1;
  
#ifdef ESP8266
  result = m_DbgClient.read();
#endif

  return result;
}

int EspDebug::peek() {
  int result = -1;

#ifdef ESP8266
  result = m_DbgClient.peek();
#endif

  return result;
}

void EspDebug::flush() {
#ifdef ESP8266
  m_DbgClient.flush();
#endif
}

// Print
size_t EspDebug::write(uint8_t data) {
  return write(&data, 1);
}

size_t EspDebug::write(const uint8_t *buffer, size_t size) {
  size_t result = 0;

  if (!m_serialOut && !m_setupLog && dbgClientClosed())
    return -1;
    
  for (size_t i=0; i<size; i++) {
    // force send
    if (m_inPos == m_bufferSize)
      sendBuffer();

    // probe send result
    if (m_inPos == m_bufferSize) {
      // write all to Serial
      if (m_serialOut)
        result = Serial.write(&buffer[i], (size - i));
      return result;
    }

    m_buffer[m_inPos] = buffer[i];
    m_inPos++;
    result++;
  }

  if (!m_bufferedWrite)
    sendBuffer();
    
  return result;
}

// others
bool EspDebug::dbgClientClosed() {
  bool result = false;

#ifdef ESP8266
  result = m_DbgClient.status() == CLOSED;
#endif

  return result;
}

void EspDebug::begin(uint16_t dbgServerPort) {
#ifdef ESP8266
  m_DbgServer = WiFiServer(dbgServerPort);
  m_DbgServer.begin();
  m_DbgServer.setNoDelay(true);
#endif
}

void EspDebug::loop() {
  // probe new client
#ifdef ESP8266
  if (m_DbgServer.hasClient()) {
    WiFiClient dbgClient = m_DbgServer.available();

    // discarding connection attempts if client is connected
    if (!dbgClientClosed()) {
      dbgClient.stop();
    } else {
    // accept new connection
      m_setupLog = false;
      
      m_DbgClient = dbgClient;
      m_DbgClient.setNoDelay(true);

      // force send
      if (m_SetupLogData != "") {
          m_DbgClient.write(m_SetupLogData.c_str(), m_SetupLogData.length());
          if (m_SetupLogData.length() == m_bufferSize)
            m_DbgClient.write("\n...\n\n");
          m_SetupLogData = "";
      }
    }
  }
#endif

  // output to network
  sendBuffer();  

  // input from network
#ifdef ESP8266
  while (m_DbgClient.available() > 0 && m_inputCallback != NULL)
    m_inputCallback(&m_DbgClient);
#endif
}

void EspDebug::sendWriteBuffer() {
  // output to network
  sendBuffer();  
}

void EspDebug::bufferedWrite(boolean enable) {
  m_bufferedWrite = enable;
}

void EspDebug::sendBuffer() {
  if (m_inPos == 0)
    return;

  if (m_serialOut) {
    int wrote = Serial.write(&m_buffer[m_SerialOut], (m_inPos - m_SerialOut));
    if (wrote > 0)
      m_SerialOut += wrote;
  }
    
#ifdef ESP8266
  if (dbgClientClosed()) {
    if (m_setupLog && m_inPos == m_bufferSize) {
      m_buffer[m_inPos] = 0;
      m_SetupLogData = String((char*)m_buffer);
      m_setupLog = false;
    }
    if (!m_setupLog)
      m_inPos = m_SerialOut = 0;
    return;
  }

  int socketSend = m_DbgClient.write(&m_buffer[0], m_inPos);

  // move buffer
  if (socketSend < m_inPos) {
    memcpy(&m_buffer[0], &m_buffer[socketSend], (m_inPos - socketSend));
    m_inPos -= socketSend;
    m_SerialOut -= socketSend;
  } else
#endif
    m_inPos = m_SerialOut = 0;
}


