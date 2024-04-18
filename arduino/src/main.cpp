#include <Arduino.h>

#define RXD2 16
#define TXD2 17
#define TIMEOUT 250
#define MAX_TRIES 20

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
    uint16_t received_checksum = (data[length-1] << 8) | data[length-2];

    // Berechnet die Checksumme beginnend beim dritten Byte bis zum vorletzten Byte
    uint16_t sum = 0;
    for (size_t i = 2; i < length - 2; i++) {
        sum += data[i];
    }
    uint16_t calculated_checksum = sum ^ 0xFFFF;
    calculated_checksum = (calculated_checksum & 0xFF) << 8 | (calculated_checksum >> 8); // Byte-Swap

    // Debug-Ausgaben
    Serial.print("Received Checksum: ");
    Serial.println(received_checksum, HEX);
    Serial.print("Calculated Checksum: ");
    Serial.println(calculated_checksum, HEX);

    // Vergleicht die berechnete Checksumme mit der empfangenen
    return received_checksum == calculated_checksum;
}

void processBmsResponse(const uint8_t* data, size_t length, uint8_t expectedCmd) {
  for (int i = 0; i < length; i++) {
    Serial.print(data[i], HEX); // Ausgeben jedes Bytes in Hexadezimal
    Serial.print(" "); // Füge ein Leerzeichen zwischen den Hex-Werten ein
  }
  Serial.println(); // Füge eine neue Zeile am Ende hinzu


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

  Serial.println("Gültige Daten empfangen:");
  for (int i = 7; i < length - 2; i += 2) {
    uint16_t voltage = (data[i] << 8) | data[i+1];
    Serial.print("Zellenspannung: ");
    Serial.print(voltage);
    Serial.println(" mV");
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
        Serial.println("Timeout erreicht, sende erneut...");
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

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
  sendBmsCommand(0x03, 0x22, 0x01, 0x40, 0x14);  // bArg wird hier als Hex-Wert 0x40 übergeben, erwartet als Antwort auch 0x40
}

void loop() {
}
