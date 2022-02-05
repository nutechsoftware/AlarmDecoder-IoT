<!-- vscode-markdown-toc -->
* 1. [Overview](#overview)
* 2. [Tested and recommended hardware](#tested-and-recommended-hardware)
* 3. [Firmware](#firmware)
  * 3.1. [SmartThings build (stsdk) - alarmdecoder_stsdk_esp32.bin](#smartthings-build-(stsdk)---alarmdecoder_stsdk_esp32.bin)
  * 3.2. [webUI build (webui) - alarmdecoder_webui_esp32.bin](#webui-build-(webui)---alarmdecoder_webui_esp32.bin)
* 4. [Configuring the AD2IoT device](#configuring-the-ad2iot-device)
* 5. [AD2Iot CLI - command line interface](#ad2iot-cli---command-line-interface)
  * 5.1. [Standard commands](#standard-commands)
  * 5.2. [Ser2sock server component](#ser2sock-server-component)
    * 5.2.1. [Configuration for Ser2sock server](#configuration-for-ser2sock-server)
  * 5.3. [Web User Interface webUI component](#web-user-interface-webui-component)
    * 5.3.1. [Configuration for webUI server](#configuration-for-webui-server)
  * 5.4. [SmartThings Direct Connected device.](#smartthings-direct-connected-device.)
    * 5.4.1. [ Configuration for SmartThings IoT client](#-configuration-for-smartthings-iot-client)
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
 [Latest stable release ![Release Version](https://img.shields.io/github/release/nutechsoftware/AlarmDecoder-IoT.svg?style=plastic) ![Release Date](https://img.shields.io/github/release-date/nutechsoftware/AlarmDecoder-IoT.svg?style=plastic)](https://github.com/nutechsoftware/AlarmDecoder-IoT/releases/latest/) [![Travis (.org) branch](https://img.shields.io/travis/nutechsoftware/AlarmDecoder-IoT/master?style=plastic)](https://travis-ci.org/nutechsoftware/AlarmDecoder-IoT)

 [Latest development branch ![Development branch](https://img.shields.io/badge/dev-yellow?style=plastic) ![GitHub last commit (branch)](https://img.shields.io/github/last-commit/nutechsoftware/AlarmDecoder-IoT/dev?style=plastic)](https://github.com/nutechsoftware/AlarmDecoder-IoT/tree/dev) [![Travis (.org) branch](https://img.shields.io/travis/nutechsoftware/AlarmDecoder-IoT/dev?style=plastic)](https://travis-ci.org/nutechsoftware/AlarmDecoder-IoT)

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

If the upgrade fails it may be the result of low memory on the device. Try disabling features restart the device and try again. Example. ```webui disable Y```. If all else fails install the latest release of the AD2IoT firmware over USB.

See the README-FLASH.MD inside the release file for instructions on flashing the firmware over the ESP32-POE-ISO USB port.

###  3.1. <a name='smartthings-build-(stsdk)---alarmdecoder_stsdk_esp32.bin'></a>SmartThings build (stsdk) - alarmdecoder_stsdk_esp32.bin
- Enabled components: Pushover, Twilio, Sendgrid, Ser2sock, SmartThings.

This is the default buildflag. This build is compiled using the [st-device-sdk-c-ref](https://github.com/SmartThingsCommunity/st-device-sdk-c-ref) from the SmartThings github repo and has the webUI component disabled.

A few options are available as the AD2IoT device moves toward being certified and directly available in the SmartThings app. In order to Discover and adopt the AD2IoT device it needs to be visible in the "My Testing Devices" under the "Adding device" panel.

First you will need to [enable Developer Mode in the app
](https://developer.samsung.com/smartthings/blog/en-us/2019/02/13/using-developer-mode-in-the-smartthings-app)

Next decide if you want to build your own profile and layout or join the existing AlarmDecoder profile for how this device will be shown in the SmartThings app.

- Join the ```Nu Tech Software Solutions, Inc.``` organization where the profiles are already working and in current development. Enroll in the organization using the Manufacturer ID '''0AOf'''. Once enrolled the device serial number and keys will be manually generated and sent to the same email address.
https://smartthings.developer.samsung.com/partner/enroll

- Use the SmartThings/Samsung developer workspace to create custom profiles and onboarding as described in this howto guide [How to build Direct Connected devices with the SmartThings Platform](https://community.smartthings.com/t/how-to-build-direct-connected-devices/204055). Generate a serial number and keys and register them in the management portal and configure the device with the validated keys.

###  3.2. <a name='webui-build-(webui)---alarmdecoder_webui_esp32.bin'></a>webUI build (webui) - alarmdecoder_webui_esp32.bin
- Enabled components: Pushover, Twilio, Sendgrid, ser2sock, webUI, MQTT.

This build uses the latest ESP-IDF v4.3 that supports WebSockets. The SmartThings driver is disabled and the webui component is enabled.

Copy the contents of flash-drive folder into the root directory of a uSD flash drive formatted with a fat32 filesystem on a single MSDOS partition. Place this uSD flash drive into the ESP32-POE-ISO board and reboot.

##  4. <a name='configuring-the-ad2iot-device'></a>Configuring the AD2IoT device
Configuration is currently only available over the USB serial port using a command line interface.

&#x2757; Do not connect the [AD2IoT network appliance](https://www.alarmdecoder.com/catalog/product_info.php/products_id/50) to alarm panel power when connected to a computer. If connection from both PC and alarm at the same time is needed be sure to connect to the alarm panel power first then connect to the PC last. The power from the panel when first connected to the AD2IoT will provide an unstable 5v output for a few microseconds. This unstable voltage can be sent back through the USB to the host computer causing the host to detect the voltage fault and halt.

- Connect the ESP32 development board USB to a host computer use a USB A to USB Micro B cable and run a terminal program such as [Putty](https://www.putty.org/) or [Tiny Serial](http://brokestream.com/tinyserial.html) to connect to the USB com port using 115200 baud. Most Linux distributions already have the CH340 USB serial port driver installed.
  - If needed the drivers for different operating systems can be downloaded [here](https://www.olimex.com/Products/IoT/ESP32/ESP32-POE-ISO/open-source-hardware).

##  5. <a name='ad2iot-cli---command-line-interface'></a>AD2Iot CLI - command line interface
- Configure the initial AD2IoT device settings
  - Select TTL GPIO pins or socket address and port for the AlarmDecoder protocol stream using one of the following commands.
    - ```ad2source COM 4:36```
    - ```ad2source SOCK 192.168.0.121:10000```
  - Enable ser2sock daemon and optionally configure the ACL for security.
    - ```ser2sockd enable Y```
    - ```ser2sockd acl 192.168.0.123/32```
  - Set the log mode to see INFO messages
    - ```logmode I```
  - Restart for these changes to take effect.
    - ```restart```

- Choose the primary operating mode of the AD2IoT device.
  - Managed network mode.
    - Enable and configure the WiFi or Ethernet networking driver.
      - Set ```'netmode'``` to ```W``` or ```E``` to enable the Wifi or Ethernet drivers and the ```<args>``` to configure the networking options such as IP address GW or DHCP and for Wifi the AP and password.
      - ```netmode E mode=d```
    - Configure the default partition in slot 0 for the partition to connect to SmartThings.
      - ```vpart 0 18```
    - Define each virtual partition. For Ademco a free keypad address needs to be assigned to each partition to control. DSC is ZeroConf and only requires the partition # from 1-8.
      - ```vpart 1 22```
    - Set a default code in slot ```0``` to ARM/DISARM etc a partition.
      - ```code 0 4112```
    - Enable webui daemon and configure the ACL.
      - ```webui enable Y```
      - ```webui acl 192.168.1.0/24```
    - Insert a uSD card formatted fat32 with the files in the ```/contrib/webUI/flash-drive/``` folder of this project in the root directory.
    - Restart for these changes to take effect.
      - ```restart```
    - Configure notifications

  - SmartThings Direct-connected device mode.
    - Disable networking to allow the SmartThings driver to manage the network hardware and allow adopting over 802.11 b/g/n 2.4ghz Wi-Fi.
      - ```netmode N```
    - Configure the default partition in slot 0 for the partition to connect to the SmartThings app.
      - ```vpart 0 18```
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

###  5.1. <a name='standard-commands'></a>Standard commands
- Show the list of commands or give more detail on a specific command.
  - ```help [command]```
- Set logging mode.
  - ```logmode {level}```
    - {level}
      - [I]nformational
      - [V]Verbose
      - [D]ebugging
      - [N]one: (default) Warnings and errors only.
  - Examples:
    - Set logging mode to INFO.
      - ```logmode I```
- Restart the device.
  - ```restart```
- Erase all NVS storage clearing all settings and reboot.
  - ```factory-reset```
- Preform an OTA upgrade now download and install new flash.
  - ```upgrade [buildflag]```
    - [buildflag]: Specify build for the release. default to current if omitted.
      - See release page for details on available builds.
- Report the current and available version.
  - ```version```
- Manage network connection type.
  - ```netmode {mode} [args]```
    - {mode}
      - [N]one: (default) Do not enable any network let component(s) manage the networking.
      - [W]iFi: Enable WiFi network driver.
      - [E]thernet: Enable ethernet network driver.
    - [arg]
      - Argument string name value pairs sepearted by &.
        - Keys: MODE,IP,MASK,GW,DNS1,DNS2,SID,PASSWORD
  - Examples
    - WiFi DHCP with SID and password.
      - ```netmode W mode=d&sid=example&password=somethingsecret```
    - Ethernet DHCP DNS2 override.
      - ```netmode E mode=d&dns2=4.2.2.2```
    - Ethernet Static IPv4 address.
      - ```netmode E mode=s&ip=192.168.1.111&mask=255.255.255.0&gw=192.168.1.1&dns1=4.2.2.2&dns2=8.8.8.8```
- Simulate a button press event.
  - ```button {id} {count} {type}```
    - {id}
      - ID of the button to push [A|B].
    - {count}
      - Number of times the button was pushed.
    - {type}
      - The type of event 'short' or 'long'.
  - Examples
    - Send a single LONG press to button A.
      - ```button A 1 long```
- Manage user codes.
  - ```code {id} [value]```
    - {id}
      - Index of code to evaluate. 0 is default.
    - [value]
      - A valid alarm code or -1 to remove.
  - Examples
    - Set default code to 1234.
      - ```code 0 1234```
    - Set alarm code for slot 1.
      - ```code 1 1234```
    - Show code in slot #3.
      - ```code 3```
    - Remove code for slot 2.
      - ```code 2 -1```
        - Note: value -1 will remove an entry.
- Connect directly to the AD2* source and halt processing with option to hard reset AD2pHat.
  - ```ad2term [reset]```
    - Note: To exit press ... three times fast.
- Manage virtual partitions.
  - ```vpart {id} {value} [zone list]```
    - {id}
      - The virtual partition ID. 0 is the default.
    - {value}
      - (Ademco)Keypad address or (DSC)Partion #. -1 to delete.
    - [zone list]
      - Comma separated list of zone numbers associated with this partition for tracking.
  - Examples
    - Set default address mask to 18 for an Ademco system with zones 2, 3, and 4.
      - ```vpart 0 18 2,3,4```
    - Set default send partition to 1 for a DSC system.
      - ```vpart 0 1```
    - Show address for partition 2.
      - ```vpart 2```
    - Remove virtual partition in slot 2.
      - ```vpart 2 -1```
       - Note: address -1 will remove an entry.
- Manage zone strings.
  - ```zone {id} [value]```
    - {id}
      - Zone number 1-255.
    - [value]
      - An alpha string for the zone.
  - Examples
    - Set zone 1 alpha string.
      - ```zone 1 TESTING LAB```
    - Remove zone 1 alpha string.
      - ```zone 1 ''```
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
      - ```ad2source```
    - Set source to ser2sock client at address and port.
      - ```ad2source SOCK 192.168.1.2:10000```
    - Set source to local attached uart with TX on GPIO 17 and RX on GPIO 16.
      - ```ad2source COM 17:16```

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
      -  String of CIDR values separated by commas.
        - Default: Empty string disables ACL list
        - Example: ser2sockd acl 192.168.0.0/28,192.168.1.0-192.168.1.10,192.168.3.4

###  5.3. <a name='web-user-interface-webui-component'></a>Web User Interface webUI component
This component provides a simple HTML5+WebSocket user interface with realtime alarm status using push messages over WebSocket. Buttons for Arming, Disarming, Exiting, and sending panic events. Panic buttons require pressing the button three times in 5 seconds to help prevent false alarms.<br>
<img src="contrib/webUI/EXAMPLE-PANEL-READY.jpg" width="200">

####  5.3.1. <a name='configuration-for-webui-server'></a>Configuration for webUI server
- ```webui {sub command} {arg}```
  - {sub command}
    - [enable] Enable / Disable WebUI daemon
      -  {arg1}: [Y]es [N]o
        - [N] Default state
        - Example: webui enable Y
    - [acl] Set / Get ACL list
      - {arg1}: ACL LIST
      -  String of CIDR values separated by commas.
        - Default: Empty string disables ACL list
        - Example: webui acl 192.168.0.0/28,192.168.1.0-192.168.1.10,192.168.3.4

###  5.4. <a name='smartthings-direct-connected-device.'></a>SmartThings Direct Connected device.
Direct-connected devices connect directly to the SmartThings cloud. The SDK for Direct Connected Devices is equipped to manage all MQTT topics and onboarding requirements, freeing you to focus on the actions and attributes of your device. To facilitate the development of device application in an original chipset SDK, the core device library and the examples were separated into two git repositories. That is, if you want to use the core device library in your original chipset SDK that installed before, you may simply link it to develop a device application in your existing development environment. For more info see https://github.com/SmartThingsCommunity/st-device-sdk-c-ref.

####  5.4.1. <a name='-configuration-for-smartthings-iot-client'></a> Configuration for SmartThings IoT client
- Enable SmartThings component
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
      - [-] Delete switch
      - [N] Notification slots
        -  Comma separated list of notification slots to use for sending this events.
      - [D] Default state
        - {arg1}: [0]CLOSE(OFF) [1]OPEN(ON)
      - [R] AUTO Reset.
        - {arg1}:  time in ms 0 to disable
      - [T] Message type filter.
        - {arg1}: Message type list separated by ',' or empty to disables filter.
          - Message Types: [ALPHA,LRR,REL,EXP,RFX,AUI,KPM,KPE,CRC,VER,ERR,EVENT]
            - For EVENT type the message will be generated by the API and not the AD2
      - [P] Pre filter REGEX or empty to disable.
      - [O] Open(ON) state regex search string list management.
        - {arg1}: Index # 1-8
        - {arg2}: Regex string for this slot or empty string  to clear
      - [C] Close(OFF) state regex search string list management.
        - {arg1}: Index # 1-8
        - {arg2}: Regex string for this slot or empty string to clear
      - [F] Trouble state regex search string list management.
        - {arg1}: Index # 1-8
        - {arg2}: Regex string for this slot or empty  string to clear
      - [o] Open output format string.
      - [c] Close output format string.
      - [f] Trouble output format string.

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
    - {phone|email}: [NXXXYYYZZZZ|user@example.com, user2@example.com]
- Sets the 'Type' for a given notification slot.
  - ```twilio type {slot} {type}```
    - {type}: [M|C|E]
      - Notification type [M]essage, [C]all, [E]mail.
  - Example: ```twilio type 0 M```
- Sets the output format for a given notification slot.
  - ```twilio format {slot} {string}```
    - {string}: Format string used to generate final output from switch output args string.
      -  Placeholder-based formatting syntax.
  - Example: ```twilio format 2 <Response><Say>{0}</Say><Say>{0}</Say></Response>```
- Define a smart virtual switch that will track and alert alarm panel state changes using user configurable filter and formatting rules.
  - ```twilio switch {slot} {setting} {arg1} [arg2]```
    - {slot}
      - 1-99 : Supports multiple virtual smart alert switches.
    - {setting}
      - [-] Delete switch
      - [N] Notification slots
        -  Comma separated list of notification slots to use for sending this events.
      - [D] Default state
        - {arg1}: [0]CLOSE(OFF) [1]OPEN(ON)
      - [R] AUTO Reset.
        - {arg1}:  time in ms 0 to disable
      - [T] Message type filter.
        - {arg1}: Message type list separated by ',' or empty to disables filter.
          - Message Types: [ALPHA,LRR,REL,EXP,RFX,AUI,KPM,KPE,CRC,VER,ERR,EVENT]
            - For EVENT type the message will be generated by the API and not the AD2
      - [P] Pre filter REGEX or empty to disable.
      - [O] Open(ON) state regex search string list management.
        - {arg1}: Index # 1-8
        - {arg2}: Regex string for this slot or empty string  to clear
      - [C] Close(OFF) state regex search string list management.
        - {arg1}: Index # 1-8
        - {arg2}: Regex string for this slot or empty string to clear
      - [F] Trouble state regex search string list management.
        - {arg1}: Index # 1-8
        - {arg2}: Regex string for this slot or empty  string to clear
      - [o] Open output string.
      - [c] Close output string.
      - [f] Trouble output string.

- Example cli commands to setup a complete virtual contact.
  - Configure notification profiles..
    ```console
    # Profile #0 EMail using api.sendgrid.com
    twilio sid 0 NA
    twilio token 0 Abcdefg012345....
    twilio from 0 foo@example.com
    twilio to 0 bar@example.com
    twilio type 0 E
    twilio format 0 {0}

    # Profile #1 SMS/Text message using api.twilio.com
    twilio sid 1 Abcdefg012345....
    twilio token 1 Abcdefg012345....
    twilio from 1 15555551234
    twilio to 1 15555551234
    twilio type 1 M
    twilio format 1 {0}

    # Profile #2 Voice Twiml call using api.twilio.com
    twilio sid 2 Abcdefg012345....
    twilio token 2 Abcdefg012345....
    twilio from 2 15555551234
    twilio to 2 15555551234
    twilio type 2 C
    twilio format 2 <Response><Pause length="3"/><Say>{0}</Say><Pause length="3"/><Say>{0}</Say><Pause length="3"/><Say>{0}</Say></Response>
    ```
  - Send notifications from profile in slot #0 for 5800 RF sensor with SN 0123456 and trigger on OPEN(ON), CLOSE(OFF) and TROUBLE REGEX patterns. In this example the Text or EMail sent would event contain the user defined message.
    ```console
    Twilio SmartSwitch #1 report
    # Set notification slots [N] to #0,1,2
    twilio switch 1 N 0,1,2
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
    # Set 'TROUBLE' state REGEX Filter [F] #01.
    twilio switch 1 F 1 !RFX:0123456,......1.
    # Set output format string for 'OPEN' state [o].
    twilio switch 1 o RF SENSOR 0123456 OPEN
    # Set output format string for 'CLOSED' state [c].
    twilio switch 1 c RF SENSOR 0123456 CLOSED
    # Set output format string for 'TROUBLE' state [f].
    twilio switch 1 f RF SENSOR 0123456 TROUBLE
    ```
  - Send notifications from profile in slot #2 in the example a Call profile when EVENT message "FIRE ON" or "FIRE OFF" are received. Use a Twiml string to define how the call is processed. This can include extensive external logic calling multiple people or just say something and hangup.
    ```console
    Twilio SmartSwitch #2 report
    # Set notification slots [N] to #2
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
    twilio switch 2 o FIRE ALARM ACTIVE
    # Set output format string for 'CLOSED' state [c].
    twilio switch 2 c FIRE ALARM CLEAR
    ```
  - Send notifications from profile in slot #2 in the example a Call profile when EVENT message "POWER BATTERY" or "POWER AC" are received. Use a Twiml string to define how the call is processed. This can include extensive external logic calling multiple people or just say something and hangup.
    ```console
    Twilio SmartSwitch #3 report
    # Set notification slots [N] to #2
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
    twilio switch 3 o ON MAIN AC POWER
    # Set output format string for 'CLOSED' state [c].
    twilio switch 3 c ON BATTERY BACKUP POWER
    ```
  - Search verbs
    ```
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

###  5.7. <a name='mqtt-client-component'></a>MQTT client component
MQTT is an OASIS standard messaging protocol for the Internet of Things (IoT). It is designed as an extremely lightweight publish/subscribe messaging transport that is ideal for connecting remote devices with a small code footprint and minimal network bandwidth. MQTT today is used in a wide variety of industries, such as automotive, manufacturing, telecommunications, oil and gas, etc. See: https://mqtt.org/

- Implementation notes:
  - Each client connects under the root topic of ```ad2iot``` with a sub topic level of the devices UUID.
    - Example: ```ad2iot/41443245-4d42-4544-4410-XXXXXXXXXXXX```
  - Last Will and Testament (LWT) is used to indicate ```online```/```offline``` ```state``` of client using ```status``` topic below the device root topic.
    - Example: ```ad2iot/41443245-4d42-4544-4410-XXXXXXXXXXXX/status = {"state": "online"}```
  - Device specific info is in the ```info``` topic below the device root topic.
    - Example: ```ad2iot/41443245-4d42-4544-4410-XXXXXXXXXXXX/info = {"firmware_version":"AD2IOT-1094","cpu_model":1,"cpu_revision":1,"cpu_cores":2,"cpu_features":["WiFi","BLE","BT"],"cpu_flash_size":4194304,"cpu_flash_type":"external","ad2_version_string":"08000002,V2.2a.8.9b-306,TX;RX;SM;VZ;RF;ZX;RE;AU;3X;CG;DD;MF;L2;KE;M2;CB;DS;ER;CR","ad2_config_string":"MODE=A&CONFIGBITS=ff05&ADDRESS=18&LRR=Y&COM=N&EXP=YYNNN&REL=YNNN&MASK=ffffffff&DEDUPLICATE=N"}```
  - Partition state tracking with minimal traffic only when state changes. Each configured partition will be under the ```partitions``` topic below the device root topic.
    - Example: ```ad2iot/41443245-4d42-4544-4410-XXXXXXXXXXXX/partitions/1 =
{"ready":false,"armed_away":false,"armed_stay":false,"backlight_on":false,"programming_mode":false,"zone_bypassed":false,"ac_power":true,"chime_on":false,"alarm_event_occurred":false,"alarm_sounding":false,"battery_low":true,"entry_delay_off":false,"fire_alarm":false,"system_issue":false,"perimeter_only":false,"exit_now":false,"system_specific":3,"beeps":0,"panel_type":"A","last_alpha_messages":"SYSTEM LO BAT                   ","last_numeric_messages":"008","event":"LOW BATTERY"}```
  - Custom virtual switches with user defined topics are kept under the ```switches``` below the device root topic.
    - Example: ```ad2iot/41443245-4d42-4544-4410-XXXXXXXXXXXX/switches/RF0180036 = {"state":"RF SENSOR 0180036 CLOSED"}```
  - Zone states by Zone ID(NNN) are kept under the ```zones``` below the device root topic.
    - Example: ```ad2iot/41443245-4d42-4544-4410-XXXXXXXXXXXX/zones/003 = {"state":"CLOSE","partition":2,"name":"THIS IS ZONE 3"}```
  - Remote ```commands``` subscription. If enabled the device will subscribe to ```commands``` below the device root topic. Warning! Only enable if on a secure broker as codes will be visible to subscribers.
    - Publish JSON template ```{ "vpart": {number}, "action": "{string}", "code": "{string}", "arg": "{string}"}```
    - Example: ```ad2iot/41443245-4d42-4544-4410-XXXXXXXXXXXX/commands = {"vpart": 0, "action": "DISARM", "code": "1234"}```
    - Example: ```ad2iot/41443245-4d42-4544-4410-XXXXXXXXXXXX/commands = {"vpart": 0, "action": "BYPASS", "code": "1234", "arg": "03"}```
    - Example: ```ad2iot/41443245-4d42-4544-4410-XXXXXXXXXXXX/commands = {"vpart": 0, "action": "FIRE_ALARM"}```
    - Example(not tested) Home Assistant(HA) MQTT Alarm Control Panel yaml.
      ```
      alarm_control_panel:
        - platform: mqtt
          name: "AlarmDecoder IoT partition 1"
          availability_topic: "ad2iot/{UUID}/status"
          availability_template: "{{ value_json.state }}"
          state_topic: "ad2iot/{UUID}/partitions/1"
          value_template: >
            {% if value_json.alarm_sounding == true || value_json.alarm_event_occurred == true %}triggered
            {% elif value_json.armed_stay == true %}
              {% if value_json.entry_delay_off == true %}armed_night
              {% elif value_json.entry_delay_off == false %}armed_home
              {% endif }
            {% elif value_json.armed_away == true %}
              {% if value_json.entry_delay_off == true %}armed_vacation
              {% elif value_json.entry_delay_off == false %}armed_away
              {% endif }
            {% else %}disarmed
            {% endif %}
          command_topic: "ad2iot/{UUID}/commands"
          code: REMOTE_CODE
          # available command verbs ["DISARM", "ARM_STAY", "ARM_AWAY", "EXIT", "AUX_ALARM", "PANIC_ALARM", "FIRE_ALARM", "BYPASS", "SEND_RAW"]
          payload_arm_home: "ARM_STAY"
          payload_trigger: "PANIC_ALARM"
          command_template: "{ vpart: 0, action: '{{ action }}', code: {{ code }}} }"
      ```

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
- Enable/Disable command subscription. Do not enable on public MQTT servers!
  - ```mqtt commands [Y/N]```
  -  {arg1}: [Y]es [N]o
  - Example: ```mqtt commands Y```
- Define a smart virtual switch that will track and alert alarm panel state changes using user configurable filter and formatting rules.
  - ```mqtt switch {slot} {setting} {arg1} [arg2]```
    - {slot}
      - 1-99 : Supports multiple virtual smart alert switches.
    - {setting}
      - [-] Delete switch
      - [N] Notification sub topic path below the base
        -  Example: ```TEST``` full topic will be ```ad2iot/41443245-4d42-4544-4410-XXXXXXXXXXXX/switches/TEST```
      - [D] Default state
        - {arg1}: [0]CLOSE(OFF) [1]OPEN(ON)
      - [R] AUTO Reset.
        - {arg1}:  time in ms 0 to disable
      - [T] Message type filter.
        - {arg1}: Message type list separated by ',' or empty to disables filter.
          - Message Types: [ALPHA,LRR,REL,EXP,RFX,AUI,KPM,KPE,CRC,VER,ERR,EVENT]
            - For EVENT type the message will be generated by the API and not the AD2
      - [P] Pre filter REGEX or empty to disable.
      - [O] Open(ON) state regex search string list management.
        - {arg1}: Index # 1-8
        - {arg2}: Regex string for this slot or empty string  to clear
      - [C] Close(OFF) state regex search string list management.
        - {arg1}: Index # 1-8
        - {arg2}: Regex string for this slot or empty string to clear
      - [F] Trouble state regex search string list management.
        - {arg1}: Index # 1-8
        - {arg2}: Regex string for this slot or empty  string to clear
      - [o] Open output format string.
      - [c] Close output format string.
      - [f] Trouble output format string.

- Examples
  - Publish events to <device root>/switches/RF0180036 when the 5800 RF sensor with serial number 0123456 changes state.
  ```console
  MQTT SmartSwitch #1 report
  # Set MQTT topic [N] to 'RF0123456'.
  mqtt switch 1 N RF1234567
  # Set default virtual switch state [D] to 'CLOSED'(0)
  mqtt switch 1 D 0
  # Set auto reset time in ms [R] to 'DISABLED'
  mqtt switch 1 R 0
  # Set message type list [T]
  mqtt switch 1 T RFX
  # Set pre filter REGEX [P]
  mqtt switch 1 P !RFX:0123456,.*
  # Set 'OPEN' state REGEX Filter [O] #01.
  mqtt switch 1 O 1 !RFX:0123456,1.......
  # Set 'CLOSED' state REGEX Filter [C] #01.
  mqtt switch 1 C 1 !RFX:0123456,0.......
  # Set 'TROUBLE' state REGEX Filter [F] #01.
  mqtt switch 1 F 1 !RFX:0123456,......1.
  # Set output format string for 'OPEN' state [o].
  mqtt switch 1 o RF SENSOR 0123456 OPEN
  # Set output format string for 'CLOSED' state [c].
  mqtt switch 1 c RF SENSOR 0123456 CLOSED
  # Set output format string for 'TROUBLE' stat
  mqtt switch 1 f RF SENSOR 0123456 TROUBLE
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
./build.sh esp32 AlarmDecoder-IoT menuconfig
```

####  6.2.3. <a name='build,-flash,-and-run'></a>Build, Flash, and Run

Build the project and flash it to the board, then run monitor tool to view serial output:

```
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
!IOT: N (547) init vpart slot 0 address 18 zones '2,5'
!IOT: N (557) init vpart slot 1 address 23 zones '3,6'
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
  [ser2sockd, twilio, pushover, webui, mqtt, restart,
   netmode, button, zone, code, vpart, ad2source,
   ad2term, logmode, factory-reset, help, upgrade, version]

Type help <command> for details on each command.

```

##  8. <a name='troubleshooting'></a>Troubleshooting
- Pressing ```.``` three times early in the ESP32 boot will prevent any startup tasks. This can help if the ESP32 is in a REBOOT CRASH LOOP. To exit this mode ```'restart'```
- If the AD2* is not communicating it may be stuck in a configuration mode or its configuration may have been corrupted during firmware update of the ESP32. If this happens you can directly connect to the AD2* over the UART or Socket by using the command ```'ad2term'```.

  Note) If the connection is a Socket it is currently necessary to have the ESP32 running and not halted at boot and connected with SmartThings for Wifi and networking to be active.

- Flashing the ESP32 board fails with timeout. It seems many of the ESP32 boards need a 4-10uF cap as a buffer on the EN pin and ground. This seems to fix it very well. Repeated attempts to upload with timeouts usually works by pressing the EN button on the board a few times during upload.
