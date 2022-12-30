# Change Log
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/)
and this project adheres to [Semantic Versioning](http://semver.org/).
## [Unreleased] Open issues

### SM - Sean Mathews coder at f34r.com
- [ ] API: Add countdown tracking for DSC/Ademco exit mode
- [ ] CORE: Needed feature ad2_fw_update() to update AD2* firmware.
- [ ] CORE: TODO: Monitor limited sockets look for ways to reduce if possible.
- [ ] CORE: Push includes down to lower level. main include has too many component specific includes.
- [ ] CORE: Audit Espressif v3.2 api usage look for more that are soon to be deprecated.
- [ ] CORE: FIXME: Setting HOST NAME when using static IP over ethernet not working.
- [ ] CORE: FIXME: reboot of esp32 causes connected ser2sock clients to hang. So far various attempts to fix have been unsuccessful. Will need to do some network captures to determine the problem.
- [ ] CORE: HUP/RESTART needs to be centralized so cleanup ex(_fifo_destroy) can happen first. How to connect with STSDK having its own restart calls.
- [ ] CORE: Noted coredump when doing oil change check and a twilio message goes out. Both are mbedtls web requests. Will need to investigate and possibly serialize web requests.
- [ ] CORE: Improve: Finish wiring Virtual Switch A & B and Button A & B.
- [ ] CORE: Wishlist: Compile for bare metal BeagleBone Black and Raspberry Pi. https://forums.freertos.org/t/freertos-porting-to-raspberry-pi-3/6686/5. Alternatively run inside an ESP32 Virtual machine on a Pi?
- [ ] CORE: TODO: better hardware abstraction. Need to remove _esp_ specific code to make it easier to port to other hardware. Trying to keep the code as POSIX as possible with the limited resources I have.
- [ ] CORE: TODO: ```'ping'``` command could come in handy. Again today needed this with ST MQTT servers seeming to be down.
- [ ] STSDK: TODO. FIX Ability to build stsdk component inside of pio build environment. Currently only possible to build with STSDK build.py script.
- [ ] STSDK: Improve: Connect Component OutputA & OutputB with switch capabilities tied to hal_
- [ ] STSDK: TODO: Add SmartThings Zone devices.
- [ ] STSDK: TODO: Find way to set IOT_PUB_QUEUE_LENGTH & IOT_QUEUE_LENGTH from 10 to 20 during build.
- [ ] TWILIO & PUSHOVER: Add virtual partition qualifier to virtual switch command. Currently on the Twilio notification is hard coded to the default virtual partition in slot 0. The Pushover notification currently has no qualifier and sends messages regardless of the partition as long as it matches. Merge these into a single pattern allowing for the user to define it by its ```vpart``` id.
- [ ] webUI: Add REST api compatible with the current webapp including a secret key. This is low priority as this method of connection is not very efficient.

### AJ
- [ ] Add a GitHub Action to run a `pio` build on every PR
- [ ] Migrate `astyle` to GitHub Action
- [ ] Update README.md to reflect `pio` build changes

---
## Releases
## [1.1.0 P2] - 2022-12-29 Sean Mathews - coder @f34rdotcom
Changes:
  - Add Travis platformio build test.
  - Fix missing settings and organized sdkconfig.defaults.
  - Improve error handling to fix null pointer crashes when processing unexpected response from Twilio rest API.
    - TODO: Find more time to audit and cleanup code.
  - Fixed some small errors in the default configuration ini file and made sure basic switches have examples in components.
  - Get STSDK building again.
### Change log
  - [ ] SM FTPD: Does not build with stsdk.
  - [ ] SM CORE: Testing modules build. All built even mqtt? Ok... I did not expect that. Needs testing.
  - [X] SM CORE: Update README.md docs on building project including stsdk and platformio.
  - [X] SM CORE: Add fixes for stsdk code that I have been sitting on. Mostly just CDECL stuff.
  - [X] SM CORE: Fix CMakeList.txt to fetch SimpleIni from github and include it for stsdk building.
  - [X] SM CORE: Travis build platformio project test.
  - [X] SM CORE: Fix sdkconfig.defaults was missing critical bits. Better organized now and "should" build same exact same firmware as in release.
  - [X] SM CORE: Fix missing compile flag ESP_HTTP_CLIENT_ENABLE_BASIC_AUTH
  - [X] SM TWILIO: Improve error handling and response data validation.
  - [X] SM CORE: Small changes to default ad2iot.ini to fix a few switch numbers and add testing for all default switches in components.
## [1.1.0 P1] - 2022-09-18 Sean Mathews - coder @f34rdotcom
Changes:
  - Partition change add new coredump partition.
    - This change will require manual flash OTA will not work. Hopefully this is the last partition change.
  - Improved top command from 1.1.0 fixed bugs and rollover issues.
  - Twilio
    - New sub command to disable or enable notification slots.
    - Expanding total notification slots from 8 to 999.
    - Memory optimization.
  - General
    - Continued work on docs.
    - Continued work on memory and CPU optimization.
### Change log
- [X] SM API: Crashes during long term testing have happened during regex parsing in the API as a result of low memory. Adding a TRY/CATCH to fail more gracefully but a better fix needs to be looked at in pushing the issue upstream to the application layer via a async call that the device is low on memory or a notification failed due to low memory. The low memory is usually been the result of some leak in one of the modules when everything is turned on all at once so some decisions need to be made on what services to use. Every service that uses SSL has a big effect on overall memory consumption when initialized.
- [X] SM STSDK: Remove stsdk json files that are not used.
- [X] SM CORE: Misc cleanup of docs. Remove redundant code in factory_reset. Bump AD2 sendQ stack from 4k to 8k to be safe but it needs more study.
- [X] SM TWILIO: Add 'disable' verb to twilio notification slots to make it easy to turn on/off. Log the info when an event is sent out to the sendQ. Max sub account change from 8 to 999. Fix missing pre 'filter' verb value not being saved into search object. Add INFO log report for each notification to help debug REGEX strings. Reduce memory remove saved notification values and instead lookup as needed.
- [X] SM CORE: Create default ad2iot.ini with settings for easy configuration over ftp.
- [X] SM CORE: Tweak partitions again. Added 64k coredump partition and expanded spiffs by 64k by reducing the two firmware image partition size by 64k. Max program size is 1,835,008 bytes.
- [X] SM CONTRIB: Add new schematics for AD2IoT main board and carrier with component values.
- [X] SM CORE: Improve top command and make optional with menuconfig.
- [X] SM FTPD: Added more sleep time to task it was using too much cpu when idle.
- [X] SM HAL: Rename esp_uart_* to uart_*, Remove esp_timer replace with hal_uptime_us().
- [X] SM CORE: Improve top to deal with overflow. TODO IDLE task will overflow so make a spacial case keep a private IDLE run tiem counter.
- [X] SM SER2SOCK: High CPU fix old bug found with new top command.
- [X] SM CORE: Refactor top and add docs. The FreeRTOS in stats reports were glitchy so it was replaced with uxTaskGetSystemState().
- [X] SM STSDK: Small fixes. Still a big mess :(
- [X] SM *: Nice task names.
- [X] SM CORE: Fix from prior change const cast.
- [X] SM CORE: Change platform.ini to be more clear on IDF version and remove old comment that is no longer relevant.
- [X] SM CORE: Add new command ```top``` to show task list and some stats. Needed something for sanity checking and tuning.
- [X] SM OTA: Remove private key and use global ca store and real world certs. For now point to www.alarmdecoder.com site and use it's cert for encryption.
- [X] SM CORE: Remove remaining cdecl that is no longer needed except for the one entrypoint from FreeRTOS.
- [X] SM CORE: Misc cleanup no code changes. Updated address_info.txt missed on last release.
## [1.1.0] - 2022-05-23 Sean Mathews - coder @f34rdotcom
Big changes:
  - Help file and docs refactor to make more user friendly.
  - Replace NVS config storage with spiffs partition and human readable ini file.
  - FTP server to allow for remote editing of the configuration file on /spiffs or /sdcard partition.
  - Migrate the redundant use of ```switch``` in each component to a global ```switch``` command and refactor existing components to use global ```switch``` settings.
  - Added support for UNKNOWN(-1) state to switch ```default``` state and ```reset``` > 0 will auto reset switch after event back to the default state.
  - Update OTA config settings so older firmware will not see this new branch. Updates to older firmware will require manual re-flashing until I get an update firmware done.
### Added
  - Commands
    - ad2config
      * Allow AD2IoT to enforce AD2pHat or attached AD2* device firmware settings.
    - switch
      * Global switches for use by all components.
      * New ```default``` value option UNKNOWN(-1).
      * auto reset to ```default``` with ```reset``` > 0 in case the open event tracked does not have a close event.
    - ftpd
      * Remote HTTP content or ad2iot.ini config file management.
  - Config using https://github.com/brofield/simpleini
### Changed
  - Yes :)

### Change log
- [X] SM - CORE: Found/fix issues in global switch command. Setting search strings for open,close,fault.
- [X] SM - API: I needed to add reset time and default value to virtual switches for my test case to work. Time is currently not tested and is only TRUE > 0 false <= 0 instead of time in ms. Found a few glitches in global switches while doing this testing.
- [X] SM - API: Default state added UNKNOWN. switch events not triggered on first state change. Still need to work on AUTO reset and default_state these will likely change this or make this more clear. For now just be sure they are in an UNKNOWN state to start. Also fixed a typo in current unused code for rest time.
- [X] SM - *: Cleanup help and docs.
- [X] SM - CORE: Continued cleanup and docs.
- [X] SM - TWILIO: Finish refactor for core config change etc.
- [X] SM - CORE: Add new command ad2config to set AlarmDecoder configuration settings to be enforced on attached AD2* device.
- [X] SM - CORE: Adding shim for doing OTA or /sdcard/fw.hex update of AD2* attached to AD2IoT device.
- [X] SM - CORE: Add ability to configure AD2* board (command 'C')  from AD2IoT so it can be configured just one time during setup. Optional or in addition add MQTT command to update config. It needs to be smart. If the only attribute in the local config is "foo=bar" then it should only attempt to change the AD2* config value "foo" if the value "bar" is different. All other AD2* config settings not specified will be left as is on the AD2* device.
- [X] SM - API: Modify getZoneType and getZoneString to return bool to detect if a zone has settings.
- [X] SM - MQTT: Send zone descriptions for all [ZONE N] entries instead of looking at partition zones list.
- [X] SM - CORE: MAX sockets error during testing. Increase LWIP MAX sockets from 10 to 16.
- [X] SM - CORE: Change short verb 'part' to full word 'partition'. I will look at adding short verbs or partial matching later but I need it to default to full words to keep it simple and clear.
- [X] SM - CORE: Remove more references to 0 index. Humans...
- [X] SM - CORE: Help refactoring to make more realistic shell interface.
- [X] SM - PUSHOVER: Help syntax refactor.
- [X] SM - PUSHOVER: Update to latest changes. Remove hard coded ssl key needed for older espressif. Update to latest config and ad2iot virtual switch templates.
- [X] SM - FTPD: Fixed stalling when client is bad and does not close a socket. Now if a new client connets it will close the current connection.
- [X] SM - FTPD: Fixed rename added response code 350 for RNFR.
- [X] SM - MQTT: misc cleanup from testing.
- [X] SM - MQTT: Unique id. Tested with multiple and only showed one. This fixed it.
- [X] SM - FTPD: Refactor to allow virtual paths to /spiffs and /sdcard folders so both uSD and spiff flash files can be modified. Other virtual folders can be added with little difficulty. Virtual path /spiffs will error with mkdir since spiffs does not allow for folders and has limits on filename sizes.
- [X] SM - CORE: Refactor partition table max config storage and app size. This fixed my boot issue and pio_fix_app_address.py so it was removed. The stnv partition was removed and hopefully I can make STSDK work with me on the name. I changed the nvs partition to 16KB . I added a 280KB spiffs partition and set the app partition size down to 1.856MB so it would all work. At boot the system will report both the sdcard and spiffs size and usage.
- [X] SM - API: Change CLOSED to CLOSE.
- [X] SM - FTPD: Add ```REST``` verb to reboot. Warning: Will not save running config. FileZilla ```Server->Enter Custom Command``` type in ```REST``` and press OK to send.
- [X] SM - *: Lots more cleanup.
- [X] SM - CORE: remove nvs config storage and change to spiffs and posix files and https://github.com/brofield/simpleini library for the config. Config will load from /sdcard/ad2iot.ini first and if that fails it will load from /spiffs/ad2iot.ini and if that fails it will continue with default settings.
- [X] SM - CORE: Partition structure change. Bump major revision. Older units must be manually flashed or run a special migration release.
- [X] SM - CORE: Add new ```switch N``` command to move switches to a global area and allow components to add extra args for internal use but use the same switch template for all modules. Less typing just define switches and then in modules like MQTT or Pushover define the module specific settings for that switch such as the output format for ```open```.
- [X] SM - ALL: Refactor configuration adding option for ini file on uSD card. I tried to make it backwards compatible and all critical configuration settings such as ```netmode``` are all compatible but partitions, codes, virtual switches all needed to be changed to be more user friendly in the ad2iot.ini config file.
- [X] SM - ALL: Remove references to virtual for partitions. It was too confusing, just part or maybe I will allow both partition and part for short.
- [X] SM - CORE: Move mounting of uSD card early after NV init to allow for loading/saving config early.
- [X] SM - WEBUI: Moved content into ```www``` folder on uSD to protect sensitive data. Users that upgrade can use FTPD to create the folder and move the files. This will be necessary when adding config files into the uSD that they are outside of the document root for the webUI component.
- [X] SM - FTPD: Adding new FTP server to allow update of uSD web pages remotely. With ACL access can be limited to a specific subnet mask or single address.
- [X] SM - STSDK: Build error with stsdk and latest stsdk build environment.
- [X] SM - MQTT: Add system flag for zone notification publish.
- [X] SM - API: Add support for system flags in zones. On Ademco special zones for system events exist such as 0xFC for "Failed Call" state.

## [1.0.9 P4] - 2022-02-22 Sean Mathews - coder @f34rdotcom
Add missing logic for MQTT ```commands``` subscription with new ```commands``` enable/disable command. Misc minor cleanup of warnings. Refactor ad2_ helpers for arming to support code or codeID. Added initial support for Auto Discovery with Home Assistant and MQTT.
### Added
  - MQTT
    - Enable/Disable feature for ```commands``` subscription with warning when enabling about security on public MQTT servers.
      - ```mqtt commands [Y|N]```
    - Missing logic behind ```commands``` subscription in the MQTT component.
      - Verbs: ```[DISARM, ARM_STAY, FW_UPDATE, ARM_AWAY, EXIT, CHIME_TOGGLE, AUX_ALARM, PANIC_ALARM, FIRE_ALARM, BYPASS, SEND_RAW, FW_UPDATE]```
    - Update README with details on ```commands```, ```tprefix``` and ```dprefix```.
    - Add topic prefix command ```tprefix```.
    - Add discovery topic prefix command ```dprefix``` for MQTT auto discovery.
### Changed
  - Change ```zone``` command to expect a json string that is not currently validated as the last argument.
    - ```{"alpha": "zone alpha descriptor", "type": "zone type string"}```.
  - Refactor utils to allow for raw codes or codeID for ARM, DISARM, etc.
  - Remove unnecessary code passed to ad2_ functions that don't need it.
  - Remove forced C code sections. Need to clean all of them up now. Not an issue with newer build environment.
  - Fixed warning on g_ad2_console_mutex.
### Change log
  - [X] SM - MISC: Fix small memory leak in virtual switch usage in Pushover, Twilio and MQTT.
  - [X] SM - MQTT: Refactor publish function to make it easy to use in the code.
  - [X] SM - HAL: Added hal wrapper for ota_do_update(hal_ota_do_update) and updated any use of ota_do_update from outside of main.
  - [X] SM - MQTT: Added commands verbs ```[DISARM, ARM_STAY, FW_UPDATE, ARM_AWAY, EXIT, CHIME_TOGGLE, AUX_ALARM, PANIC_ALARM, FIRE_ALARM, BYPASS, SEND_RAW, FW_UPDATE]```.
  - [X] SM - MQTT: Support for Home Assistant/Others auto discovery.
  - [X] SM - MQTT: Add new command ```dprefix``` to set a prefix for publishing device config documents for MQTT auto discovery.
  - [X] SM - MQTT: Add new command ```commands``` to enable/disable commands subscription.
  - [X] SM - MQTT: Remove forced cdecl calling to use C++.
  - [X] SM - MQTT: Add new command ```tprefix``` to set a prefix for all topics on the broker.
  - [X] SM - MQTT: Add new command to define signon messages to publish.
  - [X] SM - CORE: Refactor ad2_ routines to arm, disarm etc to support raw code as well as code id from ```code``` command.
  - [X] SM - CORE: ad2_utils remove forced cdecl calling to use C++ instead. TODO: I can now safely remove the extern 'C' from everything with the current build tools.
  - [X] SM - CORE: Fixed a few compiler warnings re. external with init.

## [1.0.9 P3] - 2021-11-22 Sean Mathews - coder @f34rdotcom
Continued improvements found during daily use.
### Added
  - New standard command [```factory-reset```](README.md#standard-commands) to clear NVS config partition.
  - Show panel state change JSON on CLI.
  - Twilio New [```format```](README.md#configuration-for-twilio-notifications) sub command using https://github.com/fmtlib/fmt for simple formats with positional arguments ```{}``` for auto or ```{1}``` for indexed. Allows duplicate tags ```{1}{1}```. This allows for better control of the message sent to the Twilio API depending on the API type Email, SMS, Call.
### Changed
  - Partition refactor to fill 4MB flash adding configuration and firmware storage space. Max firmware size is now 1.9M. The current firmware size for ```webui``` build is ~1.2M - 65%. Config storage partition(nvs) increased from 16KB to 44KB.
  - Refactor CLI to reduce noise and make it easier to use.
  - Pushover & Twilio allow for multiple API calls from single virtual switch event. Ex. Email + SMS Text.
  - Automatic selection of build flags for ```upgrade``` command to use current build.
  - [```version```](README.md#standard-commands) command now shows build flags.
  - API ZONE_TRACKING fix countdown messages reporting as zone faults.
  - Fix don't call subscribe api after init(s) done.
  - Bump stack to 8k on AD2 uart task same as ad2 socket client task.
### Change log
- [X] SM - CORE: Get task ID issue building with older espidf and freertos using stsdk build tool.
- [X] SM - MISC: style fixes from prior work on this release.
- [X] SM - CORE: Bump stack for AD2 uart parser to 8k. ser2sock_client_task already 8k. These task calls notifications and needs more!
- [X] SM - WEBUI & CORE: Do not call subscribe after parser is running it is not thread safe. Prevent parsing of AD2* messages until all inits are done.
- [X] SM - WEBUI: Bad var name choice no function change.
- [X] SM - API: Ademco exit countdown timer trigger zone events. Lots of zones good test :) Exclude exit messages.
- [X] SM - CORE: CLI add DELETE for some terminal configurations.
- [X] SM - TWILIO: With the ability to use multiple notification slots with notification (email, sms, call) needing different formatting it is necessary to add a new field 'format' to each notification slot.
- [X] SM - CORE: Update README.MD commands from help and some small text changes.
- [X] SM - CORE: Partition change to better use the 4MB of storage. Added 44K to NVS config partition and 400K to the two OTA firmware partitions. Existing units should flash using ESP32 flash utility but AFAIK everything is done by partition name so nothing should break for existing units with original partition. Only issue would be if the firmware gets too big but that seems not likely.
- [X] SM - CORE: Improve cli and improve task control of console. Requires adding FREERTOS_USE_TRACE_FACILITY to config.
- [X] SM - CORE: Buildflags 'stsdk' or 'webui' automatic and included in version notifications for reference.
- [X] SM - CORE: More console tidy and useability improvements.
- [X] SM - CORE: Swap portENTER_CRITICAL with taskENTER_CRITICAL. Same idea but correct use.
- [X] SM - CORE: Fix netmode command not being greedy and missing argument if it has a space such as a password :)
- [X] SM - CORE: Continued removal of verbose logging.
- [X] SM - CORE: More prefixes added to HOST uart port messages.
- [X] SM - CORE: Improve help on vpart command.
- [X] SM - MQTT: Add address_mask_filter to partition state.
- [X] SM - CORE: set ESP_LOG to call internal ad2_log_vprintf for log PREFIX formatting. Added prefix to usb host output calls.
- [X] SM - CORE: Remove ON_MESSAGE debug verbose dump added json state dump log only on alarm partition state change.
- [X] SM - CORE: Add ```address_mask_filter``` to ad2_get_partition_state_json and ad2_get_partition_zone_alerts_json as ```mask```.
- [X] SM - CORE: WIP. Add api callback for esp32 log output.
- [X] SM - CORE: Improve error handing on ad2_set nvs functions.
- [X] SM - CORE: Bump version, turn off stack event logs by default and add prefix to output messages for consistency
- [X] SM - CORE: Add  ```factory-reset``` command to clear the ```nvs``` partition and reboot.
- [X] SM - API: Fix mask in virtual partition not being updated on new events.
- [X] SM - MQTT: Add publish Contact ID event notifications to the topic ```ad2iot/<device id>/cid```. Remove excessive event logging used for development.
- [X] SM - TWILIO & PUSHOVER: Modify virtual switches to allow for multiple notification slots per event. This makes it easy to have a single event both CALL and send an email using two notification slots. Or it could notify 10 people over email etc.
- [X] SM - CORE: Some glitch with writing to NV causing ESP_ERR_NVS_NOT_ENOUGH_SPACE but plenty exists. Solution delete before save.

## [1.0.9 P2] - 2021-10-17 Sean Mathews - coder @f34rdotcom
Improve BEEP and EXIT tracking and fixed ad2term reset option.
### Changed
  - Improve BEEP and EXIT event tracking after testing on other panels.
  - Fixed reset option on ad2term command not resetting and locking up AD2*.
### Change log
- [X] SM - CORE: update license with real data not template.
- [X] SM - API: ON_BEEPS_CHANGE fix bouncing adding timeout.
- [X] SM - API: ON_EXIT_CHANGE fix to only look at ARMED messages to avoid bouncing states on system messages.
- [X] SM - CORE: ad2term reset was not working. ```#if (GPIO_AD2_RESET != GPIO_NOT_USED)``` not working? Changed to use ``` #if defined(GPIO_AD2_RESET)```.

## [1.0.9 P1] - 2021-10-12 Sean Mathews - coder @f34rdotcom
Added ON_BEEP_CHANGE. Fixed and improved ON_ZONE_CHANGE events. Added ad2_bypass_zone helper and added zone and bypass capability to webUI.
### Added
  - ON_BEEP_CHANGE event.
  - Added ad2_bypass_zone helper.
  - Show zone fault and bypass zone capability to webUI.
### Changed
  - Init virtual partitions early to keep ID's consistent.
  - Fixed some issues in zone tracking.
  - Fixed virtual zone ID's to be loaded early before any parsing of messages.
### Change log
- [X] SM - CORE: Be sure to init all virtual partitions early so they are consistent. If not configured virutal partitions ID's can change each reboot of the device.
- [X] SM - CORE: Add ad2_bypass_zone.
- [X] SM - WEBUI: Add <BYPASS> action.
- [X] SM - API: Fixed bug in zone tracking timeout outside of context.
- [X] SM - CORE: fix bug in vpart init skipped by no zone list.
- [X] SM - API: Fix bug when HEX zones 0CA(cancel) converts to 0.
- [X] SM - API: Change fire to use timeout same as zones to avoid bouncing.
- [X] SM - WEBUI: Added ON_BEEPS_CHANGE event to notifications. Add ON_ZONE_CHANGE notification and zone info to websocket state request.
- [X] SM - MQTT: Added ON_BEEPS_CHANGE event to notifications.
- [X] SM - API: Add ON_BEEPS_CHANGE.
- [X] SM - WEBUI: Add ZONE state to notification message.

## [1.0.9] - 2021-10-07 Sean Mathews - coder @f34rdotcom
Added zone change notification and several small fixes and improvements.
### Added
  - Zone tracking ON_ZONE_CHANGE events.
  - Zone alpha descriptors.
  - Zone partition assignment.
  - Multiple recipients for email notifications.
  - Delete smart switch sub commands [-] for components Twilio, Pushover, and MQTT.
  - Zone state info published over MQTT with new ON_ZONE_CHANGE events.
### Changed
  - Fix help commands causing stack fault with large blocks of text.
  - AlarmDecoder API some fixes some improvements.
  - Misc IPv4/IPv6 issues and ser2sock client issues.
  - Rename project to AlarmDecoder-IoT
### Change log
- [X] SM - CORE: Name change AlarmDecoder-STSDK -> AlarmDecoder-IoT in docs etc.
- [X] SM - CORE: Remove st-device-sdk-c no longer needed. Building for SmartThings is still done using STSDK tools.
- [X] SM - CORE: Fix stack exhaustion crash on some help commands with large text response.
- [X] SM - TWILIO: Add delete smart switch '-' command.
- [X] SM - PUSHOVER: Add delete smart switch '-' command.
- [X] SM - MQTT: Add delete smart switch '-' command.
- [X] SM - API: Add Zone tracking event triggers to AD2EventSearch class.
- [X] SM - API: Bug with 50PUL panel and comm failure message resetting fire bit exclude fire bit if msg[SYSSPECIFIC_BYTE] != '0'.
- [X] SM - TWILIO: Add support for multiple to addresses separated by commas.
- [X] SM - CORE: ad2source  with 'socket' [] needed for IPv6. RFC 3986, section 3.2.2: Host.
- [X] SM - CORE: Increase stack size of ad2uart_client adding 2k to make it 6k total now.
- [X] SM - API: Bug not reporting events on stateless events in notifySubscribers.
- [X] SM - CORE: ser2sock client code improvements and fix bug with IPv4 host address.
- [X] SM - CORE: Added ZONE list to VPART command. Only needed for DSC to associate zones to partitions.
- [X] SM - API: Add zone descriptions and zone tracking for DSC panels.
- [X] SM - API: DSC testing found some issues. Address settings are a combination of Partition and Slot# so 11 is P1 S1. Append default of slot '1' and use 'K' command.
- [X] SM - API: Add ON_CFG event handler and parsing of config and version strings.
- [X] SM - API & CORE: ad2_query_key_value -> AlarmDecoderParser::query_key_value_string. Needed in parser so moved from ad2_.
- [X] SM - API: ON_FIRE -> ON_FIRE_CHANGE for consistency.
- [X] SM - API: Change from 'FAULT' to 'TROUBLE'. Fault indicates 'OPEN' for panel documentation so stick with OPEN/CLOSE/TROUBLE.
- [X] SM - API: Add search verb for Programming and EXIT change events and removed verbs from docs that were not wired and likely will never be wired.
- [X] SM - CORE: No IPv6 dhcp address assigned notification on ethernet interface. Missing callback for IPv6.

## [1.0.8] - 2021-08-20 Sean Mathews - coder @f34rdotcom
ACL+IPv6, MQTT Client, Refactor headers etc. New simple and flexible ad2_add_http_sendQ serialized async HTTP request API for components.
### Added
  - IPv6 support with ACL(Access Control List) for webUI and ser2sockd.
  - MQTT client
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
  - New serialized async http request ad2_ api ad2_add_http_sendQ() to minimize memory usage to multiple HTTP/HTTPS push notifications.
### Changed
 - Large cleanup of headers, components, help.
 - Small changes in startup order and added global ca init early.
 - Small change to API to prevent ejecting multiple messages when the parser receives its first message. Now just send a READY state change event with all current state bits.
 - ad2_utils json helper utils refactor.
 - Migrate Pushover and Twilio/SendGrid components to use the new ad2_add_http_sendQ() api.
 - Validate and tune stack on existing tasks.
### Change log
 - [X] SM - API: Needed a way to track if it was the first message from the alarm. Added ```count``` to track how many alpha message state have been received.
 - [X] SM - STSDK: Fixed stuff that was broken by changing the API. No longer having a burst of all events on first message now component needs to do this. Tested adopting successful.
 - [X] SM - WEBUI: Cleanup a few typo's and show proper ```unknown``` state when incorrect ```vpartID``` is used as well as Waiting... message when no state is received.
 - [X] SM - WEBUI & SER2SOCKD: Default setting on ACL to 0.0.0.0/0 to allow all.
 - [X] SM - CORE: Tune profile task stacks and priority for each task.
 - [X] SM - TWILIO: Simplify Twilio/Sendgrid using new ad2_add_http_sendQ api call.
 - [X] SM - CORE: Design and implement a serialized method of making HTTP request from components through the ad2_queue_https_request api function. This will reduce memory requirements and prevent memory starvation when multiple components try to make HTTPS connections. I estimate it takes approx 30k per distinct certificate used.
 - [X] SM - PUSHOVER: Simplify Pushover using new ad2_add_http_sendQ api call. This will make it easy to add additional REST HTTP POST/GET notifications. Most of this existing code used mbedtls this conversion will use the esp_http_ api. This also should allow to switch between mbedtls to wolfssl for testing.
 - [X] SM - CORE: Help syntax cleanup. Still not happy but it is better.
 - [X] SM - MQTT: Add Last Will and Testament with a "offline" "online" indication for the client. Remove IoT system specific info to Sigon topic.
 - [X] SM - MQTT: Finish wiring 'url' command. Finished wiring up the Virtual Switches sub command 'N' defines topic to publish to. Example ```TEST``` would result in a topic of ```ad2iot/41443245-4d42-4544-4410-XXXXXXXXXXXX/TEST```.
 - [X] SM - CORE: Fix ACL CIDR MASK logic so it works for IPv6 and IPv4.
 - [X] SM - CORE: More cleanup of esp-idf component inclusion.
 - [X] SM - CORE: Large refactor to all common headers. Still needs more work but at list it is consistent.
 - [X] SM - CORE: Added helper to hal_ to return Address strings from a socket used for ACL testing.
 - [X] SM - WEBUI: Added support for IPv6 and IPv4 ACL. Supports Range, Single IP, or Mask. 1.2.3.4-1.2.3.5, 1.2.3.4, 1.2.3.4/24, 2001:0db8:85a3:0000::/64
 - [X] SM - SER2SOCKD: Added ACL logic and wired the acl command to allow an ACL string to be saved and parsed rejecting clients that don't match.
 - [X] SM - CORE: ad2_tokenize change to allow multiple tokens. Was having issues so I reverted back to strtok.
 - [X] SM - CORE: Added ad2_acl_check class to ad2_utils.
 - [X] SM - CORE: Move uSD card init from webUI into device_control and init at startup with error handling.
 - [X] SM - CORE: Minor cleanup warnings and add exclude for build to astylerc exclude.
 - [X] SM - MQTT: New component an MQTT client using ESP-MQTT with user defined events and topics and optional QOS level 2 using a uSD card for persistent storage. TLS is causing issues with random lockups but works with clear text connections. Still needs work but its a good start.
 - [X] SM - API: On first state message only fire off one event 'READY'. This prevents multiple events from firing on the first state message from the panel. Subscribe to at minimum READY event changes to be sure to catch the first event on connection to the panel.
 - [X] SM - CORE: Init the global ca store early.
 - [X] SM - CORE: Initialize virtual partitions before connecting to AD2*.
 - [X] SM - CORE: Refactor some common use calls into ad2_utils and refactor JSON library includes. Fix some callback names to be specific to avoid collisions.

## [1.0.7] - 2021-06-26 Sean Mathews - coder @f34rdotcom
 - [X] AJ - CORE: Add [PlatformIO](https://platformio.org/) (SM: AMAZING how much this has helped! Thanks!)
 - [X] SM - CORE: Improve network state management. Remove g_ad2_network_state. Tested by disconnecting network etc. Still need to improve storing of events for Twilio, Pushover when network is down for just just dropping new events if network is down. Put webUI inside of a thread to monitor for disconnects and restart if needed.
 - [X] SM - WEBUI: Connect panic buttons with 3 second resetting timers.
 - [X] SM - WEBUI: Fix default index.html page. Continued improvements.
 - [X] SM - CORE: Remove some deprecated flags from sdkconfig.defaults.
 - [X] SM - CORE: change command vpaddr to vpart as it is less confusing. Updated README commands for upgrade, fixed some spelling, rename vpaddr command to vpart.
 - [X] SM - WEBUI: Wired up the refresh button. Change arguments for webUI to be vpartID and codeID to match api calls such as ad2_disarm.
 - [X] SM - WEBUI: Found and fixed some issues found during testing multi partition and fire alarm conditions.
 - [X] SM - STSDK: Testing with latest release of st-device-sdk-c-ref v1.7 on July 26, 2021. It has been 7 months since the last release. No issues noted so far. Still using and older ESP-IDF v4.0.1-319-g79493f083 with no websocket support. With platformio build we are using v4.3.
 - [X] SM - OTA: Add ability to fetch firmware by build flags. The default is 'stsdk' but now I can publish a 'webui' build with ST disabled and websockets enabled.
 - [X] SM - WEBUI: Continued work. Disable WebSocket code if not available such as with STSDK building. UI now supports multiple partitions by specifying 'vpaddr' and 'code' on the query string and the UI will use the AD2IoT configuration settings for the 'vpaddr' and 'code'. Connected buttons to ARM AWAY/STAY, DISARM, EXIT.
 - [X] SM - API: Fixed a few bugs in the parser from I think when I switched from Arduino Strings to std::strings where the use of substr is different.
 - [X] SM - WEBUI: New module ported from ArduinoAlarmDecoder with websocket, templates and IPv4 ACL control for access. 24hr 86,400 web request load test PASS! Wiring up html and js code. Status works and screen updates in real time to show alarm state. Buttons roughed in but not sending anything yet.
 - [X] SM - CORE: Did some work on the GPIO/HAL. This still needs work and hardware testing with ethernet, uSD and AD2pHAT.
 - [X] SM - CORE: Fix cert bundle build issue if MBEDTLS_CUSTOM_CERTIFICATE_BUNDLE_PATH is enabled.
 - [X] SM - CORE: valid public certs are no longer needed but keep these old certs around for building in older ESP-IDF versions by moving to component folder.
 - [X] SM - CORE: Cleanup menuconfig removing MQTT references that need to be in component yet to be made. Moved ser2sock settings to ser2sock component.
 - [X] SM - OTA: Cleanup a little and get it working with ESP-IDF v4.3 and older v3.2.
 - [X] SM - PIO: Update to latest release v3.3.0 of platform-espressif32 now with ESP-IDF v4.3!
 - [X] SM - API: Improve tracking quirk on Ademco with system messages.
 - [X] SM - CORE: rename partition.2MB.csv to partition.4MB.csv was already a 4MB partition just wrong filename.
 - [X] SM - CORE: add "build" to .gitignore for stsdk build process.
 - [X] SM - TWILIO: Increase thread stack by 1k.
 - [X] SM - CORE: Centralize certificates. Enable MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_CMN to use a common certificate bundle and allow to add custom certificates in the new 'certs' folder. When building with older Espressif revert back to older individual certificate management api calls.
 - [X] SM - OTA: Set 10 second timeout on update socket. Change first version test from 30s after boot to 60. Fix a memory leak after failed firmware version connection.
 - [X] SM - CORE: Tune tasks for UARTS.
 - [X] SM - CORE: Change to require break sequence '...' to interrupt startup to prevent spurious data down the uart from interrupting normal boot operation.
 - [X] SM - CORE: Improve break detection from ad2term and fix some some uart driver issues I was seeing between Espressif v4.2 and v3.2.
 - [X] SM - CORE: README.MD add placeholder for building with platformio and seperate STSDK build.
 - [X] SM - CORE: Fix platformio build upload problem. The address of 0x10000 is hard coded and it needs to be 0x20000 to match the standard OTA partition schema. Refactor platformio.ini to allow building for different ESP32 boards.
 - [X] SM - CORE: Improve building newer PIO Espressif32 releases that support newer stable Espressif v4.2 or older stable v3.2. Refactor code to better keep the versions of Espressif defined inside of compiler switches for ESP_IDF_VERSION_VAL.
 - [X] SM - CORE: Running menuconfig inside of VSCODE using pio does not work. Flashes the menu screen then goes POOF!. For now test in a shell. ( PEBCAC screen size. )
 - [X] SM - CORE: Certificate store or some way to avoid having to store fixed static public keys for twilio, pushover and other REST api plugins.

## [1.0.6 P6] - 2021-04-05 Sean Mathews @f34rdotcom
- [X] CORE: Move log mode init.
- [X] API: Event format string for no match case to show event ID.
- [X] CORE: Modify changelog(this file) to include info for blame.
- [X] TWILIO: Fix syntax use newer format used in pushover module. Add simple HTTP response testing and and reporting to aid in setup.
- [X] PUSHOVER & TWILIO: Fix bug in NV storage. Someday cleanup and make the prefix an arg to the NV routines. Would require less use if 'key' var but would it be easier to read?
- [X] TWILIO: Update api.twilio.com public key. Uggg! This already changed :(. I need allow update of this key via CLI and I need to find a way to get the key to lookup the root certificate...
## [1.0.6 P5] - 2021-01-10 Sean Mathews @f34rdotcom
- [X] CORE: Improve names of components in menu config build.
- [X] STSTK: DSC panels have no special command to disarm just the code is used. Sending the code while armed disarms and while disarmed will arm. To deal with this quirk on DSC if the panel is already DISARMED and the ST Disarm button is pushed the code will not send anything to the panel.
## [1.0.6 P4] - 2021-01-08 Sean Mathews @f34rdotcom
- [ ] CORE: ad2term arg to reset AD2 device and then start the terminal. Any argument string will send hold the reset line on the AD2pHat board low for 1 second as the terminal is started causing the AD2* to hard reset. Useful when firmware on the AD2* gets corrupted and its hard to get into the boot loader.
## [1.0.6 P3] - 2021-01-08 Sean Mathews @f34rdotcom
- [X] CORE: Consolidate some common functions from Twilio and Pushover clients into ad2_utils functions.
- [X] PUSHOVER: New notification component Pushover.net. A simple HTTPS request much like SendGrid or Twilio but I prefer this command format and will be changing other commands to match soon.
## [1.0.6 P2] - 2020-12-29 Sean Mathews @f34rdotcom
- [X] CORE: refactor where serial starts to allow to enter ad2term mode early before any other  code starts. This allows to update firmware over the USB cable in ad2term mode using the GUI app or other AD2* firmware update tool.
## [1.0.6 P1] - 2020-12-21 Sean Mathews @f34rdotcom
- [X] OTA: Some change in mbedtls happend at some point causing OTA to now fail. Releaseing P1 to fix.
## [1.0.6] - 2020-12-19 Sean Mathews @f34rdotcom
- [X] SER2SOCKD: Add new component ser2sock daemon.
  - [X] README
  - [X] CMD
  - [X] INIT
  - [X] TASK
  - [X] QA/TESTING
- [X] API: Refactor api. Simplify subscriber storage class. Add new subscriber callback type. Added ON_RAW_RX_DATA so subscribers can get a raw AD2* stream needed by ser2sockd to avoid writing in ser2sock client and uart client routines.
- [X] CORE: Extract ser2sock fragments and move into ser2sock component.
## [1.0.5] - 2020-12-15 Sean Mathews @f34rdotcom
- [X] CORE: Tidy: Improve Kconfig menuconfig.
- [X] API: System specific nibble was a bit. Fixed.
- [X] API: ON_LOW_BATTERY() toggle ON/OFF with no battery on on test panel. This one keeps poping up...
- [X] CORE: Add support for Ethernet and WiFi [S]tatic or [D]ynamic '''MODE''' selection in '''netmode''' command. Static mode supports args IP,MASK,GW,DNS1,DNS2. Currently is ON/OFF settings are only used in Static mode. May later allow for override settings for a mix of DHCP+static.
- [X] CORE: Add Ethernet support for ESP32-POE-ISO board. https://raw.githubusercontent.com/OLIMEX/ESP32-POE-ISO/master/HARDWARE/ESP32-PoE-ISO-Rev.D/ESP32-PoE-ISO_Rev_D.pdf Requires patching https://github.com/vtunr/esp-idf/commit/0f8ea938d826a5aacd2f27db20c5e4edeb3e2ba9 see contrib/esp_eth_phy_lan8720.c.patch
- [X] CORE: Updated README build notes.
- [X] CORE: Refactor app_main cleanup and reorder.
- [X] CORE: Refactor app_main to allow breaking out before network loads just in case hardware is not working and it crashes.
- [X] CORE: Refactor network and event code on device_control.h(HAL). Must build Build with espidf v4.0.1+
- [X] CORE: Added 'V' verbose option to ```logmode``` command.
- [X] TWILIO: Split MODULE_init into MODULE_register_cmds MODULE_init so commands can be initialized even if not running.
- [X] STSDK: Split MODULE_init into MODULE_register_cmds MODULE_init so commands can be initialized even if not running.
- [X] CORE: Added ON_LOW_BATTERY test callback.
- [X] TWILIO: Missed log refactor after serialize refactor. No logic changes.
- [X] TWILIO: Fix ~250 byte memory leak in Twilio json code.. No logic changes.
- [X] CORE: tidy.
## [1.0.4] - 2020-11-08 Sean Mathews @f34rdotcom
- [X] CORE: Tidy QA testing.
- [X] TWILIO: Improve docs and ```twsas``` command report format as valid commands with comments.
- [X] TWILIO: Need to add ability to trigger on common states FIRE, ALARM, DISARM, ARM, CHIME etc in new twsas command.
- [X] TWILIO: Add CLI commands to allow the user to construct N number of virtual events to send messages for using the new AD2EventSearch API.
- [X] CORE: Need helper routines for doing simple template operations on strings. For now we can specify any static output message for a search alert results so it is not as critical. ex. 'twsas 1 o This is my own message for this alert'.
- [X] TWILIO: FIXME prevent multiple concurrent connections. Serialize.
- [X] TWILIO: Removed test alerts CHIME, ARM, FIRE, ALARM, LRR.
- [X] API: Added a pointer and integer arg to the search class for callback state passing.
- [X] CORE: Improve arg ad2_copy_nth_arg adding ability to parse last arg skipping spaces till EOL. Easy to grab large strings as final arguemnt in any command.
- [X] TWILIO: Commends and remove confusing constants with DEFINES.
- [X] TWILIO: Tested Email, Call and TEXT messages.
- [X] TWILIO: Refactor for 3 sub commands [M]essage [C]Call(Twiml) [E]Mail(SendGrid).
- [X] TWILIO: Add EVENT Search commands to create notifications on user defined logic.
- [X] CORE: refactor NV storage. I need to keep all slot values in a single file for easy export/import. I had them scattered all over. TODO: Still a little more and then a bunch of cleanup needed.
- [X] TWILIO: Added sendgrid pem file.
- [X] API: add some lookups for humans for enums.
- [X] CORE: Add on boot a dump of NV storage usage.
- [X] STSDK: Alarm bell ON when fire alarm is on.
- [X] CORE: Misc tidy cleanup var rename.
- [X] STSDK: Remove Alarm capability. It was not what I expected. Replaced with contact :c(.
- [X] STSDK: ARM Stay/Away and other indicators now working.
   - Issue reported on [ST forums](https://community.smartthings.com/t/securitysystem-capability-arm-fail-using-stsdk-but-disarm-works/205526). Crickets... Update. With help from ST dev team the problem went POOF! Took a bit but it looks good now. I how have all my feedback working.
- [X] API: FIX: Improve FIRE and ALARM tracking issue with state toggle.
- [X] TWILIO: TODO: Add class based command line configurable notifications to Twilio. Allow to enable/disable event messages for different event types.

## [1.0.3 P2 WIP] - 2020-10-31 (no release) Sean Mathews @f34rdotcom
- [X] API: New class AD2EventSearch and support functions to subscribe. I spent a few days on this. A bit of a unicorn hunt. I could write discrete API and CLI code for every possible message type from the AlarmDecoder protocol but this seems excessive. The task is the same on every message so a single CLI and API to create custom tracking of all messages using REGEX and simple state logic was what I ended up with. This new class when constructed becomes a virtual contact with OPEN/CLOSE/FAULT states. These states are updated based upon user supplied regex patterns and simple user provided logic hints.
- [X] API: RFX message expand hex to bin for easy parsing.
- [X] API: Added message type tracking during parse for post processing.
- [X] API: new string parse function. hex_to_binsz. Rename confusing name bit_string to bin_to_binsz.
- [X] API: Fix: found bugs when testing EXIT NOW with ST App.
- [X] API: Added ON_EXIT_CHANGE event to api.
- [X] STSDK: Added EXIT NOW state contact capability and momentary to activate EXIT mode on DSC/Ademco.
- [X] CORE: Added ad2_exit_now call to send exit now command to the panel based upon panel type.
- [X] STSDK: Wired READY TO ARM contact capability and state tracking.
- [X] API: Added BYPASS CHANGE event
- [X] STSDK: Wired BYPASS to the bypass component contact capability.
- [X] STSDK: remove battery fault contact from device profile in ST dev portal.

## [1.0.3 P1] - 2020-10-20 Sean Mathews @f34rdotcom
- [X] CORE: Add command feedback to commands that require a restart to take effect.
- [X] STSDK: Remove restart for enable. Add warnings about restarting.
- [X] CORE: Fix: build error JSON.h issue using esp4.x.
- [X] STSDK: Fix: Not enabling by default.
- [X] TWILIO: Fix: mbedtls_x509_crt_parse is broken when building esp 4.x with return code -0x2180. The PEM parsing routine expects the last byte to be null but under 4.x build it is `-`. Using EMBED_TXTFILES automatically puts a \0 at the end of the block of memory and it is included in the size of the buffer. This is the setting that is used in the 3.x build component mk file.

## [1.0.3] - 2020-10-19 Sean Mathews @f34rdotcom
- [X] STSDK: Improve: Connect Panic Alarm and Medical Alarm buttons each requires 3 taps with 5 second timeout.
- [X] STSDK: Improve: Connect DISARM button.
- [X] STSDK: Improve: Connect ARM STAY, ARM AWAY buttons.
- [X] API: Improve: POWER_CHANGE, ALARM BELL CHANGE, BATTERY CHANGE. Rename ALARM to ALARM_BELL. Home->Stay
- [X] STSDK: Improve: Add battery capability. Tested. Batter low on panel sends 0% battery. Battery OK sends 100%.
- [X] STSDK: Improve: Add Power Source capability. Tested. Shows 'mains' or 'battery' depending on alarm state.
- [X] STSDK: Improve: Connected AD2* events to capabilities:
- [X] STSDK: Improve: Cleanup add comments to capabilities declarations.
- [X] STSDK: Improve: Connect Switch A/B to hal_
- [X] CORE: Improve: Add Alarm Panel Action buttons in main component to replace securitySystem that seems to be broken on the ST cloud. Good test for dynamic update of a capability. The buttons will reflect the current panel state.
- [X] CORE: Improve: Command <strike>reboot</strike> -> restart. Much better, almost did a reboot on my host :)
- [X] CORE: Tidy: Command help and moved to \r\n on all HOST communication same as AD2*
- [X] CORE: New: Command: logmode {debug|info|none}. Set logging mode to get more | less details.
- [X] CORE: Improve: Moved esp host UART init to hal_.
- [X] CORE: Improve: New formatting utils to make it easy to build messages to send and route all host com through a one api to allow for management <strike>printf</strike>.
- [X] CORE: Improve: Refactor init sequence and error checking to get the network up before connecting to the AD2*.
- [X] STSDK: Improve: Set default 'stenable' to Y. At first boot it starts and initializes the st nvme partition allowing stserial etc to be called. It is still necessary to add the stserial etc before it will show as an AC on WiFi scanners. From the factory with Keys it will just show up at first boot.
- [X] CORE: New: DSC/Ademco support.
- [X] CORE: New: More utils. Make it easy to send '\x01\x01\x01' using <S1> macro as is done in the current AD2 webapp. upper lower case.
- [X] CORE: Less C more C++.
- [X] CORE: Fix: Switching to espidf 4.x build.
- [X] CORE: Fix deprecated stsdk api calls.
- [X] CORE: Fix: building with espidf 4.x Currently testing only on 3.x branch but will switch to 4.x as soon as some warnings are sorted out.

## [1.0.2] - 2020-10-11 Sean Mathews @f34rdotcom
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

## [1.0.1 p1] - 2020-09-27 Sean Mathews @f34rdotcom
- [X] - Added support code for 'C' mode connection.
- [X] - Refactor 'ad2source' command to allow selecting GPIO UARTS TX/RX pins.
- [X] - New command 'ad2term' to connect directly to AD2* and stop processing.
- [X] - Adding a ad2_* string util for splitting strings. Still needs const char * override.

## [1.0.1] - 2020-09-25 Sean Mathews @f34rdotcom
- [X] Code formatting and Travis CI setup on github. What a mess :)... "astyle --style=otbs"

## [1.0.0B] - 2020-09-18 Sean Mathews @f34rdotcom

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

## [1.0.0] - 2020-09-18 Sean Mathews @f34rdotcom

Initial project release for testing.

Test restults.
 - [X] OTA updates
    - TODO: Add CLI trigger
 - [X] Toggle CHIME show CHIME state
 - [X] Disarm
 - [X] Onboarding adopting into SmartThings cloud.
 - [X] CLI command line interface for settings.
 - [X] Monitor alarm state from new SmartThings app.
