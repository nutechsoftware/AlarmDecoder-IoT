# AlarmDecoder SmartThings STSDK IoT Appliance

## Overview

This project provides a framework for building a network IoT appliance to monitor and control alarm panel(s) using the AD2pHAT from AlarmDecoder.com and the open source hardware ESP32-DevKit V4 or the ESP32-EVB from Olimex.

Using an Olimex ESP32-EVB-EA it takes 6 seconds :) after booting to connect to the network and start sending messages.

## How to use example

### Hardware Required

To run this example, it's recommended that you have an ESP32-DevKit V4 or Olimex development board - [ESP32-EVB-EA](https://www.olimex.com/Products/IoT/ESP32/ESP32-EVB/open-source-hardware).

#### Pin Assignment

### Configure the project

```
./build.sh esp32 AlarmDecoder-STSDK menuconfig
```

### Build, Flash, and Run

Build the project and flash it to the board, then run monitor tool to view serial output:

```
./build.sh esp32 AlarmDecoder-STSDK build flash monitor -p /dev/ttyUSB0
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Prerequisites](https://github.com/SmartThingsCommunity/st-device-sdk-c-ref#prerequisites) for full steps to configure and use ESP-IDF and the STSDK to build this project.

## Example Output

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
