# Bluetooth-Music-Player
## This is a repository for a University (UNITBV) project made by students of IESC - CALC 4.

### About
This project evidentiates how an ESP32 and Arduino Uno Development Board can play music to external speakers.
The workflow is as follows: a phone or Bluetooth capable device connects to the Bluetooth module of the ESP, where the audio is redirected to the I2S amplifier, and also analogical via an ESP DAC towards an RCA output; in parallel, the ESP32 is connected to an Arduino Uno R3 that handles an LCD Screen, buttons and the rotary encoder that serves the purpose of a volume knob; with both inputs combined, the output from the ESP32 heads into the external amplifier and into its builtin DAC, and finally to the speaker/s, where music is played.

- Board             : Arduino Nano R3 & ESP32
- Bluetooth Module  : ESP32
- DAC               : ESP32
- Screen            : GM009605v4.3 OLED 0.5"W
- Encoder           : Rotary Digital
- Amplifier         : MAX98357A
- Speakers          : 3W 8ohm via I2S and MAX98357A & any active speaker capable of processing RCA input

## License

Copyright © 2026 UNITBV. All Rights Reserved.

This project is publicly available for viewing and
educational reference only.

Unauthorized copying, modification, distribution,
or use of the source code is prohibited.

See [LICENSE](LICENSE) for details.
