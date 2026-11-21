#include <Arduino.h>
#include <ESP_I2S.h>
#include <BluetoothA2DPSink.h>
#include <stdlib.h>
#include <string.h>

#define I2S_BCLK 18
#define I2S_LRC 19
#define I2S_DOUT 22
#define UART_BAUD 38400

I2SClass i2s;
BluetoothA2DPSink a2dp(i2s);
static char commandLine[32];
static uint8_t commandLength = 0;
static bool commandOverflow = false;
static bool btConnected = false;
static bool haveTitle = false, haveArtist = false;
static char lastTitle[96] = "", lastArtist[96] = "";
static bool hasPlayedSinceConnect = false;

static const char *audioStatus(esp_a2d_audio_state_t state) {
  if (state == ESP_A2D_AUDIO_STATE_STARTED) return "STATUS:PLAYING";
  return hasPlayedSinceConnect ? "STATUS:PAUSED" : "STATUS:NOT_PLAYING";
}

static int volumePercent(int rawVolume) {
  if (rawVolume < 0) rawVolume = 0;
  if (rawVolume > 127) rawVolume = 127;
  return (rawVolume * 100 + 63) / 127;
}

static void sendVolumeStatus(int rawVolume) {
  Serial2.print("VOLUME:");
  Serial2.println(volumePercent(rawVolume));
}

static void sendState() {
  Serial2.println(btConnected ? "BT:1" : "BT:0");
  if (btConnected) {
    esp_a2d_audio_state_t state = a2dp.get_audio_state();
    Serial2.println(audioStatus(state));
    sendVolumeStatus(a2dp.get_volume());
    if (haveTitle) { Serial2.print("TITLE:"); Serial2.println(lastTitle); }
    if (haveArtist) { Serial2.print("ARTIST:"); Serial2.println(lastArtist); }
  } else Serial2.println("STATUS:NOT_PLAYING");
}

void connection_state_changed(esp_a2d_connection_state_t state, void *) {
  btConnected = state == ESP_A2D_CONNECTION_STATE_CONNECTED;
  if (!btConnected) { haveTitle = haveArtist = false; hasPlayedSinceConnect = false; }
  else hasPlayedSinceConnect = false;
  sendState();
}

static void audio_state_changed(esp_a2d_audio_state_t state, void *) {
  if (state == ESP_A2D_AUDIO_STATE_STARTED) hasPlayedSinceConnect = true;
  Serial2.println(audioStatus(state));
}

void avrc_metadata_callback(uint8_t id, const uint8_t *text) {
  if (!text) return;
  if (id == ESP_AVRC_MD_ATTR_TITLE) {
    strlcpy(lastTitle, (const char *)text, sizeof(lastTitle)); haveTitle = true;
    Serial2.print("TITLE:"); Serial2.println(lastTitle);
  } else if (id == ESP_AVRC_MD_ATTR_ARTIST) {
    strlcpy(lastArtist, (const char *)text, sizeof(lastArtist)); haveArtist = true;
    Serial2.print("ARTIST:"); Serial2.println(lastArtist);
  }
}

static void volume_changed(int rawVolume) {
  sendVolumeStatus(rawVolume);
}

static void handleCommand(char *line) {
  if (!strcmp(line, "STATUS?")) { sendState(); return; }
  if (!btConnected) return;
  if (!strcmp(line, "PLAY")) a2dp.play();
  else if (!strcmp(line, "PAUSE")) a2dp.pause();
  else if (!strncmp(line, "VOL:", 4)) {
    char *end = nullptr; long v = strtol(line + 4, &end, 10);
    if (end != line + 4 && *end == '\0' && v >= 0 && v <= 100) {
      a2dp.set_volume((uint8_t)((v * 127L + 50L) / 100L));
      sendVolumeStatus(a2dp.get_volume());
    }
  }
}

static void pollCommands() {
  while (Serial2.available()) {
    char c = (char)Serial2.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (!commandOverflow) { commandLine[commandLength] = '\0'; if (commandLength) handleCommand(commandLine); }
      commandLength = 0; commandOverflow = false;
    } else if (!commandOverflow) {
      if (commandLength < sizeof(commandLine) - 1) commandLine[commandLength++] = c;
      else commandOverflow = true;
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(UART_BAUD, SERIAL_8N1, 16, 17);
  i2s.setPins(I2S_BCLK, I2S_LRC, I2S_DOUT);
  if (!i2s.begin(I2S_MODE_STD, 44100, I2S_DATA_BIT_WIDTH_16BIT,
                 I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
    while (true) delay(1000);
  }
  a2dp.set_on_connection_state_changed(connection_state_changed);
  a2dp.set_on_audio_state_changed(audio_state_changed);
  a2dp.set_avrc_metadata_callback(avrc_metadata_callback);
  a2dp.set_on_volumechange(volume_changed);
  a2dp.start("Tyu MP3 Player");
}

void loop() { pollCommands(); delay(2); }
