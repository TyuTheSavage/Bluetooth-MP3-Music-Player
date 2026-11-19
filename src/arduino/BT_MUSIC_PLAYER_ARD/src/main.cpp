#include <Arduino.h>
#include <SoftwareSerial.h>

SoftwareSerial esp(10, 11);

void setup()
{
    Serial.begin(115200);
    esp.begin(9600);
}

void loop()
{
    while (esp.available())
        Serial.write(esp.read());
}