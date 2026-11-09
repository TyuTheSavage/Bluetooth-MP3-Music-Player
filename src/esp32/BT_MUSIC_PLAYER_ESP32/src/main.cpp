#include <Arduino.h>
// #include <AudioTools.h>
#include <BluetoothA2DPSink.h>

BluetoothA2DPSink a2dp_sink;

volatile uint32_t audio_bytes = 0;

void audio_callback(const uint8_t *data, uint32_t len) {
    (void)data;
    audio_bytes += len;
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    a2dp_sink.set_stream_reader(audio_callback, false);
    a2dp_sink.start("UNITBV ESP32 BT Player");

    Serial.println("Bluetooth ready!");
}

void loop() {
    static uint32_t last = 0;

    if (millis() - last >= 1000) {
        last = millis();

        Serial.printf("Audio received: %lu bytes\n",
                      (unsigned long)audio_bytes);
    }

    delay(10);
}