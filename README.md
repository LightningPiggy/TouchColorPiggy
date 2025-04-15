# TouchColorPiggy

Code for the Waveshare ESP32-S3-Touch-LCD-2 based LightningPiggy

See:

- https://www.waveshare.com/product/arduino/boards-kits/esp32/esp32-s3-touch-lcd-2.htm
- http://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-2

## Installation

- Copy the "libraries" contents into your ~/Arduino/libraries folder
- Open with Arduino IDE (tested with 2.3.5)
- Install arduino-esp32 support (tested with 3.2.0)
- Select "Tools" - "Board" - "ESP32" - "Waveshare ESP32-S3-Touch-LCD-2.1"
  - Tools -> CPU Frequency: 240Mhz
  - Tools -> Flash Mode: QIO 80Mhz
  - Tools -> Flash Frequency: 80Mhz
  - Tools -> Upload Speed: 921600
  - Tools -> Partition Scheme: 16MB flash
  - Tools -> Core Debug Level: Debug
  - Tools -> PSRAM: Enabled
  - Tools -> Port: /dev/ttyACM0
  - Tools -> USB Firmware MSC On Boot: Disabled
  - Tools -> Upload Mode: USB-OTG CDC (TinyUSB)
  - Tools -> USB Mode: Hardware CDC & JTAG
