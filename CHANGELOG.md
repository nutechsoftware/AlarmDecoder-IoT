# Change Log
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/)
and this project adheres to [Semantic Versioning](http://semver.org/).

## [Unreleased] WIP
- [X] CORE: Improve: New formatting utils to make it easy to build messages to send and route all host com through a one api to allow for management <strike>printf</strike>.
- [X] CORE: Improve: Refactor init sequence and error checking to get the network up before connecting to the AD2*.
- [X] STSDK: Improve: Set default 'stenable' to Y. At first boot it starts and initializes the st nvme partition allowing stserial etc to be called. It is still necessary to add the stserial etc before it will show as an AC on WiFi scanners. From the factory with Keys it will just show up at first boot.
- [X] CORE: New: DSC/Ademco support.
- [X] CORE: New: More utils. Make it easy to send '\x01\x01\x01' using <S1> macro as is done in the current AD2 webapp. upper lower case.
- [X] CORE: Less C more C++.
- [X] CORE: Fix: Switching to espidf 4.x build.
- [X] CORE: Fix deprecated stsdk api calls.
- [X] CORE: Fix: building with espidf 4.x Currently testing only on 3.x branch but will switch to 4.x as soon as some warnings are sorted out.
- [ ] CORE: TODO: Ethernet hardware enable.
- [ ] CORE: Tidy: Improve Kconfig menuconfig.
- [ ] CORE: Wishlist: Compile for bare metal BeagleBone Black and Raspberry Pi. https://forums.freertos.org/t/freertos-porting-to-raspberry-pi-3/6686/5. Alternatively run inside an ESP32 Virtual machine on a Pi?
- [ ] CORE: TODO: better hardware abstraction. Need to remove _esp_ specific code to make it easier to port to other hardware. Trying to keep the code as POSIX as possible with the limited resources I have.
- [ ] CORE: TODO: ```'ping'``` command could come in handy. Again today needed this with ST MQTT servers seeming to be down.
- [ ] STSDK: ARM Stay/Away
   - Issue reported on [ST forums](https://community.smartthings.com/t/securitysystem-capability-arm-fail-using-stsdk-but-disarm-works/205526). Crickets...
- [ ] Twilio: TODO: Add class based command line configurable notifications to Twilio. Allow to enable/disable event messages for different event types.
- [ ] STSDK: TODO: Add SmartThings Zone devices.

## [1.0.2] - 2020-10-11
- [X] CORE: New: Command: ```'netmode <[W,E,N]> <ARGS>'```. If SmartThings is disabled allow control of network settings.
- [X] CORE: Improve: Add util function ```ad2_query_key_value``` for N/V parsing. Will be used to store settings in NV in a easy to use text only way.
- [X] CORE: Improve: Filled in empty wifi init function and added ```hal_event_handler```.
- [X] CORE: Fix: Build with espidf idf.py tool
- [X] CORE: Tidy: Rename wifi and eth init functions to hal_ prefix.
- [X] CORE: Tidy: Clean up command syntax and help to share in README.md
- [X] API: Fix: ON_FIRE processing in API.
- [X] API: Improve: getAD2PState override to AlarmDecoder parse class.
- [X] Twilio: Improve: Add rate limiting to send queue.
- [X] Twilio: Improve: Add ALARM and ALARM_RESTORE.
- [X] STSDK: New: Command ```'stenable <[Y|N]>'``` Disable / Enable for STSDK module and let network be managed by main.
- [X] STSDK: New: Prevent FALSE alarms like original ST app. Internal track FIRE panic momentary press events to track and count. If it is pressed 3 times in N seconds trigger a fire. TODO update user as to the "Trigger" level via a color ICON?
- [X] STSDK: Improve: Only report on messages to the default partition for multi partition support.
- [X] STSDK: Fix: Finish wiring smokeDetector capability and test FIRE alarm to show if smoke/clear events.
- [X] STSDK: Tidy: Function name fix ```connection_start_task```.

## [1.0.1 p1] - 2020-09-27
- [X] - Added support code for 'C' mode connection.
- [X] - Refactor 'ad2source' command to allow selecting GPIO UARTS TX/RX pins.
- [X] - New command 'ad2term' to connect directly to AD2* and stop processing.
- [X] - Adding a ad2_* string util for splitting strings. Still needs const char * override.

## [1.0.1] - 2020-09-25
- [X] Code formatting and Travis CI setup on github. What a mess :)... "astyle --style=otbs"

## [1.0.0B] - 2020-09-18

Refactor to take all dev work and clean it up into components that can be turned off / on as needed for a given project.
Changes and tests.
- [X] Add TODO: Add Networking needs to be added if STSDK disabled.
- [X] REFACTOR: Move files into component folders and start fixing stuff.
- [X] REFACTOR: Refactor API to allow multiple subscribers.
- [X] REFACTOR: Ability to disable Twilio
- [X] REFACTOR: Abiiity to disable STSDK
- [X] REFACTOR: Switching everything to C++
- [X] TWILIO: Reconnect AD2 events
- [X] TWILIO: Add more event and beging message formatting work.
- [X] STSDK: Reconnect AD2 events to STSDK
- [X] TEST: OTA over ST
- [X] TEST: Disarm over ST
- [X] TEST: Chime toggle of ST
- [X] OTA: Add version report
- [X] OTA: Add update command
- [X] TEST: CLI OTA update
- [X] OTA: Add subscriber callback for ON_FIRMWARE_VERSION
- [X] REFACTOR: Started adding event id string formats for human readable.

## [1.0.0] - 2020-09-18

Initial project release for testing.

Test restults.
 - [X] OTA updates
    - TODO: Add CLI trigger
 - [X] Toggle CHIME show CHIME state
 - [X] Disarm
 - [X] Onboarding adopting into SmartThings cloud.
 - [X] CLI command line interface for settings.
 - [X] Monitor alarm state from new SmartThings app.
