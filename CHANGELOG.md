# Change Log
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/)
and this project adheres to [Semantic Versioning](http://semver.org/).
## [Unreleased] Open issues

### SM - Sean Mathews coder at f34r.com
- [ ] TWILIO & PUSHOVER: Add virtual partition qualifier to virtual switch command. Currently on the Twilio notification is hard coded to the default virtual partition in slot 0. The Pushover notification currently has no qualifier and sends messages regardless of the partition as long as it matches. Merge these into a single pattern allowing for the user to define it by its ```vpart``` id.
- [ ] CORE: Refactor help to reduce memory usage and remove duplicate strings from the code in general.
- [ ] CORE: Audit Espressif v3.2 api usage look for more that are soon to be deprecated.
- [ ] STSDK: TODO. FIX Ability to build stsdk component inside of pio build environment. Currently only possible to build with STSDK build.py script.
- [ ] CORE: FIXME: Setting HOST NAME when using static IP over ethernet not working.
- [ ] CORE: FIXME: reboot of esp32 causes connected ser2sock clients to hang. So far various attempts to fix have been unsuccessful. Will need to do some network captures to determine the problem.
- [ ] CORE: HUP/RESTART needs to be centralized so cleanup ex(_fifo_destroy) can happen first. How to connect with STSDK having its own restart calls.
- [ ] STSDK: TODO: Find way to set IOT_PUB_QUEUE_LENGTH & IOT_QUEUE_LENGTH from 10 to 20 during build.
- [ ] CORE: Noted coredump when doing oil change check and a twilio message goes out. Both are mbedtls web requests. Will need to investigate and possibly serialize web requests.
- [ ] CORE: Need a vacuum maintenance routine for nv storage to remove dead values or format partition to factory.
- [ ] API: Add countdown tracking for DSC/Ademco exit mode
- [ ] CORE: Improve: Finish wiring Virtual Switch A & B and Button A & B.
- [ ] STSDK: Improve: Connect Component OutputA & OutputB with switch capabilities tied to hal_
- [ ] CORE: TODO: Ethernet hardware enable.
- [ ] CORE: Wishlist: Compile for bare metal BeagleBone Black and Raspberry Pi. https://forums.freertos.org/t/freertos-porting-to-raspberry-pi-3/6686/5. Alternatively run inside an ESP32 Virtual machine on a Pi?
- [ ] CORE: TODO: better hardware abstraction. Need to remove _esp_ specific code to make it easier to port to other hardware. Trying to keep the code as POSIX as possible with the limited resources I have.
- [ ] CORE: TODO: ```'ping'``` command could come in handy. Again today needed this with ST MQTT servers seeming to be down.
- [ ] STSDK: TODO: Add SmartThings Zone devices.
- [ ] webUI: Add REST api compatible with the current webapp including a secret key. This is low priority as this method of connection is not very efficient.

### AJ
- [ ] Add a GitHub Action to run a `pio` build on every PR
- [ ] Migrate `astyle` to GitHub Action
- [ ] Update README.md to reflect `pio` build changes

## [1.0.9 P3] - 2021-11-22 Sean Mathews - coder @f34rdotcom
### Added
  - New command ```factory-reset``` to clear NVS config partition.
  - Show panel state change JSON on CLI.
  - Twilio New ```format``` sub command using https://github.com/fmtlib/fmt.
### Changed
  - Partition refactor to fill 4MB flash adding configuration and firmware storage space.
  - Refactor CLI to reduce noise and make it easier to use.
  - Pushover & Twilio allow for multiple API calls from single virtual switch event. Ex. Email + SMS Text.
  - Automatic selection of build flags for ```upgrade``` command to use current build.
  - ````version``` command now shows build flags.
### Change log
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
