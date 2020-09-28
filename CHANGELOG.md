# Change Log
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/)
and this project adheres to [Semantic Versioning](http://semver.org/).

## [Unreleased] WIP
- [ ] WISHLIST: Compile for bare metal BeagleBone Black and Raspberry Pi. https://forums.freertos.org/t/freertos-porting-to-raspberry-pi-3/6686/5. Alternatively run inside an ESP32 Virtual machine on a Pi?
- [ ] TODO: better hardware abstraction. Need to remove _esp_ specific code to make it easier to port to other hardware. Trying to keep the code as POSIX as possible with the limited resources I have.
- [ ] TODO: network settings commands Wifi, Ethernet MODE select and settings.
- [ ] TODO: ````ping``` command could come in handy. Again today needed this with ST MQTT servers seeming to be down.
- [X] TODO: Build with espidf idf.py tool
- [?] Wireup smokeDetector capability and test FIRE alarm to show if triggered.
- [ ] Internal track FIRE panic momentary press events to track and count. If it is pressed 3 times in N seconds trigger a fire. TODO update user as to the "Trigger" level via a color ICON?
- [ ] ARM Stay/Away
   - Issue reported on [ST forums](https://community.smartthings.com/t/securitysystem-capability-arm-fail-using-stsdk-but-disarm-works/205526). Crickets...

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
