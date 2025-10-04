# time_server_m5 


This repository contains C++ code intended for a Raspberry PI Pico W.  It builds a basic NTP server that derives time from a GPS receiver.  In this case, the m5stack Unit GPS V1.1. It has a 3.3 v serial interface that can be directly connected to a Pico W. TinyGPSPlus parses the time from the GPS packets and the time is stored and relayed to NTP clients that send queries to UDP port 123.

UDP_Client_Server, contained in this repository, is a prior project that I developed that is intended to create client/server objects that broadcast their support of a service (a port number, in this case 123).  The server, on creation, broadcasts that it handles NTP.  A pico client on the same local network, needing the service, listens on port 9720 and when it sees the NTP service broadcast, stores the address of the server that supports it.

This is a very basic NTP server and its purpose is to provide time for a WSPR transmitter.  Ideally, WSPR transmitters transmit one second into even minutes of the hour.  So, although time is somewhat important in the protocol, there is some wiggle room in the decoding process such that time that is within a second of true UTC time is close enough.

Parts:
  - Raspberry PI Pico W
  - Computer capable of programming the Pico W
  - m5stack GPS V1.1
  - patch wires

Software:
  1)  Install the Pico SDK on the development computer
  2)  git clone https://github.com/mbroihier/time_server_m5
  3)  cd time_server_m5
  4)  mkdir build
  5)  cd build
  6)  cmake .. -D WIFI_SSID="your ssid" -D WIFI_PASSWORD="your wifi passwork" -D UDP_PORT="123"
  7)  make
  8)  install time_server.uf2 onto the Pico W
  9)  connect GPS power to 3.3v, pin 36 on the pico
  10) connect ground of the GPS to ground on the pico (pick one)
  11) connect transmit on the GPS to serial receive, pin 2 on the pico

When the pico is attached to power, this application connects to the WIFI network and the GPS.  Once enough stable packets are received from the GPS, it broadcasts that it can perform the NTP service.  Any NTP client that makes a request, receives a reply with the current time.
