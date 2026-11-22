#include <Arduino.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <stdlib.h>
#include <string.h>

static const uint8_t OLED_W = 128, OLED_H = 64;
static const uint8_t UART_RX = 10, UART_TX = 11;
static const uint8_t ENCODER_CLK = 8, ENCODER_DT = 9, BUTTON_PIN = 2;
static const uint32_t UART_BAUD = 38400;
static const uint8_t TEXT_LEN = 48;
static const uint8_t OLED_ADDR = 0x3C;
static const uint8_t OLED_ADDR_ALT = 0x3D;
static const uint16_t DEBOUNCE_MS = 35;

SoftwareSerial esp(UART_RX, UART_TX);
Adafruit_SSD1306 display(OLED_W, OLED_H, &Wire, -1);

static bool oledReady = false;
static bool dirty = true;
static bool connected = false;

static bool playing = false;
static bool paused = false;

static uint8_t volumeLevel = 50, lastClk = HIGH;
static unsigned long lastButtonChange = 0, lastEdgeUs = 0;
static uint8_t buttonReading = HIGH, buttonStable = HIGH;

static char title[TEXT_LEN] = "-"; 
static char artist[TEXT_LEN] = "-";
static char line[96];
static uint8_t lineLen = 0;
static bool overflow = false;

static void sendLine(const char *s) { esp.println(s); }
static void sendVolume() { esp.print(F("VOL:")); esp.println(volumeLevel); }
static void sendPlayback() { sendLine(playing ? "PLAY" : "PAUSE"); }

static void setText(char *dst, const char *src) {
  if (!src || !*src) src = "-";
  strncpy(dst, src, TEXT_LEN - 1); dst[TEXT_LEN - 1] = '\0'; dirty = true;
}

static void receiveLine(char *s) {
  if (!strcmp(s, "BT:1")) {
    connected = true; dirty = true;
  } else if (!strcmp(s, "BT:0")) {
    connected = false; playing = paused = false; setText(title, "-"); setText(artist, "-");
  } else if (!strcmp(s, "STATUS:PLAYING")) {
    playing = true; paused = false; dirty = true;
  } else if (!strcmp(s, "STATUS:PAUSED")) {
    playing = false; paused = true; dirty = true;
  } else if (!strcmp(s, "STATUS:NOT_PLAYING")) {
    playing = paused = false; dirty = true;
  } else if (!strncmp(s, "VOLUME:", 7)) {
    char *end = nullptr;
    long newVolume = strtol(s + 7, &end, 10);
    if (end != s + 7 && *end == '\0' && newVolume >= 0 && newVolume <= 100) {
      volumeLevel = (uint8_t)newVolume;
      dirty = true;
    }
  } else if (!strncmp(s, "TITLE:", 6)) setText(title, s + 6);
  else if (!strncmp(s, "ARTIST:", 7)) setText(artist, s + 7);
}

static void pollUart() {
  while (esp.available()) {
    char c = (char)esp.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (!overflow) { line[lineLen] = '\0'; if (lineLen) receiveLine(line); }
      lineLen = 0; overflow = false;
    } else if (!overflow) {
      if (lineLen < sizeof(line) - 1) line[lineLen++] = c; else overflow = true;
    }
  }
}

static void pollControls() {
  uint8_t clk = digitalRead(ENCODER_CLK);
  if (lastClk == HIGH && clk == LOW && micros() - lastEdgeUs >= 4000) {
    lastEdgeUs = micros();
    int16_t step = digitalRead(ENCODER_DT) ? 5 : -5;
    int16_t v = (int16_t)volumeLevel + step;
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    if (v != volumeLevel) {
      volumeLevel = (uint8_t)v;
      dirty = true;
      sendVolume();
    }
  }
  lastClk = clk;
  uint8_t raw = digitalRead(BUTTON_PIN);
  if (raw != buttonReading) { buttonReading = raw; lastButtonChange = millis(); }
  if (millis() - lastButtonChange >= DEBOUNCE_MS && raw != buttonStable) {
    buttonStable = raw;
    if (buttonStable == LOW) { playing = !playing; paused = !playing; dirty = true; sendPlayback(); }
  }
}

static void drawText(uint8_t x, uint8_t y, const char *label, const char *value, uint8_t width) {
  display.setCursor(x, y); display.print(label);
  char shown[22]; strncpy(shown, value, width);
  shown[width] = '\0';
  if (strlen(value) > width && width > 3) { shown[width - 3] = '.'; shown[width - 2] = '.'; shown[width - 1] = '.'; }
  display.setCursor(x + (uint8_t)(strlen(label) * 6), y); display.print(shown);
}

static void redraw() {
  if (!dirty || !oledReady) return;
  display.clearDisplay(); display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0); display.print(F("BT: ")); display.print(connected ? F("Connected") : F("Disconnected"));
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);
  display.setCursor(0, 13); display.print(F("Status: "));
  display.print(!connected ? F("-") : playing ? F("Playing") : paused ? F("Paused") : F("Not Playing"));
  drawText(0, 26, "Title: ", connected ? title : "-", 14);
  drawText(0, 38, "Artist: ", connected ? artist : "-", 13);
  display.setCursor(0, 49); display.print(F("Volume: ")); display.print(volumeLevel); display.print('%');
  display.drawRect(0, 57, 128, 7, SSD1306_WHITE);
  uint8_t fill = (uint16_t)volumeLevel * 126 / 100;
  if (fill) display.fillRect(1, 58, fill, 5, SSD1306_WHITE);
  display.display(); dirty = false;
}

void setup() {
  pinMode(ENCODER_CLK, INPUT_PULLUP); pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(BUTTON_PIN, INPUT_PULLUP); lastClk = digitalRead(ENCODER_CLK);
  buttonReading = buttonStable = digitalRead(BUTTON_PIN);
  esp.begin(UART_BAUD); Wire.begin(); Wire.setClock(100000UL);
  oledReady = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (!oledReady) oledReady = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR_ALT);
  if (oledReady) { display.clearDisplay(); display.display(); }
  sendVolume(); sendLine("STATUS?");
}

void loop() { pollControls(); pollUart(); redraw(); }
