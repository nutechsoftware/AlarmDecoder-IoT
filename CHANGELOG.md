# Change Log
All notable changes to this project will be documented in this file.
 
The format is based on [Keep a Changelog](http://keepachangelog.com/)
and this project adheres to [Semantic Versioning](http://semver.org/).


## [Unreleased] - 2020-09-18 WIP

Refactor to take all dev work and clean it up into components that can be turned off / on as needed for a given project.

- [X] Move files into component folders and start fixing stuff.
- [X] Refactor API to allow multiple subscribers.
- [X] Ability to disable Twilio
- [X] Abiiity to disable STSDK
  [ ] Networking needs to be added if STSDK disabled.
- [X] Switching everything to C++
- [X] Reconnect AD2 events to Twilio
- [ ] User configurable report format and event selection.
- [ ] Reconnect AD2 events to STSDK
- [ ] OTA over ST test
- [ ] Disarm over ST test
## [1.0.0] - 2020-09-18

Initial project release for testing.

Test restults.
 - [X] OTA updates
    - TODO: Add CLI trigger
 - [X] Toggle CHIME show CHIME state
 - [X] Disarm
 - [ ] ARM Stay/Away
    - Issue reported on [ST forums](https://community.smartthings.com/t/securitysystem-capability-arm-fail-using-stsdk-but-disarm-works/205526).
 - [X] Onboarding adopting into SmartThings cloud.
 - [X] CLI command line interface for settings.
 - [X] Monitor alarm state from new SmartThings app.