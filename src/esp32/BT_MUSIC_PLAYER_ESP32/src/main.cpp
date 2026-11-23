#include <Arduino.h>
#include <ESP_I2S.h>
#include <BluetoothA2DPSink.h>
#include <driver/dac.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define I2S_BCLK 18
#define I2S_LRC 19
#define I2S_DOUT 22
#define UART_BAUD 38400
#define RCA_DAC_PIN 25
#define RCA_SAMPLE_RATE 8000
#define RCA_BUFFER_SIZE 2048
#define RCA_BUFFER_MASK (RCA_BUFFER_SIZE - 1)

I2SClass i2s;
BluetoothA2DPSink a2dp(i2s);
static char commandLine[32];
static uint8_t commandLength = 0;
static bool commandOverflow = false;
static bool btConnected = false;
static bool haveTitle = false, haveArtist = false;
static char lastTitle[96] = "", lastArtist[96] = "";
static bool hasPlayedSinceConnect = false;
static hw_timer_t *rcaTimer = nullptr;
static uint8_t rcaSamples[RCA_BUFFER_SIZE];
static volatile uint16_t rcaWriteIndex = 0;
static volatile uint16_t rcaReadIndex = 0;
static uint32_t audioSampleRate = 44100;
static uint32_t resamplePhase = 0;
static float filterB0 = 0.0f, filterB1 = 0.0f, filterB2 = 0.0f;
static float filterA1 = 0.0f, filterA2 = 0.0f;
static float filterX1 = 0.0f, filterX2 = 0.0f;
static float filterY1 = 0.0f, filterY2 = 0.0f;

static void updateRcaFilter(uint16_t sampleRate) {
  if (sampleRate == 0) return;
  audioSampleRate = sampleRate;

  const float k = tanf(3.14159265f * 120.0f / sampleRate);
  const float norm = 1.0f / (1.0f + 1.41421356f * k + k * k);
  filterB0 = k * k * norm;
  filterB1 = 2.0f * filterB0;
  filterB2 = filterB0;
  filterA1 = 2.0f * (k * k - 1.0f) * norm;
  filterA2 = (1.0f - 1.41421356f * k + k * k) * norm;
}

static void IRAM_ATTR rcaDacTimerInterrupt() {
  uint8_t value = 128;
  uint16_t readIndex = rcaReadIndex;
  if (readIndex != rcaWriteIndex) {
    value = rcaSamples[readIndex];
    rcaReadIndex = (readIndex + 1) & RCA_BUFFER_MASK;
  }
  dac_output_voltage(DAC_CHAN_0, value);
}

static void rcaAudioStream(const uint8_t *data, uint32_t length) {
  const int16_t *samples = (const int16_t *)data;
  const uint32_t frameCount = length / 4;

  for (uint32_t i = 0; i < frameCount; i++) {
    float mono = ((int32_t)samples[i * 2] + samples[i * 2 + 1]) * 0.5f;
    float filtered = filterB0 * mono + filterB1 * filterX1 + filterB2 * filterX2
                   - filterA1 * filterY1 - filterA2 * filterY2;
    filterX2 = filterX1;
    filterX1 = mono;
    filterY2 = filterY1;
    filterY1 = filtered;

    resamplePhase += RCA_SAMPLE_RATE;
    if (resamplePhase >= audioSampleRate) {
      resamplePhase -= audioSampleRate;
      int output = 128 + (int)(filtered / 256.0f);
      if (output < 0) output = 0;
      if (output > 255) output = 255;

      uint16_t writeIndex = rcaWriteIndex;
      uint16_t nextIndex = (writeIndex + 1) & RCA_BUFFER_MASK;
      if (nextIndex != rcaReadIndex) {
        rcaSamples[writeIndex] = (uint8_t)output;
        rcaWriteIndex = nextIndex;
      }
    }
  }
}

static void audioSampleRateChanged(uint16_t sampleRate) {
  updateRcaFilter(sampleRate);
}

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

  updateRcaFilter(44100);
  dac_output_enable(DAC_CHAN_0);
  dac_output_voltage(DAC_CHAN_0, 128);
  rcaTimer = timerBegin(RCA_SAMPLE_RATE);
  if (rcaTimer != nullptr) {
    timerAttachInterrupt(rcaTimer, &rcaDacTimerInterrupt);
    timerAlarm(rcaTimer, 1, true, 0);
  }

  i2s.setPins(I2S_BCLK, I2S_LRC, I2S_DOUT);
  if (!i2s.begin(I2S_MODE_STD, 44100, I2S_DATA_BIT_WIDTH_16BIT,
                 I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
    while (true) delay(1000);
  }
  a2dp.set_on_connection_state_changed(connection_state_changed);
  a2dp.set_on_audio_state_changed(audio_state_changed);
  a2dp.set_avrc_metadata_callback(avrc_metadata_callback);
  a2dp.set_on_volumechange(volume_changed);
  a2dp.set_sample_rate_callback(audioSampleRateChanged);
  a2dp.set_stream_reader(rcaAudioStream, true);
  a2dp.start("ESP32 Music Player");
}

void loop() { pollCommands(); delay(2); }
