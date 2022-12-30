<!-- vscode-markdown-toc -->
* 1. [Overview](#overview)
* 2. [Tested and recommended hardware](#tested-and-recommended-hardware)
* 3. [Firmware](#firmware)
  * 3.1. [webUI build (webui) - alarmdecoder_webui_esp32.bin](#webui-build-(webui)---alarmdecoder_webui_esp32.bin)
  * 3.2. [SmartThings build (stsdk) - alarmdecoder_stsdk_esp32.bin](#smartthings-build-(stsdk)---alarmdecoder_stsdk_esp32.bin)
* 4. [Configuring the AD2IoT device](#configuring-the-ad2iot-device)
* 5. [AD2Iot CLI - command line interface](#ad2iot-cli---command-line-interface)
  * 5.1. [Basic commands](#basic-commands)
  * 5.2. [Ser2sock server component](#ser2sock-server-component)
    * 5.2.1. [Configuration for Ser2sock server](#configuration-for-ser2sock-server)
  * 5.3. [Web User Interface webUI component](#web-user-interface-webui-component)
    * 5.3.1. [Configuration for webUI server](#configuration-for-webui-server)
  * 5.4. [SmartThings Direct Connected device.](#smartthings-direct-connected-device.)
    * 5.4.1. [ Configuration for SmartThings IoT client](#-configuration-for-smartthings-iot-client)
  * 5.5. [Pushover.net notification component](#pushover.net-notification-component)
    * 5.5.1. [Configuration tool for Pushover.net notification](#configuration-tool-for-pushover.net-notification)
  * 5.6. [Twilio notification component](#twilio-notification-component)
    * 5.6.1. [Configuration for Twilio notifications](#configuration-for-twilio-notifications)
  * 5.7. [MQTT client component](#mqtt-client-component)
    * 5.7.1. [Configuration for MQTT message notifications](#configuration-for-mqtt-message-notifications)
  * 5.8. [FTP daemon component](#ftp-daemon-component)
    * 5.8.1. [Configuration for FTP server](#configuration-for-ftp-server)
* 6. [Building firmware](#building-firmware)
  * 6.1. [PlatformIO](#platformio)
    * 6.1.1. [TODO: Setup notes](#platformio-setup-notes)
  * 6.2. [SmartThings device SDK build environment](#smartthings-device-sdk-build-environment)
    * 6.2.1. [Setup build environment](#setup-build-environment)
    * 6.2.2. [Configure the project](#configure-the-project)
    * 6.2.3. [Build, Flash, and Run](#build,-flash,-and-run)
* 7. [Example Output from ESP32 USB serial console](#example-output-from-esp32-usb-serial-console)
* 8. [Troubleshooting](#troubleshooting)

<!-- vscode-markdown-toc-config
	numbering=true
	autoSave=true
	/vscode-markdown-toc-config -->
<!-- /vscode-markdown-toc -->
# AlarmDecoder IoT Network Appliance
 [Latest stable release ![Release Version](https://img.shields.io/github/release/nutechsoftware/AlarmDecoder-IoT.svg?style=plastic) ![Release Date](https://img.shields.io/github/release-date/nutechsoftware/AlarmDecoder-IoT.svg?style=plastic)](https://github.com/nutechsoftware/AlarmDecoder-IoT/releases/latest/) [![Travis (.org) branch](https://img.shields.io/travis/nutechsoftware/AlarmDecoder-IoT/master?style=plastic)](https://travis-ci.org/nutechsoftware/AlarmDecoder-IoT)

 [Latest development branch ![Development branch](https://img.shields.io/badge/dev-yellow?style=plastic) ![GitHub last commit (branch)](https://img.shields.io/github/last-commit/nutechsoftware/AlarmDecoder-IoT/dev?style=plastic)](https://github.com/nutechsoftware/AlarmDecoder-IoT/tree/dev) [![Travis (.org) branch](https://img.shields.io/travis/nutechsoftware/AlarmDecoder-IoT/dev?style=plastic)](https://travis-ci.org/nutechsoftware/AlarmDecoder-IoT)

##  1. <a name='overview'></a>Overview

This project provides an example framework for building an IoT network appliance to monitor and control one or many alarm systems.

The [AD2IoT network appliance](https://www.alarmdecoder.com/catalog/product_info.php/products_id/50) integrates the AD2pHAT from AlarmDecoder.com and a ESP32-POE-ISO. The result is a device that easily connects to a compatible alarm system and publishes the alarm state to a public or private MQTT server for monitoring. The device can also be configured to initiate SIP voice calls, text messages, or e-mail when a user defined alarm state is triggered. The typical time from the device firmware start to delivery of the first state event is less than 10 seconds. Typical latency between an alarm event and message delivery is 20ms on a local network.

The device firmware is stored in the onboard non-volatile flash making the device resistant to corruption. With OTA update support the flash can be securely loaded with the latest version over HTTPS.

##  2. <a name='tested-and-recommended-hardware'></a>Tested and recommended hardware
* ESP-POE-ISO. Ethernet+WiFi applications.
* ESP32-DevKitC-32E. WiFi only applications.

##  3. <a name='firmware'></a>Firmware
The firmware is already compiled for the ESP32-POE-ISO board and is available in the release page or via OTA(over-the-air) update. Currently the firmware is built with the following build flags 'stsdk' and 'webui'.

To switch to a specific build over the internet using OTA include the buildflag in the upgrade command.
- ```upgrade webui```

If the upgrade fails it may be the result of low memory on the device. Try disabling features restart the device and try again. Example. ```webui disable Y```. If all else fails install the latest release of the AD2IoT firmware over USB.

See the README-FLASH.MD inside the release file for instructions on flashing the firmware over the ESP32-POE-ISO USB port.

###  3.1. <a name='webui-build-(webui)---alarmdecoder_webui_esp32.bin'></a>webUI build (webui) - alarmdecoder_webui_esp32.bin
- Enabled components: Pushover, Twilio, Sendgrid, ser2sock, webUI, MQTT, ftpd.

This build uses the latest ESP-IDF v4.3 with the SmartThings driver is disabled.

- Optional uSD card with a FAT32 root partition is required for webUI.
  - Copy the contents of contrib/webUI/flash-drive folder into the root directory of the card.
  - Optionally place the sample configuration file [data/ad2iot.ini](data/ad2iot.ini) on the root directory to override the default config storage on the internal /spiffs partition.
  - Reboot the device after inserting the card for changes to take effect.

###  3.2. <a name='smartthings-build-(stsdk)---alarmdecoder_stsdk_esp32.bin'></a>SmartThings build (stsdk) - alarmdecoder_stsdk_esp32.bin
- Enabled components: Pushover, Twilio, Sendgrid, ser2sock, SmartThings.

This build is compiled using the [st-device-sdk-c-ref](https://github.com/SmartThingsCommunity/st-device-sdk-c-ref) from the SmartThings github repo and has the webUI component disabled.

A few options are available as the AD2IoT device moves toward being certified and directly available in the SmartThings app. In order to Discover and adopt the AD2IoT device it needs to be visible in the "My Testing Devices" under the "Adding device" panel.

First you will need to [enable Developer Mode in the app
](https://developer.samsung.com/smartthings/blog/en-us/2019/02/13/using-developer-mode-in-the-smartthings-app)

Next decide if you want to build your own profile and layout or join the existing AlarmDecoder profile for how this device will be shown in the SmartThings app.

- Join the ```Nu Tech Software Solutions, Inc.``` organization where the profiles are already working and in current development. Enroll in the organization using the Manufacturer ID '''0AOf'''. Once enrolled the device serial number and keys will be manually generated and sent to the same email address.
https://smartthings.developer.samsung.com/partner/enroll

- Use the SmartThings/Samsung developer workspace to create custom profiles and onboarding as described in this howto guide [How to build Direct Connected devices with the SmartThings Platform](https://community.smartthings.com/t/how-to-build-direct-connected-devices/204055). Generate a serial number and keys and register them in the management portal and configure the device with the validated keys.

##  4. <a name='configuring-the-ad2iot-device'></a>Configuring the AD2IoT device
Configuration of the AD2IoT is done directly over the USB serial port using a command line interface, or by editing the configuration file [ad2iot.ini](data/ad2iot.ini) on the internal spiffs partition using ftp over a local network or by placing a config file named [ad2iot.ini](data/ad2iot.ini) on an attached uSD card with a fat32 partition.

- Configuration using the command line interface.
  - Connect the AD2IoT ESP32 USB to a host computer use a USB A to USB Micro B cable and run a terminal program such as [Putty](https://www.putty.org/) or [Tiny Serial](http://brokestream.com/tinyserial.html) to connect to the USB com port using 115200 baud. Most Linux distributions already have the CH340 USB serial port driver installed.
  - If needed the drivers for different operating systems can be downloaded [here](https://www.olimex.com/Products/IoT/ESP32/ESP32-POE-ISO/open-source-hardware).
  - To save settings to the [ad2iot.ini](data/ad2iot.ini) use the ```restart``` command. This will save any settings changed in memory to the active configuration file before restarting to load the new settings.
- Configuration using the configuration file.
  - The ad2iot will first attempt to load the [ad2iot.ini](data/ad2iot.ini) config file from the first fat32 partition on a uSD card if attached. If this fails it will attempt to load the same file from the internal spiffs partition. If this fails the system will use defaults and save any changes on ```restart``` command to the internal spiffs partition in the file [ad2iot.ini](data/ad2iot.ini).
  - To access /sdcard/ad2iot.ini and /spiffs/ad2iot.ini files over the network enable the [FTPD component](#ftp-daemon-component). Use the custom FTP command ```REST``` to restart the ad2iot and force it to load the new configuration.
  - Sample config file with internal documentation can be found here [data/ad2iot.ini](data/ad2iot.ini)
  - Be sure to set the ftpd acl to only allow trusted systems to manage the files on the uSD card.

##  5. <a name='ad2iot-cli---command-line-interface'></a>AD2Iot CLI - command line interface
- Configure the initial AD2IoT device settings.
  - Select TTL GPIO pins or socket address and port for the AlarmDecoder protocol stream using ```one``` of the following commands.
    - TTL serial connection to AD2pHat board
      - ```ad2source COM 4:36```
    - Network shared AD2* device over ser2sock
      - ```ad2source SOCK 192.168.0.121:10000```
  - Configure the AlarmDecoder firmware settings for the the attached alarm system. For Ademco mode a free keypad address needs to be assigned to each partition to control. DSC mode is ZeroConf and only requires the mode 'D' and the partition # from 1-8.
    - Typical Ademco Vista setting: ```ad2config mode=A&address=18```
    - Typical DSC Power Series setting: ```ad2config mode=D&address=1```
  - Configure the default partition address and optional zones in partition 1.
    - ```partition 1 18 2,3,4,5```
  - Define any additional partitions and optional zones.
    - ```partition 2 22 6,7,9```
  - Set a default code in slot ```1``` to ARM/DISARM etc a partition.
    - ```code 1 4112```
- Choose the primary network operating mode of the AD2IoT device.
  - Managed network mode.
    - Enable and configure the WiFi or Ethernet networking driver.
      - Set ```'netmode'``` to ```W``` or ```E``` to enable the Wifi or Ethernet drivers and the ```<args>``` to configure the networking options such as IP address GW or DHCP and for Wifi the AP and password.
      - ```netmode E mode=d```
    - Enable ftp daemon and configure the ACL.
      - ```ftpd enable Y```
      - ```ftpd acl 192.168.1.0/24```
    - Enable webui daemon and configure the ACL.
      - ```webui enable Y```
      - ```webui acl 192.168.1.0/24```
      - Insert a uSD card formatted fat32 with the files in the ```/contrib/webUI/flash-drive/``` folder of this project in the root directory.
    - Restart for these changes to be saved and take effect.
      - ```restart```
    - Configure notifications

  - SmartThings Direct-connected device mode.```*stsdk firmware build only```
    - Disable networking to allow the SmartThings driver to manage the network hardware and allow adopting over 802.11 b/g/n 2.4ghz Wi-Fi.
      - ```netmode N```
    - Configure the default partition in slot 1 for the partition to connect to the SmartThings app.
      - ```partition 1 18```
    - Enable the SmartThings driver.
      - ```stenable Y```
    - Restart for these changes to take effect.
      - ```restart```
    - Configure the SmartThings security credentials provided by Nu Tech Software Solutions, Inc.
      - ```stserial AAABBbbcdde...```
      - ```stpublickey AAABBbbcdde...```
      - ```stprivatekey AAABBbbcdde...```
    - Restart the device and adopt the AD2IOT device under ```My Testing Devices``` in the SmartThings app.
      - ```restart```
    - Additional notification components will only work after the device is adopted and connected to the local Wi-Fi network.

###  5.1. <a name='basic-commands'></a>Main commands
- help
```console
Usage: help [command]
    You are here -> .
    Display information about builtin commands or list available commands
Options:
    command                 Specify the command for details on command usage
                            or leave blank for list of available commands
```
- logmode
```console
Usage: logmode [<mode>]
    Configuration tool for the ad2iot firmware log settings

Modes:
    I                       Informational
    V                       Verbose
    D                       Debugging
    N                       Warnings and errors only(default)
```
- restart
```console
Usage: restart
    Save config changes and restart the device
```
- factory-reset
```console
Usage: factory-reset
    Erase config storage and reboot to factory defaults
```
- top
```console
Usage: top
    Provides a dynamic real-time view of the running system
    Press any key to exit

Example:

top - 15:40:23.477 up 31 days TS: 2734823413319 Tasks: 14
Mem: 298328 total, 95508 free, 37876 min free

Name            ID  State Priority Stack CPU# Time                 %TBusy %Busy 
sys_evt           8 B           20  1048    0                 4646   0.00   0.00
AD2 ota check    15 B            0  1368    0             50118679   0.00   0.00
Tmr Svc           4 B            1  1480    0            343028056   0.01   0.03
IDLE              3 R            0  1820    0        2692433658498  98.45  96.25
AD2 cli           6 R            2  1932    0          10170914876   0.37   1.25
tiT               7 B           18  2364    0           1134852575   0.04   0.05
emac_rx           9 S           15  3236    0           1149697995   0.04   0.02
httpd            13 B            5  3340    0                  769   0.00   0.00
AD2 main         14 B            1  3468    0          11172565723   0.41   0.41
esp_timer         1 S           22  3672    0                29148   0.00   0.00
AD2 webUI        11 B            1  4192    0             48403756   0.00   0.00
AD2 GPIO COM RX   5 B            2  4288    0          18014970412   0.66   2.00
AD2 sendQ        10 B            1  5460    0            298731230   0.01   0.00
ftp daemon       12 B            1  7376    0                  615   0.00   0.00

   State legend
    'B'locked 'R'eady 'D'eleted 'S'uspended
   Column legend
    Stack: Minimum stack free bytes, CPU#: CPU affinity
    TBusy: % busy total, Busy: % busy now
```
- upgrade
```console
Usage: upgrade [buildflag]

    Preform an OTA upgrade now download and install new flash
Options:
    buildflag               Specify a different build or use current if omitted
                            See release page for details on available builds
```
- version
```console
Usage: version

    Report the current and available version
```
- netmode
```console
Usage: netmode [(N | W | E)] [<arg>]

    Configuration tool for ad2iot network settings

Options:
    N                       Disable(default) networking and allow
                            a component to take over networking
    W                       Enable WiFi core driver
    E                       Enable Ethernet core driver
    arg                     Config string to pass to network driver
                            Argument string name value pairs separated by &.
                            Keys: mode,ip,mask,gw,dns1,dns2,sid,password
Examples:
    WiFi DHCP with SID and password.
      ```netmode W mode=d&sid=example&password=somethingsecret```
    Ethernet DHCP DNS2 override.
      ```netmode E mode=d&dns2=4.2.2.2```
    Ethernet Static IPv4 address.
      ```netmode E mode=s&ip=192.168.1.111&mask=255.255.255.0&gw=192.168.1.1&dns1=4.2.2.2&dns2=8.8.8.8```
```
- switch
```console
Usage: switch <swid> [command] [<arg>]

    Configuration tool for ad2iot virtual switches

    This tool allows configuring ad2iot virtual switch that will change
     state based upon one more more filters and REGEX pattern matches.
    Up to 8 REGEX patterns for OPEN, CLOSE, and TROUBLE can be defined
    for complex state matching capabilities.

Commands:
    delete | -              Clear switch  settings
    default STATE           Default STATE [0]CLOSE(OFF) [1]OPEN(ON) [-1]UNKNOWN
    reset TIME              Auto rest TIME in ms 0 to disable
    types TYPE, TYPE,...    Message type filter list or blank to disable
    filter REGEX            Pre filter REGEX or blank to disable
    open IDX REGEX          OPEN event REGEX filter for IDX 1-8
    close IDX REGEX         CLOSE event REGEX filter for IDX 1-8
    trouble IDX REGEX       TROUBLE event REGEX filter for IDX 1-8
Options:
    switchId                ad2iot virtual switch ID 1-255
    IDX                     REGEX index 1-8 for multiple tests
    REGEX                   Regular expression or exact match string.
    TYPE                    Message types [ALPHA,LRR,REL,EXP,RFX,AUI,KPM,KPE,
                            CRC,VER,ERR,EVENT]

Common search verbs for type EVENT
      arm {STAY|AWAY}
      disarm
      power {AC|BATTERY}
      ready {ON|OFF}
      alarm {ON/OFF}
      fire {ON|OFF}
      chime {ON|OFF}
      exit {ON|OFF}
      programming {ON|OFF}
      zone {OPEN,CLOSE,TROUBLE} ZONE_NUMBER
```
- code
```console
Usage: code <codeId> [- | <value>]
    Configuration tool for alarm system codes

Options:
    codeId                  Code ID 1 - 128
    -                       Delete entry
    value                   Code string
```
- ad2term
```console
Usage: ad2term [reset]
    Connect terminal to AD2* device

    Connect directly to the AD2IoT or remote network connected AD2* device
    for management and diagnostics of the alarm system and AD2* settings.
    Note: This will halt processing events from the AD2* device.
    Note: To exit this mode and resume processing press '.' three times fast. ...

Options:
    reset                   Send hardware reboot to AD2pHat
```
- ad2config
```console
Usage: ad2config [<configString>]
    Configuration tool for AlarmDecoder hardware settings.

Options:
    configString            Name Value config string for the AlarmDecoder
                            device. Can be partial config.
                            Example set mode Ademco with default address 18.
                            ```ad2config mode=A&address=18```
```
- partition
```console
Usage: partition [(<partId> <address>) [zoneList]]
    Configuration tool for alarm panel partitions

    The AD2IoT and the attached AD2pHAT need information about the alarm system
    to function properly. This currently requires some redundant configuration.

    Use the ad2term command to connect to the AD2pHat and configure the firmware
    using the '!' or 'C' command.

    Set the MODE to DSC or Ademco and configure the devices primary address.

    For Ademco Vista panels:
     * This address must be programmed into the panel as an alpha keypad for
     each partition being managed.
     * It must not be used by any other devices.
    For DSC Power Series:
     * Use partition #1-8 and set the partition. no panel programming is needed.

Options:
    partId                  Partition ID 1-8
    address                 For DSC 1-8 for Ademco use available keypad address.
                             Use - to remove partition
    zoneList                Optional Comma separated zone list for this partition
Examples:
    Set default address mask to 18 for an Ademco system with zones 2, 3, and 4.
      ```partition 1 18 2,3,4```
    Set default send partition Id to 1 for a DSC system.
      ```partition 2 1```
    Show address for partition Id 2.
      ```partition 2```
    Remove partition Id 2.
      ```partition 2 -```
        Note: address - will remove an entry.
```
- zone
```console
Usage: zone <zoneId> [- | <value>]
    Configuration tool for zone json description strings

Options:
    zoneId                  Zone ID 1 - 255
    -                       Delete entry
    value                   json string with type and alpha attributes
                            {"type": "smoke", "alpha": "TESTING LAB SMOKE"}
```
- ad2source
```console
Usage: ad2source [(<mode> <arg>)]
    Manage AlarmDecoder protocol source

Options:
    mode                    Mode [S]ocket or [C]om port
    arg                     arg string
                              for COM use <TXPIN:RXPIN>
                              for SOCKET use <HOST:PORT>
Examples:
    Set source to ser2sock client at address and port.
      ```ad2source SOCK 192.168.1.2:10000```
    Set source to local attached uart with TX on GPIO 4 and RX on GPIO 36.
      ```ad2source COM 4:36```
```
###  5.2. <a name='ser2sock-server-component'></a>Ser2sock server component
Ser2sock allows sharing of a serial device over a TCP/IP network. It also supports encryption and authentication via OpenSSL. Typically configured for port 10000 several home automation systems are able to use this protocol to talk to the AlarmDecoder device for a raw stream of messages. Please be advised that network scanning of this port can lead to alarm faults. It is best to use the Access Control List feature to only allow specific hosts to communicate directly with the AD2* and the alarm panel.

####  5.2.1. <a name='configuration-for-ser2sock-server'></a>Configuration tool for Ser2sock server
```console
Usage: ser2sockd <command> [arg]

    Configuration tool for ser2sock server
Commands:
    enable [Y|N]            Set or get enable flag
    acl [aclString|-]       Set or get ACL CIDR CSV list use - to delete
Examples:
    ```ser2sockd enable Y```
    ```ser2sockd acl 192.168.0.0/28,192.168.1.0-192.168.1.10,192.168.3.4```
```

###  5.3. <a name='web-user-interface-webui-component'></a>Web User Interface webUI component
This component provides a simple HTML5+WebSocket user interface with realtime alarm status using push messages over WebSocket. Buttons for Arming, Disarming, Exiting, and sending panic events. Panic buttons require pressing the button three times in 5 seconds to help prevent false alarms.<br>
<img src="contrib/webUI/EXAMPLE-PANEL-READY.jpg" width="200">

####  5.3.1. <a name='configuration-for-webui-server'></a>Configuration tool for webUI server
```console
Usage: webui <command> [arg]

    Configuration tool for webUI server
Commands:
    enable [Y|N]            Set or get enable flag
    acl [aclString|-]       Set or get ACL CIDR CSV list
                            use - to delete
Examples:
    ```webui enable Y```
    ```webui acl 192.168.0.0/28,192.168.1.0-192.168.1.10,192.168.3.4```
```

###  5.4. <a name='smartthings-direct-connected-device.'></a>SmartThings Direct Connected device.
###### ```Only available in stsdk firmware build```
Direct-connected devices connect directly to the SmartThings cloud. The SDK for Direct Connected Devices is equipped to manage all MQTT topics and onboarding requirements, freeing you to focus on the actions and attributes of your device. To facilitate the development of device application in an original chipset SDK, the core device library and the examples were separated into two git repositories. That is, if you want to use the core device library in your original chipset SDK that installed before, you may simply link it to develop a device application in your existing development environment. For more info see https://github.com/SmartThingsCommunity/st-device-sdk-c-ref.

####  5.4.1. <a name='-configuration-for-smartthings-iot-client'></a> Configuration tool for SmartThings IoT client
- Enable SmartThings component *stsdk
  - ```stenable {bool}```
    - {bool}: [Y]es/[N]o
- Sets the SmartThings device_info serialNumber.
  - ```stserial {serialNumber}```
  - Example: ```stserial AaBbCcDdEeFfGg...```
- Sets the SmartThings device_info publicKey.
  - ```stpublickey {publicKey}```
  - Example: ```stpublickey AaBbCcDdEeFfGg...```
- Sets the SmartThings device_info privateKey.
  - ```stprivatekey {privateKey}```
  - Example: ```stprivatekey AaBbCcDdEeFfGg...```
- Cleanup NV data with restart option
  - ```stcleanup```

###  5.5. <a name='pushover.net-notification-component'></a>Pushover.net notification component
Pushover is a platform for sending and receiving push notifications. On the server side, they provide an HTTP API for queueing messages to deliver to devices addressable by User or Group Keys. On the device side, they offer iOS, Android, and Desktop clients to receive those push notifications, show them to the user, and store them for offline viewing. See: https://pushover.net/

####  5.5.1. <a name='configuration-tool-for-pushover.net-notification'></a>Configuration tool for Pushover.net notification
```console
Usage: pushover (apptoken|userkey) <acid> [<arg>]
Usage: pushover switch <swid> [delete|-|notify|open|close|trouble] [<arg>]

    Configuration tool for Pushover.net notification
Commands:
    apptoken acid [hash]    Application token/key HASH
    userkey acid [hash]     User Auth Token HASH
    switch swid SCMD [ARG]  Configure virtual switches
Sub-Commands:
    delete | -              Clear switch notification settings
    notify <acid>,...       List of accounts [1-8] to use for notification
    open <message>          Send <message> for OPEN events
    close <message>         Send <message> for CLOSE events
    trouble <message>       Send <message> for TROUBLE events
Options:
    acid                    Account storage location 1-8
    swid                    ad2iot virtual switch ID 1-255.
                            See ```switch``` command
    message                 Message to send for this notification
```
- Example cli commands to setup a complete virtual contact.
  - Send pushover notification from profiles in slot #1 and #2 when ad2iot virtual switch profile #1 on OPEN(ON), CLOSE(OFF) or TROUBLE REGEX patterns match.
  - ad2iot virtual switch settings for this example
    ```console
    [switch 1]
    default = 0
    reset = 0
    types = RFX
    filter = !RFX:0123456,.*
    open 1 = !RFX:0123456,1.......
    close 1 = !RFX:0123456,0.......
    trouble 1 = !RFX:0123456,......1.
    ```
    - CLI Commands
    ```console
    pushover apptoken 1 aabbccdd112233...
    pushover userkey 1 aabbccdd112233...
    pushover apptoken 2 aabbccdd112233...
    pushover userkey 2 aabbccdd112233...
    pushover switch 1 notify 1, 2
    pushover switch 1 open 5800 CONTACT SN#0123456 OPEN
    pushover switch 1 close 5800 CONTACT SN#0123456 CLOSED
    pushover switch 1 trouble 5800 CONTACT SN#0123456 TROUBLE
    ```
  - Resulting configure file changes(comments manually added later)
    ```console
    # Pushover config section
    [pushover]

    # Profile 1
    apptoken 1 = aabbccdd112233...
    userkey 1 = aabbccdd112233...

    # Profile 2
    apptoken 2 = aabbccdd112233...
    userkey 2 = aabbccdd112233...

    # Switch 1 notification settings
    switch 1 notify = 1, 2
    switch 1 open = 5800 CONTACT SN#0123456 OPEN
    switch 1 close = 5800 CONTACT SN#0123456 CLOSED
    switch 1 trouble = 5800 CONTACT SN#0123456 TROUBLE
    ```
###  5.6. <a name='twilio-notification-component'></a>Twilio notification component
Twilio (/ˈtwɪlioʊ/) is an American cloud communications platform as a service (CPaaS) company based in San Francisco, California. Twilio allows software developers to programmatically make and receive phone calls, send and receive text messages, reliably send email, and perform other communication functions using its web service APIs. See: https://www.twilio.com/

####  5.6.1. <a name='configuration-for-twilio-notifications'></a>Configuration tool for Twilio notifications
```console
Usage: twilio (disable|sid|token|from|to|type|format) <acid> [<arg>]
Usage: twilio switch <swid> [delete|-|notify|open|close|trouble] [<arg>]

    Configuration tool for Twilio + SendGrid notifications
Commands:
    disable acid [Y|N]      Disable notification account(acid)
    sid acid [hash]         Twilio String Identifier(SID)
    token acid [hash]       Twilio Auth Token
    from acid [address]     Validated Email or Phone #
    to acid [address]       Email or Phone #
    type acid [M|C|E]       Notification type Mail, Call, EMail
    format acid [format]    Output format string
    switch swid SCMD [ARG]  Configure switches
Sub-Commands: switch
    delete | -              Clear switch notification settings
    notify <acid>,...       List of accounts [1-999] to use for notification
    open <message>          Send <message> for OPEN events
    close <message>         Send <message> for CLOSE events
    trouble <message>       Send <message> for TROUBLE events
Options:
    acid                    Account storage location 1-999
    swid                    ad2iot virtual switch ID 1-255.
                            See ```switch``` command
    message                 Message to send for this notification
    address                 EMail or Phone # depending on type
    format                  Template format string
```
  - ad2iot virtual switch settings for this example
    ```console
    [switch 1]
    default = 0
    reset = 0
    types = RFX
    filter = !RFX:0123456,.*
    open 1 = !RFX:0123456,1.......
    close 1 = !RFX:0123456,0.......
    trouble 1 = !RFX:0123456,......1.

    [switch 2]
    default = 0
    reset = 0
    types = EVENT
    open 1 FIRE ON
    close 1 FIRE OFF

    [switch 3]
    default = 0
    reset = 0
    types = EVENT
    open 1 POWER AC
    close 1 POWER BATTERY
    ```
- Example cli commands to setup a complete virtual contact.
  - Configure notification profiles..
    ```console
    # Twilio config section
    [twilio]

    # Account ID #1 EMail using api.sendgrid.com
    sid 1 = NA
    token 1 = Abcdefg012345....
    from 1 = foo@example.com
    to 1 = bar@example.com
    type 1 = email
    format 1 = {}

    # Account ID #2 Text message using api.twilio.com
    sid 2 Abcdefg012345....
    token 2 = Abcdefg012345....
    from 2 = 15555551234
    to 2 = 15555551234
    type 2 = text
    format 2 = {}

    # Account ID #3 Voice Twiml call using api.twilio.com
    sid 3 = Abcdefg012345....
    token 3 = Abcdefg012345....
    from 3 = 15555551234
    to 3 = 15555551234
    type 3 = call
    format 3 = <Response><Pause length="3"/><Say>{0}</Say><Pause length="3"/><Say>{0}</Say><Pause length="3"/><Say>{0}</Say></Response>
    ```
  - Send notifications from profile in slot #0 for 5800 RF sensor with SN 0123456 and trigger on OPEN(ON), CLOSE(OFF) and TROUBLE REGEX patterns. In this example the Text or EMail sent would event contain the user defined message.
    ```console
    switch 1 notify = 0,1,2
    switch 1 open = RF SENSOR 0123456 OPEN
    switch 1 close = RF SENSOR 0123456 CLOSED
    switch 1 trouble = RF SENSOR 0123456 TROUBLE
    ```
  - Send notifications from profile in slot #2 in the example a Call profile when EVENT message "FIRE ON" or "FIRE OFF" are received. Use a Twiml string to define how the call is processed. This can include extensive external logic calling multiple people or just say something and hangup.
    ```console
    switch 2 notify = 2
    switch 2 open = FIRE ALARM ACTIVE
    switch 2 close = FIRE ALARM CLEAR
    ```
  - Send notifications from profile in slot #2 in the example a Call profile when EVENT message "POWER BATTERY" or "POWER AC" are received. Use a Twiml string to define how the call is processed. This can include extensive external logic calling multiple people or just say something and hangup.
    ```console
    switch 3 notify = 2
    switch 3 open = ON MAIN AC POWER
    switch 3 close = ON BATTERY BACKUP POWER
    ```
###  5.7. <a name='mqtt-client-component'></a>MQTT client component
MQTT is an OASIS standard messaging protocol for the Internet of Things (IoT). It is designed as an extremely lightweight publish/subscribe messaging transport that is ideal for connecting remote devices with a small code footprint and minimal network bandwidth. MQTT today is used in a wide variety of industries, such as automotive, manufacturing, telecommunications, oil and gas, etc. See: https://mqtt.org/

- Implementation notes:
  - Each client connects under the root topic of ```ad2iot``` with a sub topic level of the devices UUID.
    - Example: ```[{tprefix}/]ad2iot/41443245-4d42-4544-4410-XXXXXXXXXXXX```
  - Last Will and Testament (LWT) is used to indicate ```online```/```offline``` ```state``` of client using ```status``` topic below the device root topic.
    - Example: ```[{tprefix}/]ad2iot/41443245-4d42-4544-4410-XXXXXXXXXXXX/status = online```
  - Device specific info is in the ```info``` topic below the device root topic.
    - Example: ```[{tprefix}/]ad2iot/41443245-4d42-4544-4410-XXXXXXXXXXXX/info = {"firmware_version":"AD2IOT-1094","cpu_model":1,"cpu_revision":1,"cpu_cores":2,"cpu_features":["WiFi","BLE","BT"],"cpu_flash_size":4194304,"cpu_flash_type":"external","ad2_version_string":"08000002,V2.2a.8.9b-306,TX;RX;SM;VZ;RF;ZX;RE;AU;3X;CG;DD;MF;L2;KE;M2;CB;DS;ER;CR","ad2_config_string":"MODE=A&CONFIGBITS=ff05&ADDRESS=18&LRR=Y&COM=N&EXP=YYNNN&REL=YNNN&MASK=ffffffff&DEDUPLICATE=N"}```
  - Topic prefix when configrued with ```tprefix``` will prefix all publish topics with a specified path.
    - Example: Place ```ad2iot``` topic under the ```homeassistant``` topic.
      - ```tprefix homeassistant```
  - Auto Discovery topic ```dprefix``` will publish the alarm panel device config for each partition, zone, and sensor configured. See https://www.home-assistant.io/docs/mqtt/discovery/
    - Example: Place discovery topic under Home Assistant.
      - ```dprefix homeassistant```
  - Partition state tracking with minimal traffic only when state changes. Each configured partition will be under the ```partitions``` topic below the device root topic.
    - Example: ```ad2iot/41443245-4d42-4544-4410-XXXXXXXXXXXX/partitions/1 =
{"ready":false,"armed_away":false,"armed_stay":false,"backlight_on":false,"programming_mode":false,"zone_bypassed":false,"ac_power":true,"chime_on":false,"alarm_event_occurred":false,"alarm_sounding":false,"battery_low":true,"entry_delay_off":false,"fire_alarm":false,"system_issue":false,"perimeter_only":false,"exit_now":false,"system_specific":3,"beeps":0,"panel_type":"A","last_alpha_messages":"SYSTEM LO BAT                   ","last_numeric_messages":"008","event":"LOW BATTERY"}```
  - Custom virtual switches with user defined topics are under the ```switches``` below the device root topic.
    - Example: ```[{tprefix}/]ad2iot/41443245-4d42-4544-4410-XXXXXXXXXXXX/switches/1 = {"state":"ON"}```
  - Zone states by Zone ID(NNN) are under the ```zones``` below the device root topic.
    - Example: ```[{tprefix}/]ad2iot/41443245-4d42-4544-4410-XXXXXXXXXXXX/zones/3 = {"state":"CLOSE","partition":2,"name":"THIS IS ZONE 3"}```
  - Remote ```commands``` subscription. If enabled the device will subscribe to ```commands``` below the device root topic. Warning! Only enable if on a secure broker as codes will be visible to subscribers.
    - Publish JSON template ```{ "partition": {number}, "action": "{string}", "code": "{string}", "arg": "{string}"}```
    - Example: ```[{tprefix}/]ad2iot/41443245-4d42-4544-4410-XXXXXXXXXXXX/commands = {"partition": 1, "action": "DISARM", "code": "1234"}```
    - Example: ```[{tprefix}/]ad2iot/41443245-4d42-4544-4410-XXXXXXXXXXXX/commands = {"partition": 1, "action": "BYPASS", "code": "1234", "arg": "03"}```
    - Example: ```[{tprefix}/]ad2iot/41443245-4d42-4544-4410-XXXXXXXXXXXX/commands = {"partition": 1, "action": "FIRE_ALARM"}```
  - Contact ID reporting if found will be published to ```[{tprefix}/]ad2iot/41443245-4d42-4544-4410-XXXXXXXXXXXX/cid```
    - Example: ```{ "event_message": "!LRR:002,1,CID_3441,ff"}```

  - Home Assistant intigration.
    - Configure ```dprefix``` to ```homeassistant``` or the location you have configured HA to look for MQTT discovery topics.
    - Optional configure ```tprefix``` to ```homeassistant``` to force the ```ad2iot``` topic to be under the default HA mqtt topic.
    - MQTT Auto Discovery setup See https://www.home-assistant.io/docs/mqtt/discovery/
####  5.7.1. <a name='configuration-for-mqtt-message-notifications'></a>Configuration tool for MQTT message notifications
```console
Usage: mqtt (enable|url|commands|tprefix|dprefix) [<arg>]
Usage: mqtt switch <swid> [delete|-|open|close|trouble] [<arg>]

    Configuration tool for MQTT notification
Commands:
    enable [Y|N]            Component enable flag
    url [url]               MQTT URL string
    commands [Y|N]          Remote command enable flag
    tprefix [path]          Topic prefix
    dprefix [path]          Discovery prefix
    switch swid SCMD [ARG]  Configure virtual switches
Sub-Commands:
    delete | -              Clear switch notification settings
    description <jsonStr>   Send JSON description string for discovery
    open <message>          Send <message> for OPEN events
    close <message>         Send <message> for CLOSE events
    trouble <message>       Send <message> for TROUBLE events
Options:
    swid                    ad2iot virtual switch ID 1-255.
                            See ```switch``` command
    message                 Message to send for this notification
    jsonStr                 ex. {"name": "foo", "type":"door", "value_template": "{{value_json.state}"}
```
- Examples
  - Publish events to <device root>/switches/1 when the 5800 RF sensor with serial number 0123456 changes state.
  ```console
  ## ad2iot virtual switch 1 configuration.
  [switch 1]
  default = 0
  reset = 0
  types = RFX
  filter = !RFX:0123456,.*
  open 1 = !RFX:0123456,1.......
  close 1 = !RFX:0123456,0.......
  trouble 1 = !RFX:0123456,......1.

  # MQTT config settings section
  [mqtt]
  enable = true
  commands = true
  tprefix = homeassistant
  dprefix = homeassistant
  url = mqtt://user:pass@testmqtt.example.com/
  switch 1 notify = 1
  switch 1 description = { "name": "5800 Micra SN:0123456", "type": "door", "value_template": "{{value_json.state}}" }
  switch 1 open = ON
  switch 1 close = OFF
  switch 1 trouble = ON
  ```
###  5.8. <a name='ftp-daemon-component'></a>FTP daemon component
The File Transfer Protocol (FTP) is a standard communication protocol used for the transfer of computer files from a server to a client on a computer network. FTP is built on a client–server model architecture using separate control and data connections between the client and the server. FTP users may authenticate themselves with a clear-text sign-in protocol, normally in the form of a username and password, but can connect anonymously if the server is configured to allow it.

If enabled the daemon will allow update of files on the attached uSD card. This allows for update of HTML or configuration settings from a secure host on the local network. To secure the FTP daemon the ACL needs to be configured to allow limited access to this service.

This daemon only supports one command and control connection at a time. Be sure to disable or limit the client used to a single connection or the client may appear to stall or timeout.

- Implementation notes:
  - root folder contains two virtual paths.
    * /spiffs
      - The spiffs partition. spiffs does not allow sub folders and has limits on file names.
    * /sdcard
      - The first fat32 partition on a uSD card if available and connected
  - Basic commands to rename, remove, upload, and dowloand files on the spiffs partition and attached uSD.

####  5.8.1. <a name='configuration-for-ftp-server'></a>Configuration tool for ftp server
```console
Usage: ftpd <command> [arg]

    Configuration tool for FTP server
Commands:
    enable [Y|N]            Set or get enable flag
    acl [aclString|-]       Set or get ACL CIDR CSV list
                            use - to delete
Examples:
    ```ftpd enable Y```
    ```ftpd acl 192.168.0.0/28,192.168.1.0-192.168.1.10,192.168.3.4```
```

##  6. <a name='building-firmware'></a>Building firmware
###  6.1. <a name='platformio'></a>PlatformIO
####  6.1.1. <a name='platformio-setup-notes'></a>Open the project and use the platformio UI inside of vscode to build and flash. Select esp32dev or esp32-poe-iso tree and select Build to compile.
###  6.2. <a name='smartthings-device-sdk-build-environment'></a>SmartThings device SDK build environment
####  6.2.1. <a name='setup-build-environment'></a>Setup build environment
- Based on the instructions in the [SmartThings SDK for Direct connected devices for C](https://github.com/SmartThingsCommunity/st-device-sdk-c-ref) for setting up a build environment and [This community post](https://community.smartthings.com/t/how-to-build-direct-connected-devices/204055) to build the code inside of the stsdk c-ref build environment.
```
# Make the root esp folder.
cd ~
mkdir esp

# Install the xtensa esp32 compiler
cd ~/esp
wget https://dl.espressif.com/dl/xtensa-esp32-elf-linux64-1.22.0-80-g6c4433a-5.2.0.tar.gz
tar -xvf xtensa-esp32-elf-linux64-1.22.0-80-g6c4433a-5.2.0.tar.gz

# Get and install esp-idf toolchain v4.3.2
cd ~/esp
git clone -b v4.3.2 --recursive https://github.com/espressif/esp-idf.git
cd ~/esp/esp-idf
./install.sh

# Install st-device-sdk-c-ref master branch currently v1.7.5.
cd ~/esp
git clone https://github.com/SmartThingsCommunity/st-device-sdk-c-ref.git
cd st-device-sdk-c-ref
python setup.py esp32

## At the end will be prompted to set the environment for building by sourcing the exports.sh file created during setup of stsdk c-ref.
. ./export.sh

# Link our external AlarmDecoder-IoT project into the apps folder for st-device-sdk-c-ref.
ln -s ~/Code/AlarmDecoder-IoT ~/esp/st-device-sdk-c-ref/apps/esp32
```

####  6.2.2. <a name='configure-the-project'></a>Configure the project
Run menu config and enable/disable components. Each module will consume code space and memory so test with the ```top``` command to be sure resources are not being exausted.
```
cd ~/esp/st-device-sdk-c-ref
./build.py esp32 AlarmDecoder-IoT menuconfig
```

####  6.2.3. <a name='build,-flash,-and-run'></a>Build, Flash, and Run

Build the project and flash it to the board, then run monitor tool to view serial output:

```
cd ~/esp/st-device-sdk-c-ref
./build.sh esp32 AlarmDecoder-IoT build flash monitor -p /dev/ttyUSB0
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Prerequisites](https://github.com/SmartThingsCommunity/st-device-sdk-c-ref#prerequisites) for full steps to configure and use ESP-IDF and the STSDK to build this project.

##  7. <a name='example-output-from-esp32-usb-serial-console'></a>Example Output from ESP32 USB serial console


```
rst:0xc (SW_CPU_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:1760
load:0x40078000,len:12920
load:0x40080400,len:3604
entry 0x400805f0
I (432) cpu_start: Pro cpu up.
I (432) cpu_start: Single core mode
I (440) cpu_start: Pro cpu start user code
I (440) cpu_start: cpu freq: 160000000
I (440) cpu_start: Application information:
I (441) cpu_start: Project name:     alarmdecoder_stsdk_esp32
I (446) cpu_start: App version:      1.0.6p6-133-gc2cf6cd-dirty
I (452) cpu_start: Compile time:     Nov 20 2021 15:56:32
I (457) cpu_start: ELF file SHA256:  0123456789abcdef...
I (462) cpu_start: ESP-IDF:          4.3.0
I (466) heap_init: Initializing. RAM available for dynamic allocation:
I (472) heap_init: At 3FF80000 len 00002000 (8 KiB): RTCRAM
I (477) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (482) heap_init: At 3FFB8E58 len 000271A8 (156 KiB): DRAM
I (488) heap_init: At 3FFE0440 len 0001FBC0 (126 KiB): D/IRAM
I (493) heap_init: At 40078000 len 00008000 (32 KiB): IRAM
I (498) heap_init: At 400953A0 len 0000AC60 (43 KiB): IRAM
I (504) !IOT: Starting AlarmDecoder AD2IoT network appliance version (AD2IOT-1093) build flag (webui)

!IOT: N (517) ESP32 with 2 CPU cores, WiFi/BT/BLE, silicon revision 1, 4MB external flash
!IOT: N (527) Initialize NVS subsystem start. Done.
!IOT: N (547) NVS usage 17.51%. Count: UsedEntries = (331), FreeEntries = (1559), AllEntries = (1890)
!IOT: N (547) init partition slot 1 address 18 zones '2,5'
!IOT: N (557) init partition slot 2 address 23 zones '3,6'
!IOT: N (607) Mounting uSD card PASS.
!IOT: N (667) Initialize AD2 UART client using txpin(4) rxpin(36)
!IOT: N (667) Send '.' three times in the next 5 seconds to stop the init.
AD2IOT #
!IOT: N (5667) Starting main task.
!IOT: N (5667) 'netmode' set to 'E'.
!IOT: I (5667) UARTCLI: network TCP/IP stack init start
!IOT: I (5667) UARTCLI: network TCP/IP stack init finish
!IOT: I (5677) UARTCLI: ETH hardware init start
!IOT: I (5677) gpio: GPIO[12]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
!IOT: I (5697) system_api: Base MAC address is not set
!IOT: I (5697) system_api: read default base MAC address from EFUSE
!IOT: I (5717) esp_eth.netif.glue: 01:23:45:67:89:ab
!IOT: I (5717) esp_eth.netif.glue: ethernet attached to netif
!IOT: I (5717) UARTCLI: DHCP Mode selected
!IOT: I (5737) UARTCLI: Ethernet starting
!IOT: I (5737) UARTCLI: ETH hardware init finish
!IOT: N (5777) TWILIO: Init done. Found and configured 4 virtual switches.
!IOT: N (5797) PUSHOVER: Init done. Found and configured 0 virtual switches.
!IOT: N (5797) WEBUI: service disabled.
!IOT: N (5797) MQTT init UUID: 41443245-4d42-4544-4410-0123456778ab
!IOT: E (5807) TRANS_TCP: DNS lookup failed err=202 res=0x0
!IOT: E (5817) MQTT_CLIENT: Error transport connect
!IOT: N (5867) MQTT: Init done. Found and configured 0 virtual switches.
AD2IOT #
!IOT: I (9737) UARTCLI: Ethernet Link Up
!IOT: I (9737) UARTCLI: Ethernet HW Addr 01:23:45:67:89:ab
!IOT: N (10867) SER2SOCKD: Init done. Service starting.
!IOT: N (11667) NEW IP ADDRESS: if(eth) addr(fe80:0000:0000:0000:9af4:abff:fe20:f14b) mask() gw()
!IOT: I (11667) esp_netif_handlers: eth ip: 192.168.111.123, mask: 255.255.255.0, gw: 192.168.111.1
!IOT: N (11677) NEW IP ADDRESS: if(eth) addr(192.168.111.123) mask(255.255.255.0) gw(192.168.111.1)
!IOT: N (11687) AD2 State change: {"ready":false,"armed_away":false,"armed_stay":false,"backlight_on":false,"programming":false,"zone_bypassed":false,"ac_power":true,"chime_on":false,"alarm_event_occurred":false,"alarm_sounding":false,"battery_low":false,"entry_delay_off":false,"fire_alarm":false,"system_issue":false,"perimeter_only":false,"exit_now":false,"system_specific":3,"beeps":0,"panel_type":"A","last_alpha_message":"****DISARMED****Hit * for faults","last_numeric_messages":"008","mask":1073712748,"event":"READY"}
!IOT: N (11777) AD2 State change: {"ready":true,"armed_away":false,"armed_stay":false,"backlight_on":false,"programming":false,"zone_bypassed":false,"ac_power":true,"chime_on":false,"alarm_event_occurred":false,"alarm_sounding":false,"battery_low":false,"entry_delay_off":false,"fire_alarm":false,"system_issue":false,"perimeter_only":false,"exit_now":false,"system_specific":3,"beeps":0,"panel_type":"A","last_alpha_message":"****DISARMED****  Ready to Arm  ","last_numeric_messages":"008","mask":1073450604,"event":"READY"}
!IOT: N (12667) NEW IP ADDRESS: if(eth) addr(2001:0db8:85a3:0000:0000:8a2e:0370:7334) mask() gw()
AD2IOT #
AD2IOT # help
Available AD2IoT terminal commands
  [ser2sockd, twilio, pushover, webui, mqtt, ftpd,
   restart, netmode, switch, zone, code, partition,
   ad2source, ad2config, ad2term, logmode, factory-reset, top,
   help, upgrade, version]
Type help <command> for details on each command.

```

##  8. <a name='troubleshooting'></a>Troubleshooting
- Pressing ```.``` three times early in the ESP32 boot will prevent any startup tasks. This can help if the ESP32 is in a REBOOT CRASH LOOP. To exit this mode ```'restart'```
- If the AD2* is not communicating it may be stuck in a configuration mode or its configuration may have been corrupted during firmware update of the ESP32. If this happens you can directly connect to the AD2* over the UART or Socket by using the command ```'ad2term'```.

  Note) If the connection is a Socket it is currently necessary to have the ESP32 running and not halted at boot and connected with SmartThings for Wifi and networking to be active.

- Flashing the ESP32-DEV board fails with timeout. It seems many of the ESP32 boards need a 4-10uF cap as a buffer on the EN pin and ground. This seems to fix it very well. Repeated attempts to upload with timeouts usually works by pressing the EN button on the board a few times during upload.
