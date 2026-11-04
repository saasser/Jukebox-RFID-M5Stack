#include <M5Unified.h>
#include <DFRobotDFPlayerMini.h>

HardwareSerial DFPlayerSerial(1);
DFRobotDFPlayerMini dfPlayer;

constexpr uint8_t DFPLAYER_RX_PIN = 18;
constexpr uint8_t DFPLAYER_TX_PIN = 17;

void setup() {
  Serial.begin(115200);

  auto cfg = M5.config();
  M5.begin(cfg);

  M5.Power.setExtOutput(true);
  delay(1000);

  Serial.println("Iniciando DFPlayer...");

  DFPlayerSerial.begin(
    9600,
    SERIAL_8N1,
    DFPLAYER_RX_PIN,
    DFPLAYER_TX_PIN
  );

  delay(1500);

  if (!dfPlayer.begin(DFPlayerSerial, false, false)) {
    Serial.println("ERROR: DFPlayer no responde.");
    Serial.println("Revisa VCC, GND y cruce TX/RX.");

    M5.Display.clear(TFT_BLACK);
    M5.Display.setTextColor(TFT_RED, TFT_BLACK);
    M5.Display.drawCenterString(
      "DFPlayer no responde",
      M5.Display.width() / 2,
      100,
      &fonts::Font2
    );

    return;
  }

  Serial.println("DFPlayer detectado.");

  M5.Display.clear(TFT_BLACK);
  M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
  M5.Display.drawCenterString(
    "DFPlayer detectado",
    M5.Display.width() / 2,
    100,
    &fonts::Font2
  );

  dfPlayer.outputDevice(DFPLAYER_DEVICE_SD);
  dfPlayer.volume(23);

  delay(500);

  // Reproduce /mp3/0001.mp3
  dfPlayer.playMp3Folder(2);
}

void loop() {
  M5.update();

  if (dfPlayer.available()) {
    Serial.print("Mensaje DFPlayer. Tipo: ");
    Serial.print(dfPlayer.readType());
    Serial.print(" Valor: ");
    Serial.println(dfPlayer.read());
  }

  delay(10);
}
