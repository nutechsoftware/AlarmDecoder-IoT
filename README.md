# AlarmDecoder SmartThings STSDK IoT Appliance
[Latest stable release ![Release Version](https://img.shields.io/github/release/nutechsoftware/AlarmDecoder-STSDK.svg?style=plastic) ![Release Date](https://img.shields.io/github/release-date/nutechsoftware/AlarmDecoder-STSDK.svg?style=plastic)](https://github.com/nutechsoftware/AlarmDecoder-STSDK/releases/latest/) [![Travis (.org) branch](https://img.shields.io/travis/nutechsoftware/AlarmDecoder-STSDK/master?style=plastic)](https://travis-ci.org/nutechsoftware/AlarmDecoder-STSDK)

[Latest development branch ![Development branch](https://img.shields.io/badge/dev-yellow?style=plastic) ![GitHub last commit (branch)](https://img.shields.io/github/last-commit/nutechsoftware/AlarmDecoder-STSDK/dev?style=plastic)](https://github.com/nutechsoftware/AlarmDecoder-STSDK/tree/dev) [![Travis (.org) branch](https://img.shields.io/travis/nutechsoftware/AlarmDecoder-STSDK/dev?style=plastic)](https://travis-ci.org/nutechsoftware/AlarmDecoder-STSDK)

## Overview

This project provides a framework for building a network IoT appliance to monitor and control alarm panel(s) using the AD2pHAT from AlarmDecoder.com and ESP32-DevKitC-32E or compatible ESP32 development board.

## Hardware Required

To run this example, it's recommended that you have a ESP32-DevKitC-32E or similar ESP32 based board. It is recommended to use the latest ESP32 V3 chips as they have improved encryption and firmware security features.

Using an Olimex ESP32-EVB-EA it takes 6 seconds :) after booting to connect to the network and start sending messages.


### Firmware
TODO: HOWTO flash the latest available firmware or compile this project from source. [Building firmware](#Building-firmware)


## New SmartThings app integration
A few options are available as the AD2IoT device moves toward being certified and directly available in the SmartThings app. In order to Discover and adopt the AD2IoT device it needs to be visible in the "My Testing Devices" under the "Adding device" panel.

First you will need to [enable Developer Mode in the app
](https://developer.samsung.com/smartthings/blog/en-us/2019/02/13/using-developer-mode-in-the-smartthings-app)

Next decide if you want to build your own profile and layout or join the existing AlarmDecoder profile for how this device will be shown in the SmartThings app.

- Join the AlarmDecoder organization where the profiles are already working and in current development. Enroll in the  AlarmDecoder organization using the Manufacturer ID '''0AOf'''
https://smartthings.developer.samsung.com/partner/enroll

- Use the SmartThings developer workspace to create custom profiles and onboarding as described in this howto guide [How to build Direct Connected devices with the SmartThings Platform](https://community.smartthings.com/t/how-to-build-direct-connected-devices/204055)


## Configure the AD2IoT device.
 - Connect the ESP32 development board USB to a host computer and run a terminal program to connect to the virtual com port.
 - Configure the AD2* Source cli command **'ad2source'**

Choose one of the following configurations.
 - SmartThings STSDK mode
   - Set ```'netmode'``` to N to allow STSDK to manage the network hardware.
   - Enable the STSDK module **'stenable'**
   - Configure the SmartThings security credentials
  **'stserial'** **'stpublickey'** **'stprivatekey'**
    - Add the credentials to the SmartThings developer workspace.
 - Managed networking mode.
   - Set ```'netmode'``` to ```W``` or ```E``` to enable the Wifi or Ethernet drivers and the ```<args>``` to configure the networking options such as IP address GW or DHCP and for Wifi the AP and password as described in the ```netmode``` command help.


## AD2Iot CLI command reference
### Base commands
  - Show the list of commands or give more detail on a specific command.

    ```help [command]```

  - Reboot the device.

    ```reboot```

  - Preform an OTA upgrade now download and install new flash.

    ```upgrade```

  - Report the current and available version.

    ```version```

  - Manage network connection type.

    ```netmode {mode} [args]```

    - {mode}
      - [N]one: (default) Do not enable any network let component(s) manage the networking.
      - [W]iFi: Enable WiFi network driver.
      - [E]thernet: Enable ethernet network driver.
    - [arg]
      - Argument string name value pairs sepearted by &.
        - Keys: MODE,IP,MASK,GW,DNS1,DNS2,SID,PASSWORD

    Examples
      - WiFi DHCP with SID and password.
        - netmode W mode=d&sid=example&password=somethingsecret
      - Ethernet DHCP DNS2 override.
        - netmode E mode=d&dns2=4.2.2.2
      - Ethernet Static IPv4 address
        - netmode E mode=s&ip=192.168.1.111&mask=255.255.255.0&gw=192.168.1.1&dns1=4.2.2.2&dns2=8.8.8.8

  - Simulate a button press event.

    ```button {count} {type}```

    - {count}
      - Number of times the button was pushed.
    - {type}
      - The type of event 'short' or 'long'.

    Examples
      - Send a single LONG button press.
        - button 1 long

  - Manage user codes.

    ```code {id} [value]```

    - {id}
      - Index of code to evaluate. 0 is default.
    - [value]
      - A valid alarm code or -1 to remove.

    Examples
      - Set default code to 1234
        - code 0 1234
      - Set alarm code for slot 1
        - code 1 1234
      - Show code in slot #3
        - code 3
      - Remove code for slot 2
        - code 2 -1

      Note: value -1 will remove an entry.

  - Connect directly to the AD2* source and halt processing.

    ```ad2term```

    Note: To exit press ... three times fast.

  - Manage virtual partitions

    ```vpaddr {id} {value}```

    - {id}
      - The virtual partition ID. 0 is the default.
    - [value]
      - (Ademco)Keypad address or (DSC)Partion #. -1 to delete.

    Examples
      - Set default address mask to 18 for an Ademco system.
        - vpaddr 0 18
      - Set default send partition to 1 for a DSC system.
        - vpaddr 0 1
      - Show address for partition 2
        - vpaddr 2
      - Remove virtual partition in slot 2
        - vpaddr 2 -1

      Note: address -1 will remove an entry.

  - Manage AlarmDecoder protocol source.

    ```ad2source [{mode} {arg}]```

    - {mode}
      - [S]ocket: Use ser2sock server over tcp for AD2* messages.
      - [C]om port: Use local UART for AD2* messages.
    - {arg}
      - [S]ocket arg: {HOST:PORT}
      - [C]om arg: {TXPIN:RXPIN}.

    Examples:
      - Show current mode
        - ad2source
      - Set source to ser2sock client at address and port
        - ad2source SOCK 192.168.1.2:10000
      - Set source to local attached uart with TX on GPIP 17 and RX on GPIO 16
        - ad2source COM 17:16

### SmartThings STSDK IoT commands
  - Enable SmartThings component

    ```stenable {bool}```

    - {bool}: [Y]es/[N]o

  - Cleanup NV data with reboot option

    ```stcleanup```

  - Sets the SmartThings device_info serialNumber.

    ```stserial {serialNumber}```

    Example: stserial AaBbCcDdEeFfGg...

  - Sets the SmartThings device_info privateKey.

    ```stprivatekey {privateKey}```

    Example: stprivatekey AaBbCcDdEeFfGg...

  - Sets the SmartThings device_info publicKey.

    ```stpublickey {publicKey}```

    Example: stpublickey AaBbCcDdEeFfGg...

### Twilio notification commands
  - Sets the 'Account SID' for notification.

    ```twsid {slot} {hash}```

    - {slot}: [N]
      - For default use 0. Support multiple Twilio accounts.
    - {hash}: Twilio 'Account SID'

    Example: twsid 0 aabbccdd112233..

  - Sets the 'User Auth Token' for notifications.

    ```twtoken {slot} {hash}```

    - {slot}: [N]
      - For default use 0. Support multiple Twilio accounts.
    - {hash}: Twilio 'User Auth Token'

    Example: twtoken 0 aabbccdd112233..

  - Sets the 'Type' for notification.

    ```twtype {slot} {type}```

    - {slot}
      - For default use 0. Support multiple Twilio accounts.
    - {type}: [M|T]
      - Notification type [M]essage or [T]wiml.

    Example: twtype 0 M

  - Sets the 'From' address for notification.

    ```twfrom {slot} {phone} ```

    - {slot}: [N]
      - For default use 0. Support multiple Twilio accounts.
    - {phone}: [NXXXYYYZZZZ]
      - From phone #

    Example: twfrom 0 13115552368

  - Sets the 'To' address for notification.

    ```twto {slot} {phone}```

    - {slot}: [N]
      - For default use 0. Support multiple Twilio accounts.
    - {phone}: [NXXXYYYZZZZ]
      - To phone #

    Example: twto 0 13115552368


## Building firmware
### Setup build environment
- Follow the instructions in the [SmartThings SDK for Direct connected devices for C](https://github.com/SmartThingsCommunity/st-device-sdk-c-ref) project for setting up a build environment. Confirm you can build the switch_example before continuing.
- Select the esp32_v3.3 build environment.
```
./setup.py esp32_v3.3
```

- Place the contents of this his project in ```st-device-sdk-c-ref/apps/esp32/```

### Configure the project

```
./build.sh esp32_v3.3 AlarmDecoder-STSDK menuconfig
```

### Build, Flash, and Run

Build the project and flash it to the board, then run monitor tool to view serial output:

```
./build.sh esp32 AlarmDecoder-STSDK build flash monitor -p /dev/ttyUSB0
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Prerequisites](https://github.com/SmartThingsCommunity/st-device-sdk-c-ref#prerequisites) for full steps to configure and use ESP-IDF and the STSDK to build this project.

## Example Output from ESP32 USB serial console.

```
I (131082) AD2API: !DBG: SIZE(1) PID(1) MASK(FFFFFF7F) Ready(1) Armed[Away(0) Home(0)] Bypassed(0) Exit(0)
I (131082) AD2_IoT: MESSAGE_CB: '[10000001100000003A--],008,[f72600ff1008001c28020000000000]," DISARMED CHIME   Ready to Arm  "'
I (140912) AD2API: !DBG: SIZE(1) PID(1) MASK(FFFFFF7F) Ready(1) Armed[Away(0) Home(0)] Bypassed(0) Exit(0)
I (140912) AD2_IoT: MESSAGE_CB: '[10000001100000003A--],008,[f72600ff1008001c28020000000000]," DISARMED CHIME   Ready to Arm  "'
I (146872) AD2API: !DBG: SIZE(1) PID(1) MASK(FFFFFF7F) Ready(1) Armed[Away(0) Home(0)] Bypassed(0) Exit(0)
I (146872) AD2_IoT: MESSAGE_CB: '[10010101000000003A--],008,[f72600ff1008011c08020000000000],"****DISARMED****  Ready to Arm  "'
I (146892) AD2_IoT: ON_CHIME_CHANGE: CHIME(0)
Sequence number return : 8
I (146892) [IoT]: _publish_event(958) > publish event, topic : /v1/deviceEvents/uuid, payload :
{"deviceEvents":[{"component":"chime","capability":"contactSensor","attribute":"contact","value":"open","providerData":{"sequenceNumber":8,"timestamp":"1599094428182"}}]}
I (152482) AD2API: !DBG: SIZE(1) PID(1) MASK(FFFFFF7F) Ready(0) Armed[Away(0) Home(1)] Bypassed(0) Exit(1)
I (152482) AD2_IoT: MESSAGE_CB: '[00110301000000003A--],008,[f72600ff1008038c08020000000000],"ARMED ***STAY***You may exit now"'
I (152492) AD2_IoT: ON_READY_CHANGE: READY(0) EXIT(1) HOME(1) AWAY(0)
I (152502) AD2_IoT: ON_ARM: READY(0) EXIT(1) HOME(1) AWAY(0)
Sequence number return : 9
I (152512) [IoT]: _publish_event(958) > publish event, topic : /v1/deviceEvents/uuid, payload :
{"deviceEvents":[{"component":"main","capability":"securitySystem","attribute":"securitySystemStatus","value":"armedStay","providerData":{"sequenceNumber":9,"timestamp":"1599094433795"}}]}
I (157122) AD2_IoT: LRR_CB: !LRR:002,1,CID_3441,ff
I (162212) AD2API: !DBG: SIZE(1) PID(1) MASK(FFFFFF7F) Ready(0) Armed[Away(0) Home(1)] Bypassed(0) Exit(1)
I (162212) AD2_IoT: MESSAGE_CB: '[00110001000000003A--],008,[f72600ff1008008c08020000000000],"ARMED ***STAY***You may exit now"'
I (172142) AD2API: !DBG: SIZE(1) PID(1) MASK(FFFFFF7F) Ready(0) Armed[Away(0) Home(1)] Bypassed(0) Exit(1)
I (172142) AD2_IoT: MESSAGE_CB: '[00110001000000003A--],008,[f72600ff1008008c08020000000000],"ARMED ***STAY***You may exit now"'
I (182132) AD2API: !DBG: SIZE(1) PID(1) MASK(FFFFFF7F) Ready(0) Armed[Away(0) Home(1)] Bypassed(0) Exit(1)
I (182132) AD2_IoT: MESSAGE_CB: '[00110001000000003A--],008,[f72600ff1008008c08020000000000],"ARMED ***STAY***You may exit now"'
I (191902) AD2API: !DBG: SIZE(1) PID(1) MASK(FFFFFF7F) Ready(0) Armed[Away(0) Home(1)] Bypassed(0) Exit(1)
I (191902) AD2_IoT: MESSAGE_CB: '[00110001000000003A--],008,[f72600ff1008008c08020000000000],"ARMED ***STAY***You may exit now"'
I (201882) AD2API: !DBG: SIZE(1) PID(1) MASK(FFFFFF7F) Ready(0) Armed[Away(0) Home(1)] Bypassed(0) Exit(1)
I (201882) AD2_IoT: MESSAGE_CB: '[00110001000000003A--],008,[f72600ff1008008c08020000000000],"ARMED ***STAY***You may exit now"'
I (202372) AD2API: !DBG: SIZE(1) PID(1) MASK(FFFFFF7F) Ready(0) Armed[Away(0) Home(1)] Bypassed(0) Exit(1)
I (202372) AD2_IoT: MESSAGE_CB: '[00110001000000003A--],008,[f72600ff1008008c08020000000000],"ARMED ***STAY***You may exit now"'
I (211782) AD2API: !DBG: SIZE(1) PID(1) MASK(FFFFFF7F) Ready(0) Armed[Away(0) Home(1)] Bypassed(0) Exit(1)
I (211782) AD2_IoT: MESSAGE_CB: '[00110001000000003A--],008,[f72600ff1008008c08020000000000],"ARMED ***STAY***You may exit now"'
I (212622) AD2API: !DBG: SIZE(1) PID(1) MASK(FFFFFF7F) Ready(0) Armed[Away(0) Home(1)] Bypassed(0) Exit(0)
I (212622) AD2_IoT: MESSAGE_CB: '[00100001000000003A--],008,[f72600ff1008008c08020000000000],"ARMED ***STAY***                "'
I (212642) AD2_IoT: ON_READY_CHANGE: READY(0) EXIT(0) HOME(1) AWAY(0)
I (222122) AD2API: !DBG: SIZE(1) PID(1) MASK(FFFFFF7F) Ready(0) Armed[Away(0) Home(1)] Bypassed(0) Exit(0)
I (222122) AD2_IoT: MESSAGE_CB: '[00100001000000003A--],008,[f72600ff1008008c08020000000000],"ARMED ***STAY***                "'
I (232122) AD2API: !DBG: SIZE(1) PID(1) MASK(FFFFFF7F) Ready(0) Armed[Away(0) Home(1)] Bypassed(0) Exit(0)
I (232122) AD2_IoT: MESSAGE_CB: '[00100001000000003A--],008,[f72600ff1008008c08020000000000],"ARMED ***STAY***                "'
I (241882) AD2API: !DBG: SIZE(1) PID(1) MASK(FFFFFF7F) Ready(0) Armed[Away(0) Home(1)] Bypassed(0) Exit(0)
I (241882) AD2_IoT: MESSAGE_CB: '[00100001000000003A--],008,[f72600ff1008008c08020000000000],"ARMED ***STAY***                "'
I (244122) AD2API: !DBG: SIZE(1) PID(1) MASK(FFFFFF7F) Ready(1) Armed[Away(0) Home(0)] Bypassed(0) Exit(0)
I (244122) AD2_IoT: MESSAGE_CB: '[10010101000000003A--],008,[f72600ff1008011c08020000000000],"****DISARMED****  Ready to Arm  "'
I (244142) AD2_IoT: ON_READY_CHANGE: READY(1) EXIT(0) HOME(0) AWAY(0)
I (244142) AD2_IoT: ON_DISARM: READY(1)
Sequence number return : 10
I (244152) [IoT]: _publish_event(958) > publish event, topic : /v1/deviceEvents/uuid, payload :
{"deviceEvents":[{"component":"main","capability":"securitySystem","attribute":"securitySystemStatus","value":"disarmed","providerData":{"sequenceNumber":10,"timestamp":"1599094525439"}}]}
I (247202) AD2API: !DBG: SIZE(1) PID(1) MASK(FFFFFF7F) Ready(1) Armed[Away(0) Home(0)] Bypassed(0) Exit(0)
I (247202) AD2_IoT: MESSAGE_CB: '[10010001000000003A--],008,[f72600ff1008001c08020000000000],"****DISARMED****  Ready to Arm  "'
I (248122) AD2_IoT: LRR_CB: !LRR:002,1,CID_1441,ff
I (257132) AD2API: !DBG: SIZE(1) PID(1) MASK(FFFFFF7F) Ready(1) Armed[Away(0) Home(0)] Bypassed(0) Exit(0)
I (257132) AD2_IoT: MESSAGE_CB: '[10010001000000003A--],008,[f72600ff1008001c08020000000000],"****DISARMED****  Ready to Arm  "'
I (266972) AD2API: !DBG: SIZE(1) PID(1) MASK(FFFFFF7F) Ready(1) Armed[Away(0) Home(0)] Bypassed(0) Exit(0)
I (266972) AD2_IoT: MESSAGE_CB: '[10010001000000003A--],008,[f72600ff1008001c08020000000000],"****DISARMED****  Ready to Arm  "'

```

## Troubleshooting
- Pressing ENTER early in the ESP32 boot will prevent any startup tasks. This can help if the ESP32 is in a REBOOT CRASH LOOP. To exit this mode ```'reboot'```
- If the AD2* is not communicating it may be stuck in a configuration mode or its configuration may have been corrupted during firmware update of the ESP32. If this happens you can directly connect to the AD2* over the UART or Socket by using the command ```'ad2term'```.

  Note) If the connection is a Socket it is currently necessary to have the ESP32 running and not halted at boot and connected with SmartThings for Wifi and networking to be active.

- Flashing the ESP32 board fails with timeout. It seems many of the ESP32 boards need a 4-10uF cap as a buffer on the EN pin and ground. This seems to fix it very well. Repeated attempts to upload with timeouts usually works by pressing the EN button on the board a few times during upload.