#include <Arduino.h>

void restoreRTCDataFromRTCMemory() {
    if (ESP.rtcUserMemoryRead(0, (uint32_t*) &rtcData, sizeof(rtcData))) {
      uint32_t crcOfData = calculateCRC32(((uint8_t*) &rtcData) + 4, sizeof(rtcData) - 4);
      Serial.print("CRC32 of data: ");
      Serial.println(crcOfData, HEX);
      Serial.print("CRC32 read from RTC: ");
      Serial.println(rtcData.crc32, HEX);
      if (crcOfData != rtcData.crc32) {
        Serial.println("CRC32 in RTC memory doesn't match CRC32 of data. Data is probably invalid!");
      }
      else {
        Serial.println("CRC32 check ok, data is probably valid.");
      }
    }
}

void writeRTCMemory() {
  // Update CRC32 of data
  rtcData.crc32 = calculateCRC32(((uint8_t*) &rtcData) + 4, sizeof(rtcData) - 4);
  // Write struct to RTC memory
  if (ESP.rtcUserMemoryWrite(0, (uint32_t*) &rtcData, sizeof(rtcData))) {
    Serial.println("wrote RTC Memory..");
    // Serial.println("Write: ");
    // printMemory();
    // Serial.println();
  }
}

uint32_t calculateCRC32(const uint8_t *data, size_t length)
{
  uint32_t crc = 0xffffffff;
  while (length--) {
    uint8_t c = *data++;
    for (uint32_t i = 0x80; i > 0; i >>= 1) {
      bool bit = crc & 0x80000000;
      if (c & i) {
        bit = !bit;
      }
      crc <<= 1;
      if (bit) {
        crc ^= 0x04c11db7;
      }
    }
  }
  return crc;
}

void printMemory() {
  char buf[3];
  for (int i = 0; i < sizeof(rtcData); i++) {
    sprintf(buf, "%02X", rtcData.data[i]);
    Serial.print(buf);
    if ((i + 1) % 32 == 0) {
      Serial.println();
    }
    else {
      Serial.print(" ");
    }
  }
  Serial.println();
}


void initRTCMemory() {
  // Generate new data set for the struct
  for (int i = 0; i < sizeof(rtcData); i++) {
    rtcData.data[i] = 0x00;
  }
  writeRTCMemory();
}
