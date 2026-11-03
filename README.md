# STM32-Bluetooth-Music-Player
## This is a repository for a University (UNITBV) project made by students of IESC - CALC 4.

### About
This project evidentiates how an STM32 Development Board can, with the help of a Bluetooth Module, play music to external speakers.
The workflow is as follows: a phone or Bluetooth capable device connects to the Bluetooth module capable of I2S; the module then connects to the ESP32 where audio processing happens; in parallel, the ESP32 is connected to an Arduino Uno R3 that handles an LCD Screen, buttons and the rotary encoder that serves the purpose of a volume knob; with both inputs combined, the output from the ESP32 heads into the external amplifier and into its builtin DAC, and finally to the speaker, where music is played.

- Board             : Arduino Nano R3 & ESP32
- Bluetooth Module  : ESP32
- Screen            : LCD 0.5"
- Encoder           : Rotary Digital
- Amplifier         : -
- Speakers          : 3W 8ohm

## License

Copyright © 2026 UNITBV. All Rights Reserved.

This project is publicly available for viewing and
educational reference only.

Unauthorized copying, modification, distribution,
or use of the source code is prohibited.

See [LICENSE](LICENSE) for details.
