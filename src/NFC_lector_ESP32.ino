/*
  NFC_card_reader_ESP32.ino
  
  Created: 23 Oct 2026
  Modified: 05 Nov 2026
  By: Daniel Santillana 
  University of Extremadura
  dsantil@alumnos.unex.es
  Spain
*/



#include <M5Unified.h>
#include <SPI.h>
#include <MFRC522.h>

// Definimos los pines que conectaste
#define SS_PIN 8
#define RST_PIN -1 // No lo hemos conectado, usamos -1

MFRC522 rfid(SS_PIN, RST_PIN);

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  
  M5.Lcd.setTextSize(2);
  M5.Lcd.clear(TFT_BLACK);
  M5.Lcd.setTextColor(TFT_WHITE);
  
  // Inicializamos el bus SPI especificando nuestros pines (SCK, MISO, MOSI, SS)
  SPI.begin(36, 35, 37, 8); 
  
  rfid.PCD_Init(); // Arrancamos el lector RFID
  
  M5.Lcd.setCursor(10, 20);
  M5.Lcd.println("Lector RFID listo");
  M5.Lcd.setCursor(10, 50);
  M5.Lcd.setTextColor(TFT_GREEN);
  M5.Lcd.println("Acerca un disco...");
}

void loop() {
  M5.update();

  // Si no hay tarjeta presente, el bucle se reinicia y no hace nada
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  // Si detecta tarjeta, limpiamos la pantalla y extraemos el código (UID)
  M5.Lcd.clear(TFT_BLACK);
  M5.Lcd.setCursor(10, 20);
  M5.Lcd.setTextColor(TFT_YELLOW);
  M5.Lcd.println("Disco detectado:");
  
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    uid += String(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase(); // Lo ponemos en mayúsculas para que quede más limpio
  
  M5.Lcd.setCursor(10, 60);
  M5.Lcd.setTextColor(TFT_WHITE);
  M5.Lcd.println("UID:" + uid);
  
  // Pausa de 2 segundos para que te dé tiempo a leerlo antes de pedir el siguiente
  delay(2000); 
  
  M5.Lcd.clear(TFT_BLACK);
  M5.Lcd.setCursor(10, 50);
  M5.Lcd.setTextColor(TFT_GREEN);
  M5.Lcd.println("Acerca un disco...");
}
