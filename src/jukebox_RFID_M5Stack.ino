#include <SPI.h>
#include <SD.h>

// SD.h debe incluirse antes de M5Unified.h
#include <M5Unified.h>

#include <MFRC522.h>
#include <DFRobotDFPlayerMini.h>

// =====================================================
// CONFIGURACIÓN DE PINES
// =====================================================

// Bus SPI compartido por la SD interna y el RC522
constexpr uint8_t SPI_SCK_PIN  = 36;
constexpr uint8_t SPI_MISO_PIN = 35;
constexpr uint8_t SPI_MOSI_PIN = 37;

// Chip Select de la SD interna
constexpr uint8_t SD_CS_PIN = 4;

// Chip Select del lector RFID
constexpr uint8_t RFID_CS_PIN = 8;

// RC522 sin pin físico de reset
constexpr int8_t RFID_RST_PIN = -1;

// UART del DFPlayer
// DFPlayer TX, pin 3 -> GPIO 18 del CoreS3
// DFPlayer RX, pin 2 <- GPIO 17 del CoreS3
constexpr uint8_t DFPLAYER_RX_PIN = 18;
constexpr uint8_t DFPLAYER_TX_PIN = 17;

// Tamaño de letra de la pantalla de espera
constexpr float TAMANO_TEXTO_ESPERA = 1.5;

// =====================================================
// CONFIGURACIÓN GENERAL
// =====================================================

// Duración de reproducción de cada canción
constexpr unsigned long DURACION_REPRODUCCION_MS = 47000;

// Pausa mínima entre dos lecturas RFID
constexpr unsigned long BLOQUEO_RFID_MS = 2000;

// Frecuencia conservadora para la SD
constexpr uint32_t FRECUENCIA_SD = 10000000;

// Volumen del DFPlayer, rango válido entre 0 y 30
constexpr uint8_t VOLUMEN_DFPLAYER = 22;

// =====================================================
// DISPOSITIVOS
// =====================================================

HardwareSerial DFPlayerSerial(1);
DFRobotDFPlayerMini myDFPlayer;

MFRC522 mfrc522(RFID_CS_PIN, RFID_RST_PIN);

// Estado de cada periférico
bool sdDisponible = false;
bool rfidDisponible = false;
bool dfPlayerDisponible = false;

// =====================================================
// CATÁLOGO DE DISCOS
// =====================================================

struct Disco {
  byte uid[4];
  const char* titulo;
  const char* artista;
  const char* imagen;
  uint16_t pista;
};

const Disco catalogo[] = {
  {
    {0xCE, 0xD0, 0xCD, 0x5D},
    "Cream",
    "Pat Martino",
    "/cream.jpg",
    4
  },
  {
    {0x8F, 0x20, 0x8B, 0x06},
    "Flamingos",
    "Enrique Bunbury",
    "/flamingos.jpg",
    2
  },
  {
    {0x8F, 0xFE, 0x7E, 0x06},
    "Where The Light Is",
    "John Mayer",
    "/where_the_light_is.jpg",
    3
  },
  {
    {0xAF, 0x74, 0x7F, 0x06},
    "Let It Be",
    "The Beatles",
    "/let_it_be.jpg",
    6
  }
};

constexpr size_t NUM_DISCOS =
    sizeof(catalogo) / sizeof(catalogo[0]);

// =====================================================
// MÁQUINA DE ESTADOS
// =====================================================

enum EstadoSistema {
  MODO_ESPERA,
  MOSTRANDO_CARATULA,
  MODO_HISTORIAL,
  MENSAJE_TEMPORAL
};

EstadoSistema estadoActual = MODO_ESPERA;

unsigned long tiempoInicioCaratula = 0;
unsigned long tiempoInicioMensaje = 0;
unsigned long duracionMensaje = 0;
unsigned long ultimaLecturaRFID = 0;

// =====================================================
// HISTORIAL
// =====================================================

String ultimosDiscos[5] = {
  "-",
  "-",
  "-",
  "-",
  "-"
};

// =====================================================
// DECLARACIONES DE FUNCIONES
// =====================================================

bool inicializarSD();
bool inicializarRFID();
bool inicializarDFPlayer();

void gestionarPantallaTactil();
void gestionarTemporizadores();
void gestionarRFID();

void procesarTarjeta();
int buscarDiscoPorUID();
void imprimirUID();

void mostrarDisco(int indice);
void mostrarErrorImagen(const Disco& disco);
void finalizarReproduccion();

void pintarPantallaEspera();
void pintarHistorial();

void mostrarMensajeTemporal(
  const String& mensaje,
  uint32_t color,
  unsigned long duracion
);

void registrarEnHistorial(const String& metadatos);

// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(115200);
  delay(300);

  // Deseleccionamos inicialmente los dispositivos SPI
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);

  pinMode(RFID_CS_PIN, OUTPUT);
  digitalWrite(RFID_CS_PIN, HIGH);

  // Inicialización del CoreS3
  auto cfg = M5.config();
  M5.begin(cfg);

  /*
   * Activa la alimentación de los puertos externos.
   * El DFPlayer toma 5 V y GND del puerto A.
   */
  M5.Power.setExtOutput(true);

  /*
   * Esperamos a que la alimentación del DFPlayer
   * se estabilice antes de iniciar su UART.
   */
  delay(1500);

  M5.Display.clear(TFT_BLACK);
  M5.Display.setRotation(1);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(5, 5);

  Serial.println();
  Serial.println("================================");
  Serial.println("Iniciando catalogo musical NFC");
  Serial.println("================================");

  // Inicialización del bus SPI compartido
  SPI.begin(
    SPI_SCK_PIN,
    SPI_MISO_PIN,
    SPI_MOSI_PIN,
    -1
  );

  // Inicialización de los periféricos
  sdDisponible = inicializarSD();
  rfidDisponible = inicializarRFID();
  dfPlayerDisponible = inicializarDFPlayer();

  delay(700);

  pintarPantallaEspera();
}

// =====================================================
// LOOP PRINCIPAL
// =====================================================

void loop() {
  M5.update();

  gestionarPantallaTactil();
  gestionarTemporizadores();

  if (estadoActual == MODO_ESPERA) {
    gestionarRFID();
  }

  delay(5);
}

// =====================================================
// INICIALIZACIÓN DE LA MICROSD INTERNA
// =====================================================

bool inicializarSD() {
  Serial.println();
  Serial.println("Iniciando microSD interna...");

  digitalWrite(RFID_CS_PIN, HIGH);
  digitalWrite(SD_CS_PIN, HIGH);

  if (!SD.begin(
        SD_CS_PIN,
        SPI,
        FRECUENCIA_SD
      )) {
    Serial.println("ERROR: SD.begin() ha fallado.");
    return false;
  }

  if (SD.cardType() == CARD_NONE) {
    Serial.println(
      "ERROR: no se detecta la tarjeta microSD."
    );

    SD.end();
    return false;
  }

  Serial.println("MicroSD interna detectada.");

  for (size_t i = 0; i < NUM_DISCOS; i++) {
    Serial.print("Comprobando ");
    Serial.print(catalogo[i].imagen);
    Serial.print(": ");

    if (SD.exists(catalogo[i].imagen)) {
      Serial.println("OK");
    } else {
      Serial.println("NO EXISTE");
    }
  }

  return true;
}

// =====================================================
// INICIALIZACIÓN DEL RC522
// =====================================================

bool inicializarRFID() {
  Serial.println();
  Serial.println("Iniciando RC522...");

  digitalWrite(SD_CS_PIN, HIGH);
  digitalWrite(RFID_CS_PIN, HIGH);

  mfrc522.PCD_Init();
  delay(50);

  byte version =
      mfrc522.PCD_ReadRegister(MFRC522::VersionReg);

  Serial.print("Version RC522: 0x");

  if (version < 0x10) {
    Serial.print("0");
  }

  Serial.println(version, HEX);

  if (version == 0x00 || version == 0xFF) {
    Serial.println(
      "ERROR: no hay comunicacion con el RC522."
    );

    return false;
  }

  mfrc522.PCD_AntennaOn();
  mfrc522.PCD_SetAntennaGain(MFRC522::RxGain_max);

  Serial.println("RC522 detectado.");
  return true;
}

// =====================================================
// INICIALIZACIÓN DEL DFPLAYER
// =====================================================

bool inicializarDFPlayer() {
  Serial.println();
  Serial.println("Iniciando DFPlayer...");

  DFPlayerSerial.begin(
    9600,
    SERIAL_8N1,
    DFPLAYER_RX_PIN,
    DFPLAYER_TX_PIN
  );

  delay(1500);

  /*
   * Primer false:
   * no se esperan confirmaciones ACK del DFPlayer.
   *
   * Segundo false:
   * no se reinicia automáticamente el módulo.
   *
   * Esta es la configuración que ha funcionado
   * correctamente con tu DFPlayer.
   */
  if (!myDFPlayer.begin(
        DFPlayerSerial,
        false,
        false
      )) {
    Serial.println(
      "ERROR: no se ha podido iniciar el DFPlayer."
    );

    return false;
  }

  myDFPlayer.setTimeOut(500);
  myDFPlayer.outputDevice(DFPLAYER_DEVICE_SD);
  myDFPlayer.volume(VOLUMEN_DFPLAYER);

  delay(500);

  Serial.println("DFPlayer preparado sin ACK.");
  return true;
}

// =====================================================
// GESTIÓN DE LA PANTALLA TÁCTIL
// =====================================================

void gestionarPantallaTactil() {
  auto toque = M5.Touch.getDetail();

  if (!toque.wasPressed()) {
    return;
  }

  if (estadoActual == MODO_HISTORIAL) {
    estadoActual = MODO_ESPERA;
    pintarPantallaEspera();
    return;
  }

  if (estadoActual == MOSTRANDO_CARATULA) {
    finalizarReproduccion();
  }

  estadoActual = MODO_HISTORIAL;
  pintarHistorial();
}

// =====================================================
// GESTIÓN DE TEMPORIZADORES
// =====================================================

void gestionarTemporizadores() {
  unsigned long ahora = millis();

  if (estadoActual == MOSTRANDO_CARATULA) {
    if (
      ahora - tiempoInicioCaratula >=
      DURACION_REPRODUCCION_MS
    ) {
      finalizarReproduccion();

      estadoActual = MODO_ESPERA;
      pintarPantallaEspera();
    }
  }

  if (estadoActual == MENSAJE_TEMPORAL) {
    if (
      ahora - tiempoInicioMensaje >=
      duracionMensaje
    ) {
      estadoActual = MODO_ESPERA;
      pintarPantallaEspera();
    }
  }
}

// =====================================================
// GESTIÓN DEL RFID
// =====================================================

void gestionarRFID() {
  if (!rfidDisponible) {
    return;
  }

  unsigned long ahora = millis();

  if (
    ultimaLecturaRFID != 0 &&
    ahora - ultimaLecturaRFID < BLOQUEO_RFID_MS
  ) {
    return;
  }

  // Deseleccionamos la SD antes de comunicarnos con RFID
  digitalWrite(SD_CS_PIN, HIGH);

  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  ultimaLecturaRFID = ahora;

  procesarTarjeta();
}

// =====================================================
// PROCESAMIENTO DE LA TARJETA
// =====================================================

void procesarTarjeta() {
  imprimirUID();

  int indice = buscarDiscoPorUID();

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  // Deseleccionamos el RC522
  digitalWrite(RFID_CS_PIN, HIGH);

  if (indice < 0) {
    mostrarMensajeTemporal(
      "Disco no registrado",
      TFT_RED,
      1500
    );

    return;
  }

  mostrarDisco(indice);
}

// =====================================================
// BÚSQUEDA DEL DISCO POR UID
// =====================================================

int buscarDiscoPorUID() {
  if (mfrc522.uid.size != 4) {
    Serial.print("UID no compatible. Longitud: ");
    Serial.println(mfrc522.uid.size);
    return -1;
  }

  for (size_t i = 0; i < NUM_DISCOS; i++) {
    if (
      memcmp(
        mfrc522.uid.uidByte,
        catalogo[i].uid,
        4
      ) == 0
    ) {
      return static_cast<int>(i);
    }
  }

  return -1;
}

// =====================================================
// MOSTRAR Y REPRODUCIR UN DISCO
// =====================================================

void mostrarDisco(int indice) {
  const Disco& disco = catalogo[indice];

  String metadatos =
      String(disco.titulo) +
      " - " +
      String(disco.artista);

  registrarEnHistorial(metadatos);

  Serial.println();
  Serial.print("Disco encontrado: ");
  Serial.println(metadatos);

  if (dfPlayerDisponible) {
    myDFPlayer.playMp3Folder(disco.pista);

    Serial.print("Reproduciendo pista MP3: ");
    Serial.println(disco.pista);
  } else {
    Serial.println(
      "Aviso: DFPlayer no disponible."
    );
  }

  M5.Display.clear(TFT_BLACK);

  bool imagenCargada = false;

  if (sdDisponible) {
    digitalWrite(RFID_CS_PIN, HIGH);

    imagenCargada = M5.Display.drawJpgFile(
      SD,
      disco.imagen,
      0,
      0
    );
  }

  if (!imagenCargada) {
    mostrarErrorImagen(disco);
  } else {
    // Franja inferior de título y artista
    M5.Display.fillRect(
      0,
      M5.Display.height() - 38,
      M5.Display.width(),
      38,
      TFT_BLACK
    );

    M5.Display.setTextSize(1);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);

    M5.Display.drawCenterString(
      disco.titulo,
      M5.Display.width() / 2,
      M5.Display.height() - 37,
      &fonts::Font2
    );

    M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);

    M5.Display.drawCenterString(
      disco.artista,
      M5.Display.width() / 2,
      M5.Display.height() - 19,
      &fonts::Font2
    );
  }

  estadoActual = MOSTRANDO_CARATULA;
  tiempoInicioCaratula = millis();
}

// =====================================================
// ERROR AL MOSTRAR LA IMAGEN
// =====================================================

void mostrarErrorImagen(const Disco& disco) {
  M5.Display.clear(TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_RED, TFT_BLACK);

  if (!sdDisponible) {
    M5.Display.drawCenterString(
      "MicroSD no disponible",
      M5.Display.width() / 2,
      70,
      &fonts::Font2
    );
  } else if (!SD.exists(disco.imagen)) {
    M5.Display.drawCenterString(
      "El archivo no existe",
      M5.Display.width() / 2,
      70,
      &fonts::Font2
    );
  } else {
    M5.Display.drawCenterString(
      "No se puede abrir el JPG",
      M5.Display.width() / 2,
      70,
      &fonts::Font2
    );
  }

  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);

  M5.Display.drawCenterString(
    disco.imagen,
    M5.Display.width() / 2,
    135,
    &fonts::Font2
  );

  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);

  M5.Display.drawCenterString(
    disco.titulo,
    M5.Display.width() / 2,
    175,
    &fonts::Font2
  );
}

// =====================================================
// FINALIZAR REPRODUCCIÓN
// =====================================================

void finalizarReproduccion() {
  if (dfPlayerDisponible) {
    myDFPlayer.stop();
  }

  Serial.println("Reproduccion finalizada.");
}

// =====================================================
// HISTORIAL
// =====================================================

void registrarEnHistorial(const String& metadatos) {
  if (ultimosDiscos[0] == metadatos) {
    return;
  }

  for (int i = 4; i > 0; i--) {
    ultimosDiscos[i] = ultimosDiscos[i - 1];
  }

  ultimosDiscos[0] = metadatos;
}

void pintarHistorial() {
  M5.Display.clear(TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);

  M5.Display.drawCenterString(
    "ULTIMOS DISCOS",
    M5.Display.width() / 2,
    8,
    &fonts::Font4
  );

  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);

  for (int i = 0; i < 5; i++) {
    int y = 48 + i * 29;

    M5.Display.setCursor(10, y);

    M5.Display.printf(
      "%d. %s",
      i + 1,
      ultimosDiscos[i].c_str()
    );
  }

  M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);

  M5.Display.drawCenterString(
    "Toca para volver",
    M5.Display.width() / 2,
    M5.Display.height() - 23,
    &fonts::Font2
  );
}

// =====================================================
// PANTALLA DE ESPERA
// =====================================================

void pintarPantallaEspera() {
  M5.Display.clear(TFT_BLACK);

  M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
  M5.Display.setTextSize(TAMANO_TEXTO_ESPERA);

  M5.Display.drawCenterString(
    "Acerca un disco",
    M5.Display.width() / 2,
    65,
    &fonts::Font4
  );

  M5.Display.drawCenterString(
    "al lector...",
    M5.Display.width() / 2,
    110,
    &fonts::Font4
  );

  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);

  M5.Display.drawCenterString(
    "Toca para ver historial",
    M5.Display.width() / 2,
    M5.Display.height() - 23,
    &fonts::Font2
  );
}

// =====================================================
// MENSAJE TEMPORAL
// =====================================================

void mostrarMensajeTemporal(
    const String& mensaje,
    uint32_t color,
    unsigned long duracion) {

  M5.Display.clear(TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(color, TFT_BLACK);

  M5.Display.drawCenterString(
    mensaje,
    M5.Display.width() / 2,
    M5.Display.height() / 2 - 15,
    &fonts::Font4
  );

  tiempoInicioMensaje = millis();
  duracionMensaje = duracion;
  estadoActual = MENSAJE_TEMPORAL;
}

// =====================================================
// MOSTRAR UID EN EL MONITOR SERIE
// =====================================================

void imprimirUID() {
  Serial.print("UID detectado [");
  Serial.print(mfrc522.uid.size);
  Serial.print(" bytes]: ");

  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) {
      Serial.print("0");
    }

    Serial.print(
      mfrc522.uid.uidByte[i],
      HEX
    );

    if (i + 1 < mfrc522.uid.size) {
      Serial.print(":");
    }
  }

  Serial.println();
}
