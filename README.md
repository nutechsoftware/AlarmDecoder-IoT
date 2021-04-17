# AlarmDecoder IoT Appliance with SmartThings STSDK
[Latest stable release ![Release Version](https://img.shields.io/github/release/nutechsoftware/AlarmDecoder-STSDK.svg?style=plastic) ![Release Date](https://img.shields.io/github/release-date/nutechsoftware/AlarmDecoder-STSDK.svg?style=plastic)](https://github.com/nutechsoftware/AlarmDecoder-STSDK/releases/latest/) [![Travis (.org) branch](https://img.shields.io/travis/nutechsoftware/AlarmDecoder-STSDK/master?style=plastic)](https://travis-ci.org/nutechsoftware/AlarmDecoder-STSDK)

[Latest development branch ![Development branch](https://img.shields.io/badge/dev-yellow?style=plastic) ![GitHub last commit (branch)](https://img.shields.io/github/last-commit/nutechsoftware/AlarmDecoder-STSDK/dev?style=plastic)](https://github.com/nutechsoftware/AlarmDecoder-STSDK/tree/dev) [![Travis (.org) branch](https://img.shields.io/travis/nutechsoftware/AlarmDecoder-STSDK/dev?style=plastic)](https://travis-ci.org/nutechsoftware/AlarmDecoder-STSDK)

## Overview

This project provides a framework for building an IoT network appliance to monitor and control one or many alarm systems.

By integrating the AD2pHAT from AlarmDecoder.com and a compatible ESP32 SoC based board the resulting device connects a compatible alarm system to a public or private MQTT server for monitoring. The device can also be configured to initiate SIP voice calls, SMS messages, or e-mail when a user defined alarm state is triggered. The typical time from the device firmware start to being delivery of a state event is less than 10 seconds. Typical latency between an alarm event and message delivery is 20ms on a local network.

The device firmware is stored in the onboard non-volatile flash making the device resistant to corruption. With OTA update support the flash can be securely loaded with the latest version in just a few seconds over HTTPS.

## Tested and recommended hardware
* ESP32-DevKitC-32E. WiFi only applications.
* ESP-POE-ISO. Ethernet+WiFi applications.

## Firmware
TODO: HOWTO flash the latest available firmware or compile this project from source. [Building firmware](#Building-firmware)


## SmartThings app integration
A few options are available as the AD2IoT device moves toward being certified and directly available in the SmartThings app. In order to Discover and adopt the AD2IoT device it needs to be visible in the "My Testing Devices" under the "Adding device" panel.

First you will need to [enable Developer Mode in the app
](https://developer.samsung.com/smartthings/blog/en-us/2019/02/13/using-developer-mode-in-the-smartthings-app)

Next decide if you want to build your own profile and layout or join the existing AlarmDecoder profile for how this device will be shown in the SmartThings app.

- Join the AlarmDecoder organization where the profiles are already working and in current development. Enroll in the  AlarmDecoder organization using the Manufacturer ID '''0AOf'''
https://smartthings.developer.samsung.com/partner/enroll

- Use the SmartThings/Samsung developer workspace to create custom profiles and onboarding as described in this howto guide [How to build Direct Connected devices with the SmartThings Platform](https://community.smartthings.com/t/how-to-build-direct-connected-devices/204055)


## Configure the AD2IoT device.
 - Connect the ESP32 development board USB to a host computer and run a terminal program to connect to the virtual com port using 115200 baud.
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

Configure the notifications using the notification components CLI commands.

## AD2Iot CLI command reference
### Base commands
  - Show the list of commands or give more detail on a specific command.

    ```help [command]```

  - Set logging mode.

    ```logmode {level}```

    - {level}
         [I] - Informational.
         [V] - Verbose.
         [D] - Debugging. (Only if compiled with DEBUG).
         [N] - None: (default) Warnings and errors only.
    Examples:
      - Set logging mode to INFO.
        - logmode I

  - Restart the device.

    ```restart```

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
      - Ethernet Static IPv4 address.
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
      - Set default code to 1234.
        - code 0 1234
      - Set alarm code for slot 1.
        - code 1 1234
      - Show code in slot #3.
        - code 3
      - Remove code for slot 2.
        - code 2 -1

      Note: value -1 will remove an entry.

  - Connect directly to the AD2* source and halt processing with option to hard reset AD2pHat.

    ```ad2term [reset]```

    Note: To exit send a period ```.``` three times fast.

  - Manage virtual partitions.

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
      - Show address for partition 2.
        - vpaddr 2
      - Remove virtual partition in slot 2.
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
      - Show current mode.
        - ad2source
      - Set source to ser2sock client at address and port.
        - ad2source SOCK 192.168.1.2:10000
      - Set source to local attached uart with TX on GPIO 17 and RX on GPIO 16.
        - ad2source COM 17:16

### ser2sock daemon command and sub commands
  - ser2sock daemon component command
    ```ser2sockd {sub command} {arg}```
    - {sub command}
      - [enable] Enable / Disable ser2sock daemon
        - {arg1}: [Y]es [N]o
          - [N] Default state
          - Example: ser2sockd enable Y
      - [acl] Set / Get ACL list
        - {arg1}: ACL LIST
          -  String of CIDR values seperated by commas.
          - Default: Empty string disables ACL list
          - Example: ser2sockd acl 192.168.0.123/32,192.168.1.0/24

### SmartThings STSDK IoT commands
  - Enable SmartThings component

    ```stenable {bool}```

    - {bool}: [Y]es/[N]o

  - Cleanup NV data with restart option

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

### Configuration for pushover.net message notifications.

  - Sets the 'Application Token/Key' for a given notification slot. Multiple slots allow for multiple pushover accounts or applications.

    ```pushover apptoken {slot} {hash}```

    - {slot}: [N]
      - For default use 0.

    - {hash}: Pushover.net 'User Auth Token'.

    Example: pushover apptoken 0 aabbccdd112233..

  - Sets the 'User Key' for a given notification slot.

    ```pushover userkey {slot} {hash}```

    - {slot}: [N]
      - For default use 0.

    - {hash}: Pushover 'User Key'.

    Example: pushover userkey 0 aabbccdd112233..

  - Define a smart virtual switch that will track and alert alarm panel state changes using user configurable filter and formatting rules.

    ```pushover switch {slot} {setting} {arg1} [arg2]```

    - {slot}
      - 1-99 : Supports multiple virtual smart alert switches.

    - {setting}
      - [N] Notification slot
        -  Notification settings slot to use for sending this events.
      - [D] Default state
        - {arg1}: [0]CLOSE(OFF) [1]OPEN(ON)
      - [R] AUTO Reset.
        - {arg1}:  time in ms 0 to disable
      - [T] Message type filter.
        - {arg1}: Message type list seperated by ',' or empty to disables filter.
          - Message Types: [ALPHA,LRR,REL,EXP,RFX,AUI,KPM,KPE,CRC,VER,ERR,EVENT]
            - For EVENT type the message will be generated by the API and not the AD2
      - [P] Pre filter REGEX or empty to disable.
      - [O] Open(ON) state regex search string list management.
        - {arg1}: Index # 1-8
        - {arg2}: Regex string for this slot or empty string  to clear
      - [C] Close(OFF) state regex search string list management.
        - {arg1}: Index # 1-8
        - {arg2}: Regex string for this slot or empty string to clear
      - [F] Fault state regex search string list management.
        - {arg1}: Index # 1-8
        - {arg2}: Regex string for this slot or empty  string to clear
      - [o] Open output format string.
      - [c] Close output format string.
      - [f] Fault output format string.

### Configuration for Twilio message notifications.

  - Sets the 'SID' for a given notification slot. Multiple slots allow for multiple twilio accounts.

    ```twilio sid {slot} {hash}```

    - {slot}: [N]
      - For default use 0.
    - {hash}: Twilio 'Account SID'.

    Example: twilio sid 0 aabbccdd112233..

  - Sets the 'Auth Token' for a given notification slot.

    ```twilio token {slot} {hash}```

    - {slot}: [N]
      - For default use 0.

    - {hash}: Twilio 'Auth Token'.

    Example: twilio token 0 aabbccdd112233..

  - Sets the 'From' info for a given notification slot.

    ```twilio from {slot} {phone|email}```

    - {phone|email}: [NXXXYYYZZZZ|user@example.com]

  - Sets the 'To' info for a given notification slot.

    ```twilio to {slot} {phone|email}```

    - {phone|email}: [NXXXYYYZZZZ|user@example.com]

  - Sets the 'Type' for a given notification slot.

    ```twilio type {slot} {type}```

    - {type}: [M|C|E]
      - Notification type [M]essage, [C]all, [E]mail.
    Example: type 0 M

  - Define a smart virtual switch that will track and alert alarm panel state changes using user configurable filter and formatting rules.

    ```twilio switch {slot} {setting} {arg1} [arg2]```

    - {slot}
      - 1-99 : Supports multiple virtual smart alert switches.

    - {setting}
      - [N] Notification slot
        -  Notification settings slot to use for sending this events.
      - [D] Default state
        - {arg1}: [0]CLOSE(OFF) [1]OPEN(ON)
      - [R] AUTO Reset.
        - {arg1}:  time in ms 0 to disable
      - [T] Message type filter.
        - {arg1}: Message type list seperated by ',' or empty to disables filter.
          - Message Types: [ALPHA,LRR,REL,EXP,RFX,AUI,KPM,KPE,CRC,VER,ERR,EVENT]
            - For EVENT type the message will be generated by the API and not the AD2
      - [P] Pre filter REGEX or empty to disable.
      - [O] Open(ON) state regex search string list management.
        - {arg1}: Index # 1-8
        - {arg2}: Regex string for this slot or empty string  to clear
      - [C] Close(OFF) state regex search string list management.
        - {arg1}: Index # 1-8
        - {arg2}: Regex string for this slot or empty string to clear
      - [F] Fault state regex search string list management.
        - {arg1}: Index # 1-8
        - {arg2}: Regex string for this slot or empty  string to clear
      - [o] Open output format string.
      - [c] Close output format string.
      - [f] Fault output format string.

    - Examples cli commands to setup a complete virtual contact.
      - Configure notification profiles..
        ```console
        # Profile #0 EMail using api.sendgrid.com
        twilio sid 0 NA
        twilio token 0 Abcdefg012345....
        twilio from 0 foo@example.com
        twilio to 0 bar@example.com
        twilio type 0 E

        # Profile #1 SMS/Text message using api.twilio.com
        twilio sid 1 Abcdefg012345....
        twilio token 1 Abcdefg012345....
        twilio from 1 15555551234
        twilio to 1 15555551234
        twilio type 1 M

        # Profile #2 Voice Twiml call using api.twilio.com
        twilio sid 2 Abcdefg012345....
        twilio token 2 Abcdefg012345....
        twilio from 2 15555551234
        twilio to 2 15555551234
        twilio type 2 C
        ```
      - Send notifications from profile in slot #0 for 5800 RF sensor with SN 0123456 and trigger on OPEN(ON), CLOSE(OFF) and FAULT REGEX patterns. In this example the Text or EMail sent would event contain the user defined message.
        ```console
        Twilio SmartSwitch #1 report
        # Set notification slot [N] to #0.
        twilio switch 1 N 0
        # Set default virtual switch state [D] to 'CLOSED'(0)
        twilio switch 1 D 0
        # Set auto reset time in ms [R] to 'DISABLED'
        twilio switch 1 R 0
        # Set message type list [T]
        twilio switch 1 T RFX
        # Set pre filter REGEX [P]
        twilio switch 1 P !RFX:0123456,.*
        # Set 'OPEN' state REGEX Filter [O] #01.
        twilio switch 1 O 1 !RFX:0123456,1.......
        # Set 'CLOSED' state REGEX Filter [C] #01.
        twilio switch 1 C 1 !RFX:0123456,0.......
        # Set 'FAULT' state REGEX Filter [F] #01.
        twilio switch 1 F 1 !RFX:0123456,......1.
        # Set output format string for 'OPEN' state [o].
        twilio switch 1 o RF SENSOR 0123456 OPEN
        # Set output format string for 'CLOSED' state [c].
        twilio switch 1 c RF SENSOR 0123456 CLOSED
        # Set output format string for 'FAULT' state [f].
        twilio switch 1 f RF SENSOR 0123456 FAULT
        ```
      - Send notifications from profile in slot #2 in the example a Call profile when EVENT message "FIRE ON" or "FIRE OFF" are received. Use a Twiml string to define how the call is processed. This can include extensive external logic calling multiple people or just say something and hangup.
        ```console
        Twilio SmartSwitch #2 report
        # Set notification slot [N] to #2.
        twilio switch 2 N 2
        # Set default virtual switch state [D] to 'CLOSED'(0)
        twilio switch 2 D 0
        # Set auto reset time in ms [R] to 'DISABLED'
        twilio switch 2 R 0
        # Set message type list [T]
        twilio switch 2 T EVENT
        # Set 'OPEN' state REGEX Filter [O] #01.
        twilio switch 2 O 1 FIRE ON
        # Set 'CLOSED' state REGEX Filter [C] #01.
        twilio switch 2 C 1 FIRE OFF
        # Set output format string for 'OPEN' state [o].
        twilio switch 2 o <Response><Say>Notification alert FIRE ALARM</Say></Response>
        # Set output format string for 'CLOSED' state [c].
        twilio switch 2 c <Response><Say>Notification alert FIRE CLEAR</Say></Response>
        ```
      - Send notifications from profile in slot #2 in the example a Call profile when EVENT message "POWER BATTERY" or "POWER AC" are received. Use a Twiml string to define how the call is processed. This can include extensive external logic calling multiple people or just say something and hangup.
        ```console
        Twilio SmartSwitch #3 report
        # Set notification slot [N] to #2.
        twilio switch 3 N 2
        # Set default virtual switch state [D] to 'CLOSED'(0)
        twilio switch 3 D 0
        # Set auto reset time in ms [R] to 'DISABLED'
        twilio switch 3 R 0
        # Set message type list [T]
        twilio switch 3 T EVENT
        # Set 'OPEN' state REGEX Filter [O] #01.
        twilio switch 3 O 1 POWER AC
        # Set 'CLOSED' state REGEX Filter [C] #01.
        twilio switch 3 C 1 POWER BATTERY
        # Set output format string for 'OPEN' state [o].
        twilio switch 3 o <Response><Say>Notification alert ON MAIN AC POWER</Say></Response>
        # Set output format string for 'CLOSED' state [c].
        twilio switch 3 c <Response><Say>Notification alert ON BATTERY BACKUP POWER</Say></Response>
        ```
      - Existing search verbs. ```RFX``` and others are not useful here as they can be filtered by message type ```RFX``` directly. The more useful verbs contain a modifier such as ON/OFF. TODO: Add ZONE tracking verbs and algorithm.
        ```
        ARM STAY
        ARM AWAY
        DISARMED
        POWER AC
        POWER BATTERY
        READY ON
        READY OFF
        ALARM ON
        ALARM OFF
        FIRE ON
        FIRE OFF
        LOW BATTERY
        CHIME ON
        CHIME OFF
        MESSAGE
        RELAY
        EXPANDER
        CONTACT ID
        RFX
        AUI
        KPM
        CRC
        VER
        ERR
        ```

## Building firmware
### PlatformIO
#### TODO: Setup notes.
### SmartThings device SDK build environment
#### Setup build environment
- Follow the instructions in the [SmartThings SDK for Direct connected devices for C](https://github.com/SmartThingsCommunity/st-device-sdk-c-ref) project for setting up a build environment. Confirm you can build the switch_example before continuing.
- Select the esp32 build environment. Branch v1.4 seems to be the current active branch and uses espidf v4.0.1-317-g50b3e2c81.
```
cd ~/esp
 git clone https://github.com/SmartThingsCommunity/st-device-sdk-c-ref.git -b release/v1.4
 cd st-device-sdk-c-ref
./setup.py esp32
```

- Place the contents of this his project in ```st-device-sdk-c-ref/apps/esp32/```

#### Configure the project

```
./build.sh esp32 AlarmDecoder-STSDK menuconfig
```

#### Build, Flash, and Run

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
- Pressing ENTER early in the ESP32 boot will prevent any startup tasks. This can help if the ESP32 is in a REBOOT CRASH LOOP. To exit this mode ```'restart'```
- If the AD2* is not communicating it may be stuck in a configuration mode or its configuration may have been corrupted during firmware update of the ESP32. If this happens you can directly connect to the AD2* over the UART or Socket by using the command ```'ad2term'```.

  Note) If the connection is a Socket it is currently necessary to have the ESP32 running and not halted at boot and connected with SmartThings for Wifi and networking to be active.

- Flashing the ESP32 board fails with timeout. It seems many of the ESP32 boards need a 4-10uF cap as a buffer on the EN pin and ground. This seems to fix it very well. Repeated attempts to upload with timeouts usually works by pressing the EN button on the board a few times during upload.