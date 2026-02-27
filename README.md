# JC3248W535_AviPlayer_with_Touch_UI
The JC3248W535 is a great little ESP32 powered board that incudes a touch screen, audio, SD card slot and battery management. Here I describe how I managed to create an AviPlayer with a touch screen UI using ArduinoGFX.

<b>Prerequisites</b>

In the Boards menu, I recommend using the 3.0.2 version of the "esp32" by Espressif Systems.


In the Libraries menu, I recommend using the 1.4.9 version of the GFX Library for Arduino bu Moon On Our Nation.

The other libraries you will need to install manually:


https://github.com/pschatzmann/arduino-libhelix


https://github.com/arionik/avilib


https://github.com/esp-arduino-libs/ESP32_JPEG


https://github.com/me-processware/JC3248W535-Driver



Settings


Under the Arduino/Tools menu, you will need to select the correct settings:


Board:"ESP32S3 Dev Module"


Port: "<the correct COM port>"


USB CDC On Boot: "Enabled"


Flash Mode: "QIO 120 MHz"


Flash Size: "16MB (128Mb)"


Partition Scheme: "16M Flash (3MB APP/9.9MB FATFS)"


PSRAM: "OPI PSRAM"




