# ESP32 ED060KD1 WiFi Uploader

PIO firmware for ESP32-S3-N16R8 + epdiy V7 driving ED060KD1.

The ED060KD1 driver is extracted from the working `mazha_epdiyV7` baseline and
keeps the accepted 16-level grayscale calibration table and GPIO46 panel power
enable.

Use:

1. Flash to the board.
2. Connect WiFi to the open AP `ED060KD1-WIFI`.
3. Open `http://192.168.4.1`.
4. Select an image, crop/scale/rotate it, adjust tone, process it, and upload.

Networking:

- AP mode is always available and has no password.
- STA mode can be configured from the web page and is stored in NVS.

Image slots:

- The firmware uses a custom 16 MB flash partition table.
- LittleFS starts at `0x310000` and has about 12.9 MB available.
- There are 12 image slots.
- Each slot stores one `1448 x 1072` packed 4bpp grayscale frame.
- Slots can be shown, overwritten, deleted, or used by the carousel.
- Carousel settings are stored in NVS and empty slots are skipped.

Upload format:

- Browser sends a `1448 x 1072` packed 4bpp grayscale frame.
- Pixel 0 is stored in the low nibble, pixel 1 in the high nibble.
- Firmware maps each 0-15 input level through the calibrated ED060KD1 LUT
  before drawing to the epdiy framebuffer.
