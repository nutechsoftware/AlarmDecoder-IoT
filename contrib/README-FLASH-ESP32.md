### Flashing ESP32 with AD2IoT firmware

#### Requirements
- ESP32 Flashing tool
  - https://www.espressif.com/en/support/download/other-tools
  - https://www.espressif.com/sites/default/files/tools/flash_download_tool_3.9.3.zip

- USB Drivers
  - Board specific drivers may be required programming.
    - ESP32-POE-ISO USB drivers
      - https://www.olimex.com/Products/IoT/ESP32/ESP32-POE-ISO/open-source-hardware


#### Using the ESP32 DOWNLOAD TOOL
  - Select "Developer mode"
  - Select "ESP32 DownloadTool"
  - Match settings to provided example screen capture ESP32-DOWNLOAD-TOOL-UPLOADING-FIREMWARE.png
  - Connect the ESP32 board via USB and be sure it registers as a com port in device manager.
  - Press start.

#### Configure the firmware operation
- Use Putty or other serial terminal program to connect to the COM port of the ESP32 board.
- Using the command line interface(CLI) to configure the device and confirm communication are correct to the AlarmDecoder hardware AD2pHat board or remote SER2SOCK connected AD2* device.