#include "Debug.h"

EspDebug::EspDebug() {
}

EspDebug::~EspDebug() {
  m_DbgServer = NULL;
}


// Stream overrides
int EspDebug::available() {
  return m_DbgClient.available();
}

int EspDebug::read() {
  return m_DbgClient.read();
}

int EspDebug::peek() {
  return m_DbgClient.peek();
}

void EspDebug::flush() {
  m_DbgClient.flush();
}

// Print
size_t EspDebug::write(uint8_t data) {
  return write(&data, 1);
}

size_t EspDebug::write(const uint8_t *buffer, size_t size) {
  size_t result = 0;

  if (!m_serialOut && !m_setupLog && m_DbgClient.status() == CLOSED)
    return -1;
    
  for (size_t i=0; i<size; i++) {
    // force send
    if (m_inPos == m_bufferSize) {
      if (m_setupLog)
        m_setupLog = false;
      sendBuffer();
    }

    if (m_inPos == m_bufferSize)
      return result;

    m_buffer[m_inPos] = buffer[i];
    m_inPos++;
    result++;
  }

  if (!m_bufferedWrite)
    sendBuffer();
    
  return result;
}

// others
void EspDebug::begin(uint16_t dbgServerPort) {
  m_DbgServer = WiFiServer(dbgServerPort);
  m_DbgServer.begin();
  m_DbgServer.setNoDelay(true);
}

void EspDebug::loop() {
  // probe new client
  if (m_DbgServer.hasClient()) {
    WiFiClient dbgClient = m_DbgServer.available();

    // discarding connection attempts if client is connected
    if (m_DbgClient.status() != CLOSED) {
      dbgClient.stop();
    } else {
    // accept new connection
      m_setupLog = false;
      
      m_DbgClient = dbgClient;
      m_DbgClient.setNoDelay(true);

      if (m_inPos == m_bufferSize)
        print("\n...\n\n");
    }
  }

  // output to network
  sendBuffer();  

  // input from network
  while (m_DbgClient.available() > 0 && m_inputCallback != NULL)
    m_inputCallback(&m_DbgClient);
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

  if (m_serialOut)
    Serial.write(&m_buffer[0], m_inPos);
    
  if (m_DbgClient.status() == CLOSED) {
    if (!m_setupLog)
      m_inPos = 0;
    return;
  }

  int socketSend = m_DbgClient.write(&m_buffer[0], m_inPos);

  // move buffer
  if (socketSend < m_inPos) {
    memcpy(&m_buffer[0], &m_buffer[socketSend], (m_inPos - socketSend));
    m_inPos -= socketSend;
  } else
    m_inPos = 0;
}


