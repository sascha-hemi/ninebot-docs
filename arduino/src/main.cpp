#include <Arduino.h>

#define RXD2 16
#define TXD2 17
#define TIMEOUT 200
#define MAX_TRIES 25

String serialNumber;
int16_t current;

uint16_t calculateChecksum(const uint8_t* data, size_t length) {
  uint16_t sum = 0;
  for (size_t i = 0; i < length; i++) {
    sum += data[i];
  }
  uint16_t checksum = sum ^ 0xFFFF;
  return (checksum & 0xFF) << 8 | (checksum >> 8);
}

bool verifyChecksum(const uint8_t* data, size_t length) {
    if (length < 4) {  // Stellt sicher, dass genug Daten für eine Checksumme vorhanden sind
        Serial.println("Datenlänge ist zu kurz für Checksummenüberprüfung.");
        return false;
    }
    // Empfangene Checksumme aus den letzten zwei Bytes im Little-Endian Format
    uint16_t received_checksum = (data[length-2] << 8) | data[length-1];

    // Berechnet die Checksumme beginnend beim dritten Byte bis zum vorletzten Byte
    uint16_t sum = 0;
    for (size_t i = 2; i < length - 2; i++) {
        sum += data[i];
    }
    uint16_t calculated_checksum = sum ^ 0xFFFF;
    calculated_checksum = (calculated_checksum & 0xFF) << 8 | (calculated_checksum >> 8); // Byte-Swap

    // // Debug-Ausgaben
    // Serial.print("Received Checksum: ");
    // Serial.println(received_checksum, HEX);
    // Serial.print("Calculated Checksum: ");
    // Serial.println(calculated_checksum, HEX);

    // Vergleicht die berechnete Checksumme mit der empfangenen
    return received_checksum == calculated_checksum;
}

void processBmsResponse(const uint8_t* data, size_t length, uint8_t expectedCmd) {
  if (!verifyChecksum(data, length)) {
    Serial.println("Checksumme ungültig!");
    return;
  }
  if (data[5] != expectedCmd) { // Korrekt, data[4] sollte das bArg sein, das überprüft wird
    Serial.print("Erwartetes Register (Hex): ");
    Serial.println(expectedCmd, HEX); // Ausgabe in Hex
    Serial.print("Erhaltenes Register (Hex): ");
    Serial.println(data[5], HEX); // Ausgabe in Hex
    Serial.println("Falsches Register in der Antwort.");
    return;
  }

  // Serial.println("Gültige Daten empfangen:");
  switch(data[5]){
    case 0x40: // Fall für Zellenspannungen
            // Serial.println("Verarbeitung der Zellenspannungen:");
            for (int i = 6; i < length - 2; i += 2) {
                uint16_t voltage = (data[i+1] << 8) | data[i];  // Little-Endian Verarbeitung
                Serial.print("Zellenspannung: ");
                Serial.print(voltage);
                Serial.println(" mV");
            }
            break;

    case 0x18:  // Factory capacity
            Serial.print("Factory Capacity (mAh): ");
            Serial.println((data[7] << 8) | data[6]);
            break;

    case 0x17:  // Firmware version
            Serial.print("Firmware Version: ");
            Serial.println((data[7] << 8) | data[6], HEX);  // Datenindex 6 und 7 nach '55 AA'
            break;

    case 0x19:  // Actual capacity
            Serial.print("Actual Capacity (mAh): ");
            Serial.println((data[7] << 8) | data[6]);
            break;

    case 0x30:  // Statusbits
            Serial.println("Statusbits:");
            for (int i = 0; i < 16; i++) {
                Serial.println("Bit " + String(i) + ": " + String(((data[7] << 8) | data[6] >> i) & 1));
            }
            break;

    case 0x31:  // Remaining capacity mAh
            Serial.print("Remaining Capacity (mAh): ");
            Serial.println((data[7] << 8) | data[6]);
            break;

    case 0x32:  // Remaining capacity %
            Serial.print("Remaining Capacity (%): ");
            Serial.println((data[7] << 8) | data[6]);
            break;

    case 0x33:  // Current
            Serial.print("Current (mA): ");
            current = (data[7] << 8) | data[6];
            if (current > 32767) current -= 65536;  // Umrechnung für 16-bit signed
            Serial.println(current*10);
            break;

    case 0x34:  // Voltage
            Serial.print("Voltage (mV): ");
            Serial.println(((data[7] << 8) | data[6])*10);
            break;

    case 0x3B:  // Health
            Serial.print("Health (%): ");
            Serial.println((data[7] << 8) | data[6]);
            break;

    case 0x10: // Fall für Seriennummer
            // Serial.println("Verarbeitung der Seriennummer:");
            serialNumber = "";
            for (int i = 6; i < length - 2; i++) {
                serialNumber += (char)data[i]; // Konvertiere jedes Byte zu einem Zeichen und füge es zur Seriennummer hinzu
            }
            Serial.print("Seriennummer: ");
            Serial.println(serialNumber);
            break;
  }
}

bool sendBmsCommand(uint8_t bLen, uint8_t bAddr, uint8_t bCmd, uint8_t bArg, uint8_t bPayload) {
  uint8_t command[] = {0x55, 0xAA, bLen, bAddr, bCmd, bArg, bPayload};
  uint16_t checksum = calculateChecksum(command + 2, sizeof(command) - 2);

  for (int attempt = 0; attempt < MAX_TRIES; attempt++) {
    Serial2.write(command, sizeof(command));
    Serial2.write((uint8_t)(checksum >> 8));
    Serial2.write((uint8_t)(checksum & 0xFF));

    unsigned long startTime = millis();
    while (Serial2.available() == 0) {
      if (millis() - startTime >= TIMEOUT) {
        // Serial.println("Timeout erreicht, sende erneut...");
        break;
      }
    }

    if (Serial2.available() > 0) {
      uint8_t response[256];
      int index = 0;
      while (Serial2.available() > 0 && index < 256) {
        response[index++] = Serial2.read();
      }
      processBmsResponse(response, index, bArg); // Übergabe von bArg direkt in Hex
      return true;
    }
  }

  Serial.println("Maximale Versuche erreicht, keine Antwort.");
  return false;
}

void sendAllCommands() {
  sendBmsCommand(0x03, 0x22, 0x01, 0x40, 0x14);
  sendBmsCommand(0x03, 0x22, 0x01, 0x10, 0x0E);
  sendBmsCommand(0x03, 0x22, 0x01, 0x17, 0x02);
  sendBmsCommand(0x03, 0x22, 0x01, 0x18, 0x02);
  sendBmsCommand(0x03, 0x22, 0x01, 0x19, 0x02);
  sendBmsCommand(0x03, 0x22, 0x01, 0x31, 0x02);
  sendBmsCommand(0x03, 0x22, 0x01, 0x32, 0x02);
  sendBmsCommand(0x03, 0x22, 0x01, 0x33, 0x02);
  sendBmsCommand(0x03, 0x22, 0x01, 0x34, 0x02);
  sendBmsCommand(0x03, 0x22, 0x01, 0x3B, 0x02);
  sendBmsCommand(0x03, 0x22, 0x01, 0x30, 0x02);
  sendBmsCommand(0x03, 0x22, 0x01, 0x35, 0x02);
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
       
}


void loop() {
  static unsigned long lastMillis = 0;
  if (millis() - lastMillis >= 10000) {
    lastMillis = millis();
    sendAllCommands();
  }
}