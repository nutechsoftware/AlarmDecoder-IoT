#!/usr/bin/python
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
comments = [
"OPEN RELAY 1 FAULTING ZONE 3 on Partition 2",
"CLOSE RELAY 1 RESTORING ZONE 3 on Partition 2",
"OPEN RELAY 2 FAULTING ZONE 2 on Partition 1",
"OPEN RELAY 2 RESTORING ZONE 2 on Partition 1",
]

s = socket.socket()

# Connect to AlarmDecoder IoT ser2sock port
s.connect(("192.168.3.120", 10000))

# Infinite loop CTL+C to break
while True:
  for i in range(len(commands)):
      print("sending: " + commands[i] + " - " + comments[i])

      s.send(commands[i].encode("utf8"))
      s.recv(1024)
      time.sleep(5)
