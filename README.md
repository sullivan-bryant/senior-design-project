# Directories

## ArduinoIDE
Use for uploading to an Arduino Nano ESP32 device via the Arduino IDE. Since external `.h` files are in the same directory, inclusion errors shouldn't occur. 

## PlatformIO
Use for uploading via PlatformIO. I use the PlatformIO extension for Visual Studio Code:

 1. Get PlatformIO IDE extension by PlatformIO,
 2. Enable extension, then open the PlatformIO folder from the extension's menu.

### Uploading Filesystem
A mini filesystem can be uploaded to the Nano containing all the files in the `data` directory. This is used for hosting the local website for controlling the board. The default `localhost` is `192.168.4.1`. 

#### Uploading FS Image via Arduino IDE
1. Prepare files in `data` directory, in same directory as `.ino` sketch
2. Go to __Menu > Tools > ESP32 Data Sketch Upload__
3. Verify via call to SPIFFS in regular code:
```cpp
void setup() {
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS mount failure.");
        esp_deep_sleep_start();
    }
}
```

#### Uploading FS Image via PlatformIO
The extension offers two ways for uploading the filesystem image via SPIFFS: through the terminal or from the command pallete.

Make sure to use spiffs build and minimum partitions in the `.ini` file:
```ini
; use esptool to flash filesystem image. won't work w/ regular DFU uploader
upload_protocol = esptool
board_build.filesystem = spiffs
board_build.partitions = min_spiffs.csv
board_build.flash_mode = dio
```
#
To upload via command pallete:

 1. `Ctrl+Shift+P` (or `Cmd+Shift+P`)
 2. Type and select: __PlatformIO: Upload File System Image__
    
#
To upload via terminal, run:
```bash
pio run --target uploadfs
```
