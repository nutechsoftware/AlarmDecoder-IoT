#/usr/bin/python
# AlarmDecoder IoT load testing.
# Open/Close relays tied to ZONE's on a test panel
# READY/FAULT on mulitple partitions infinite loop
#
import socket
import time
commands = [
    "4112#701", # OPEN RELAY 1
    "4112#801", # CLOSE RELAY 1
    "4112#702", # OPEN RELAY 2
    "4112#802"  # CLOSE RELAY 2
]
s = socket.socket()

# Connect to AlarmDecoder IoT ser2sock port
s.connect(("192.168.3.120", 10000))

# Infinite loop CTL+C to break
while True:
  for command in commands:
      s.send(command.encode("utf8"))
      time.sleep(3)
