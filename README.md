<!-- vscode-markdown-toc -->
* 1. [Overview](#overview)
* 2. [Tested and recommended hardware](#tested-and-recommended-hardware)
* 3. [Firmware](#firmware)
  * 3.1. [SmartThings build (stsdk) - alarmdecoder_stsdk_esp32.bin](#smartthings-build-(stsdk)---alarmdecoder_stsdk_esp32.bin)
  * 3.2. [webUI build (webui) - alarmdecoder_webui_esp32.bin](#webui-build-(webui)---alarmdecoder_webui_esp32.bin)
* 4. [Configure the AD2IoT device](#configure-the-ad2iot-device)
* 5. [AD2Iot CLI - command line interface](#ad2iot-cli---command-line-interface)
  * 5.1. [Standard commands](#standard-commands)
  * 5.2. [Ser2sock server component](#ser2sock-server-component)
    * 5.2.1. [Configuration for Ser2sock server](#configuration-for-ser2sock-server)
  * 5.3. [Web User Interface webUI component](#web-user-interface-webui-component)
    * 5.3.1. [Configuration for webUI server](#configuration-for-webui-server)
  * 5.4. [SmartThings IoT integration component](#smartthings-iot-integration-component)
  * 5.5. [Pushover.net notification component](#pushover.net-notification-component)
    * 5.5.1. [Configuration for Pushover.net notification](#configuration-for-pushover.net-notification)
  * 5.6. [Twilio notification component](#twilio-notification-component)
    * 5.6.1. [Configuration for Twilio notifications](#configuration-for-twilio-notifications)
  * 5.7. [MQTT client component](#mqtt-client-component)
    * 5.7.1. [Configuration for MQTT message notifications](#configuration-for-mqtt-message-notifications)
* 6. [Building firmware](#building-firmware)
  * 6.1. [PlatformIO](#platformio)
    * 6.1.1. [TODO: Setup notes](#todo:-setup-notes)
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
 [Latest stable release ![Release Version](https://img.shields.io/github/release/nutechsoftware/AlarmDecoder-STSDK.svg?style=plastic) ![Release Date](https://img.shields.io/github/release-date/nutechsoftware/AlarmDecoder-STSDK.svg?style=plastic)](https://github.com/nutechsoftware/AlarmDecoder-STSDK/releases/latest/) [![Travis (.org) branch](https://img.shields.io/travis/nutechsoftware/AlarmDecoder-STSDK/master?style=plastic)](https://travis-ci.org/nutechsoftware/AlarmDecoder-STSDK)

 [Latest development branch ![Development branch](https://img.shields.io/badge/dev-yellow?style=plastic) ![GitHub last commit (branch)](https://img.shields.io/github/last-commit/nutechsoftware/AlarmDecoder-STSDK/dev?style=plastic)](https://github.com/nutechsoftware/AlarmDecoder-STSDK/tree/dev) [![Travis (.org) branch](https://img.shields.io/travis/nutechsoftware/AlarmDecoder-STSDK/dev?style=plastic)](https://travis-ci.org/nutechsoftware/AlarmDecoder-STSDK)

##  1. <a name='overview'></a>Overview

This project provides an example framework for building an IoT network appliance to monitor and control one or many alarm systems.

The [AD2IoT network appliance](https://www.alarmdecoder.com/catalog/product_info.php/products_id/50) integrates the AD2pHAT from AlarmDecoder.com and a ESP32-POE-ISO. The result is a device that easily connects to a compatible alarm system and publishes the alarm state to a public or private MQTT server for monitoring. The device can also be configured to initiate SIP voice calls, SMS messages, or e-mail when a user defined alarm state is triggered. The typical time from the device firmware start to being delivery of a state event is less than 10 seconds. Typical latency between an alarm event and message delivery is 20ms on a local network.

The device firmware is stored in the onboard non-volatile flash making the device resistant to corruption. With OTA update support the flash can be securely loaded with the latest version in just a few seconds over HTTPS.

##  2. <a name='tested-and-recommended-hardware'></a>Tested and recommended hardware
* ESP-POE-ISO. Ethernet+WiFi applications.
* ESP32-DevKitC-32E. WiFi only applications.

##  3. <a name='firmware'></a>Firmware
The firmware is already compiled for the ESP32-POE-ISO board and is available in the release page or via OTA(over-the-air) update. Currently the firmware is built with the following build flags 'stsdk' and 'webui'.

To switch to a specific build over the internet using OTA include the buildflag in the upgrade command.
- ```upgrade webui```

See the README-FLASH.MD inside the release file for instructions on flash the firmware over the ESP32-POE-ISO USB.

###  3.1. <a name='smartthings-build-(stsdk)---alarmdecoder_stsdk_esp32.bin'></a>SmartThings build (stsdk) - alarmdecoder_stsdk_esp32.bin
- Components: Pushover, Twilio, Sendgrid, ser2sock, SmartThings

This is the default buildflag. This build is compiled using the [st-device-sdk-c-ref](https://github.com/SmartThingsCommunity/st-device-sdk-c-ref) from the SmartThings github repo and has the webUI component disabled.

A few options are available as the AD2IoT device moves toward being certified and directly available in the SmartThings app. In order to Discover and adopt the AD2IoT device it needs to be visible in the "My Testing Devices" under the "Adding device" panel.

First you will need to [enable Developer Mode in the app
](https://developer.samsung.com/smartthings/blog/en-us/2019/02/13/using-developer-mode-in-the-smartthings-app)

Next decide if you want to build your own profile and layout or join the existing AlarmDecoder profile for how this device will be shown in the SmartThings app.

- Join the AlarmDecoder organization where the profiles are already working and in current development. Enroll in the  AlarmDecoder organization using the Manufacturer ID '''0AOf'''
https://smartthings.developer.samsung.com/partner/enroll

- Use the SmartThings/Samsung developer workspace to create custom profiles and onboarding as described in this howto guide [How to build Direct Connected devices with the SmartThings Platform](https://community.smartthings.com/t/how-to-build-direct-connected-devices/204055)

###  3.2. <a name='webui-build-(webui)---alarmdecoder_webui_esp32.bin'></a>webUI build (webui) - alarmdecoder_webui_esp32.bin
- Components: Pushover, Twilio, Sendgrid, ser2sock, webUI, MQTT

<img style="border:5px double black;"  src="contrib/webUI/EXAMPLE-PANEL-READY.jpg" width="200">


This build uses the latest ESP-IDF v4.3 that supports WebSockets. The SmartThings driver is disabled and the webui component is enabled.

Copy the contents of flash-drive folder into the root directory of a uSD flash drive formatted with a fat32 filesystem on a single MSDOS partition. Place this uSD flash drive into the ESP32-POE-ISO board and reboot.

##  4. <a name='configure-the-ad2iot-device'></a>Configure the AD2IoT device
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

##  5. <a name='ad2iot-cli---command-line-interface'></a>AD2Iot CLI - command line interface
Currently all configuration is done over the the ESP32 usb serial port. For drivers see the [ESP32-POE-ISO product page](https://www.olimex.com/Products/IoT/ESP32/ESP32-POE-ISO/open-source-hardware). The USB port also provides power so disconnect the device from the alarm using the quick connect terminal block and and connect the device to a computer using a USB 2.0 A-Male to Micro B Cable for configuration using the command line interface.

　<font color='red'>Do not connect the [AD2IoT network appliance](https://www.alarmdecoder.com/catalog/product_info.php/products_id/50) to alarm panel power when connected to a computer.</font> If connection from both PC and alarm at the same time is needed be sure to connect to the alarm panel power first then connect to the PC last. The power from the panel when first connected to the AD2IoT will provide an unstable 5v output for a few microseconds. This unstable voltage can be sent back through the USB to the host computer causing the host to detect the voltage fault and halt.

###  5.1. <a name='standard-commands'></a>Standard commands
- Show the list of commands or give more detail on a specific command.
  - ```help [command]```
- Set logging mode.
  - ```logmode {level}```
    - {level}
      - [I]nformational.
      - [V]erbose.
      - [D]ebugging. (Only if compiled with DEBUG).
      - [N]one: (default) Warnings and errors only.
  - Examples:
    - Set logging mode to INFO.
      - ```logmode I```
- Restart the device.
  - ```restart```
- Preform an OTA upgrade now download and install new flash.
  - ```upgrade [buildflag]```
    - [buildflag]: Specify build for the release. default to 'stsdk' if omitted. See release page for details on available builds.
- Report the current and available version.
  - ```version```
- Manage network connection type.
  - ```netmode {mode} [args]```
    - {mode}
      - [N]one: (default) Do not enable any network let component(s) manage the networking.
      - [W]iFi: Enable WiFi network driver.
      - [E]thernet: Enable ethernet network driver.
    - [arg]
      - Argument string name value pairs separated by &.
        - Keys: MODE,IP,MASK,GW,DNS1,DNS2,SID,PASSWORD
  - Examples
    - WiFi DHCP with SID and password.
      - netmode W mode=d&sid=example&password=somethingsecret
    - Ethernet DHCP DNS2 override.
      - netmode E mode=d&dns2=4.2.2.2
    - Ethernet Static IPv4 address.
      - netmode E mode=s&ip=192.168.1.111&mask=255.255.255.0&gw=192.168.1.1&dns1=4.2.2.2&dns2=8.8.8.8
- Simulate a button press event.
  - ```button {count} {type}```
    - {count}
      - Number of times the button was pushed.
    - {type}
      - The type of event 'short' or 'long'.
  - Examples
    - Send a single LONG button press.
      - button 1 long
- Manage user codes.
  - ```code {id} [value]```
    - {id}
      - Index of code to evaluate. 0 is default.
    - [value]
      - A valid alarm code or -1 to remove.
  - Examples
    - Set default code to 1234.
      - code 0 1234
    - Set alarm code for slot 1.
      - code 1 1234
    - Show code in slot #3.
      - code 3
    - Remove code for slot 2.
      - code 2 -1
        - Note: value -1 will remove an entry.
- Connect directly to the AD2* source and halt processing with option to hard reset AD2pHat.
  - ```ad2term [reset]```
    - Note: To exit send a period ```.``` three times fast.
- Manage virtual partitions.
  - ```vpart {id} {value}```
    - {id}
      - The virtual partition ID. 0 is the default.
    - [value]
      - (Ademco)Keypad address or (DSC)Partion #. -1 to delete.
    - Examples
      - Set default address mask to 18 for an Ademco system.
        - vpart 0 18
      - Set default send partition to 1 for a DSC system.
        - vpart 0 1
      - Show address for partition 2.
        - vpart 2
      - Remove virtual partition in slot 2.
        - vpart 2 -1
          - Note: address -1 will remove an entry.
- Manage AlarmDecoder protocol source.
  - ```ad2source [{mode} {arg}]```
    - {mode}
      - [S]ocket: Use ser2sock server over tcp for AD2* messages.
      - [C]om port: Use local UART for AD2* messages.
    - {arg}
      - [S]ocket arg: {HOST:PORT}
      - [C]om arg: {TXPIN:RXPIN}.
  - Examples:
    - Show current mode.
      - ad2source
    - Set source to ser2sock client at address and port.
      - ad2source SOCK 192.168.1.2:10000
    - Set source to local attached uart with TX on GPIO 17 and RX on GPIO 16.
      - ad2source COM 17:16

###  5.2. <a name='ser2sock-server-component'></a>Ser2sock server component
Ser2sock allows sharing of a serial device over a TCP/IP network. It also supports encryption and authentication via OpenSSL. Typically configured for port 10000 several home automation systems are able to use this protocol to talk to the AlarmDecoder device for a raw stream of messages. Please be advised that network scanning of this port can lead to alarm faults. It is best to use the Access Control List feature to only allow specific hosts to communicate directly with the AD2* and the alarm panel.

####  5.2.1. <a name='configuration-for-ser2sock-server'></a>Configuration for Ser2sock server
- ```ser2sockd {sub command} {arg}```
  - {sub command}
    - [enable] Enable / Disable ser2sock daemon
      -  {arg1}: [Y]es [N]o
        - [N] Default state
        - Example: ser2sockd enable Y
    - [acl] Set / Get ACL list
      - {arg1}: ACL LIST
      -  String of CIDR values seperated by commas.
        - Default: Empty string disables ACL list
        - Example: ser2sockd acl 192.168.0.0/28,192.168.1.0-192.168.1.10,192.168.3.4

###  5.3. <a name='web-user-interface-webui-component'></a>Web User Interface webUI component
####  5.3.1. <a name='configuration-for-webui-server'></a>Configuration for webUI server
- ```webui {sub command} {arg}```
  - {sub command}
    - [enable] Enable / Disable WebUI daemon
      -  {arg1}: [Y]es [N]o
        - [N] Default state
        - Example: webui enable Y
    - [acl] Set / Get ACL list
      - {arg1}: ACL LIST
      -  String of CIDR values seperated by commas.
        - Default: Empty string disables ACL list
        - Example: webui acl 192.168.0.0/28,192.168.1.0-192.168.1.10,192.168.3.4

###  5.4. <a name='smartthings-iot-integration-component'></a>SmartThings IoT integration component


###  5.5. <a name='pushover.net-notification-component'></a>Pushover.net notification component
Pushover is a platform for sending and receiving push notifications. On the server side, they provide an HTTP API for queueing messages to deliver to devices addressable by User or Group Keys. On the device side, they offer iOS, Android, and Desktop clients to receive those push notifications, show them to the user, and store them for offline viewing. See: https://pushover.net/

####  5.5.1. <a name='configuration-for-pushover.net-notification'></a>Configuration for Pushover.net notification
- Sets the ```Application Token/Key``` for a given notification slot. Multiple slots allow for multiple pushover accounts or applications.
  - ```pushover apptoken {slot} {hash}```
    - {slot}: [N]
      - For default use 0.
    - {hash}: Pushover.net ```User Auth Token```.
  - Example: ```pushover apptoken 0 aabbccdd112233..```
- Sets the 'User Key' for a given notification slot.
  - ```pushover userkey {slot} {hash}```
    - {slot}: [N]
      - For default use 0.
    - {hash}: Pushover ```User Key```.
  - Example: ```pushover userkey 0 aabbccdd112233..```
- Define a smart virtual switch that will track and alert alarm panel state changes using user configurable filter and formatting rules.
  - ```pushover switch {slot} {setting} {arg1} [arg2]```
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

###  5.6. <a name='twilio-notification-component'></a>Twilio notification component
Twilio (/ˈtwɪlioʊ/) is an American cloud communications platform as a service (CPaaS) company based in San Francisco, California. Twilio allows software developers to programmatically make and receive phone calls, send and receive text messages, reliably send email, and perform other communication functions using its web service APIs. See: https://www.twilio.com/

####  5.6.1. <a name='configuration-for-twilio-notifications'></a>Configuration for Twilio notifications
- Sets the 'SID' for a given notification slot. Multiple slots allow for multiple twilio accounts.
  - ```twilio sid {slot} {hash}```
    - {slot}: [N]
      - For default use 0.
    - {hash}: Twilio ```Account SID```.
  - Example: ```twilio sid 0 aabbccdd112233..```
- Sets the ```Auth Token``` for a given notification slot.
  - ```twilio token {slot} {hash}```
    - {slot}: [N]
      - For default use 0.
    - {hash}: Twilio ```Auth Token```.
    -  Example: ```twilio token 0 aabbccdd112233..```
- Sets the 'From' info for a given notification slot.
  - ```twilio from {slot} {phone|email}```
    - {phone|email}: [NXXXYYYZZZZ|user@example.com]
- Sets the 'To' info for a given notification slot.
  - ```twilio to {slot} {phone|email}```
    - {phone|email}: [NXXXYYYZZZZ|user@example.com]
- Sets the 'Type' for a given notification slot.
  - ```twilio type {slot} {type}```
    - {type}: [M|C|E]
      - Notification type [M]essage, [C]all, [E]mail.
  - Example: ```twilio type 0 M```
- Define a smart virtual switch that will track and alert alarm panel state changes using user configurable filter and formatting rules.
  - ```twilio switch {slot} {setting} {arg1} [arg2]```
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

###  5.7. <a name='mqtt-client-component'></a>MQTT client component
MQTT is an OASIS standard messaging protocol for the Internet of Things (IoT). It is designed as an extremely lightweight publish/subscribe messaging transport that is ideal for connecting remote devices with a small code footprint and minimal network bandwidth. MQTT today is used in a wide variety of industries, such as automotive, manufacturing, telecommunications, oil and gas, etc. See: https://mqtt.org/

- Implementation notes:
  - Each client connects under the root topic of ```ad2iot``` with a sub topic level of the devices UUID.
    - Example: ```ad2iot/41443245-4d42-4544-4410-XXXXXXXXXXXX```
  - LWT is used to indicate ```online```/```offline``` ```state``` of client using ```status``` topic below the device root topic.
    - Example: ```ad2iot/41443245-4d42-4544-4410-XXXXXXXXXXXX/status = {"state": "online"}```
  - Device specific info is in the ```info``` topic below the device root topic.
  - Partition state tracking with minimal traffic only when state changes. Each configured partition will be under the ```partitions``` topic below the device root topic.
    - Example: ```ad2iot/41443245-4d42-4544-4410-30aea49e7130/partitions/1 =
{"ready":false,"armed_away":false,"armed_stay":false,"backlight_on":false,"programming_mode":false,"zone_bypassed":false,"ac_power":true,"chime_on":false,"alarm_event_occurred":false,"alarm_sounding":false,"battery_low":true,"entry_delay_off":false,"fire_alarm":false,"system_issue":false,"perimeter_only":false,"exit_now":false,"system_specific":3,"beeps":0,"panel_type":"A","last_alpha_messages":"SYSTEM LO BAT                   ","last_numeric_messages":"008","event":"LOW BATTERY"}```
    - Custom virtual switches with user defined topics are kept under the ```switches``` below the device root topic.
      - Example: ```ad2iot/41443245-4d42-4544-4410-30aea49e7130/switches/RF0180036 = {"state":"RF SENSOR 0180036 CLOSED"}```
####  5.7.1. <a name='configuration-for-mqtt-message-notifications'></a>Configuration for MQTT message notifications
- Publishes the virtual partition state using the following topic pattern.
  - ad2iot/41443245-4d42-4544-4410-XXXXXXXXXXXX/partitions/Y
  - X: The unique id using the ESP32 WiFi mac address.
  - Y: The virtual partition ID 1-9 or a Virtual switch sub topic.
- [enable] Enable / Disable MQTT client
  -  {arg1}: [Y]es [N]o
    - [N] Default state
  - Example: ```mqtt enable Y```
- Sets the URL to the MQTT broker.
  - ```mqtt url {url}```
    - {url}: MQTT broker URL.
  - Example: ```mqtt url mqtt://mqtt.eclipseprojects.io```
- Define a smart virtual switch that will track and alert alarm panel state changes using user configurable filter and formatting rules.
  - ```mqtt switch {slot} {setting} {arg1} [arg2]```
    - {slot}
      - 1-99 : Supports multiple virtual smart alert switches.
    - {setting}
      - [N] Notification sub topic path below the base
        -  Example: ```TEST``` full topic will be ```ad2iot/41443245-4d42-4544-4410-XXXXXXXXXXXX/switches/TEST```
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

- Examples
  - Publish events to <device root>/switches/RF0180036 when the 5800 RF sensor with serial number 1234567 changes state.
  ```console
  MQTT SmartSwitch #1 report
  # Set MQTT topic [N] to 'RF1234567'.
  mqtt switch 1 N RF1234567
  # Set default virtual switch state [D] to 'CLOSED'(0)
  mqtt switch 1 D 0
  # Set auto reset time in ms [R] to 'DISABLED'
  mqtt switch 1 R 0
  # Set message type list [T]
  mqtt switch 1 T RFX
  # Set pre filter REGEX [P]
  mqtt switch 1 P !RFX:1234567,.*
  # Set 'OPEN' state REGEX Filter [O] #01.
  mqtt switch 1 O 1 !RFX:1234567,1.......
  # Set 'CLOSED' state REGEX Filter [C] #01.
  mqtt switch 1 C 1 !RFX:1234567,0.......
  # Set 'FAULT' state REGEX Filter [F] #01.
  mqtt switch 1 F 1 !RFX:1234567,......1.
  # Set output format string for 'OPEN' state [o].
  mqtt switch 1 o RF SENSOR 1234567 OPEN
  # Set output format string for 'CLOSED' state [c].
  mqtt switch 1 c RF SENSOR 1234567 CLOSED
  # Set output format string for 'FAULT' stat
  ```

##  6. <a name='building-firmware'></a>Building firmware
###  6.1. <a name='platformio'></a>PlatformIO
####  6.1.1. <a name='todo:-setup-notes'></a>TODO: Setup notes
###  6.2. <a name='smartthings-device-sdk-build-environment'></a>SmartThings device SDK build environment
####  6.2.1. <a name='setup-build-environment'></a>Setup build environment
- Follow the instructions in the [SmartThings SDK for Direct connected devices for C](https://github.com/SmartThingsCommunity/st-device-sdk-c-ref) project for setting up a build environment. Confirm you can build the switch_example before continuing.
- Select the esp32 build environment. Branch v1.4 seems to be the current active branch and uses espidf v4.0.1-317-g50b3e2c81.
```
cd ~/esp
 git clone https://github.com/SmartThingsCommunity/st-device-sdk-c-ref.git -b release/v1.4
 cd st-device-sdk-c-ref
./setup.py esp32
```

- Place the contents of this his project in ```st-device-sdk-c-ref/apps/esp32/```

####  6.2.2. <a name='configure-the-project'></a>Configure the project

```
./build.sh esp32 AlarmDecoder-STSDK menuconfig
```

####  6.2.3. <a name='build,-flash,-and-run'></a>Build, Flash, and Run

Build the project and flash it to the board, then run monitor tool to view serial output:

```
./build.sh esp32 AlarmDecoder-STSDK build flash monitor -p /dev/ttyUSB0
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Prerequisites](https://github.com/SmartThingsCommunity/st-device-sdk-c-ref#prerequisites) for full steps to configure and use ESP-IDF and the STSDK to build this project.

##  7. <a name='example-output-from-esp32-usb-serial-console'></a>Example Output from ESP32 USB serial console

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

##  8. <a name='troubleshooting'></a>Troubleshooting
- Pressing ENTER early in the ESP32 boot will prevent any startup tasks. This can help if the ESP32 is in a REBOOT CRASH LOOP. To exit this mode ```'restart'```
- If the AD2* is not communicating it may be stuck in a configuration mode or its configuration may have been corrupted during firmware update of the ESP32. If this happens you can directly connect to the AD2* over the UART or Socket by using the command ```'ad2term'```.

  Note) If the connection is a Socket it is currently necessary to have the ESP32 running and not halted at boot and connected with SmartThings for Wifi and networking to be active.

- Flashing the ESP32 board fails with timeout. It seems many of the ESP32 boards need a 4-10uF cap as a buffer on the EN pin and ground. This seems to fix it very well. Repeated attempts to upload with timeouts usually works by pressing the EN button on the board a few times during upload.