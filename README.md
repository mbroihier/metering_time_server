# time_server 


This repository contains C++ code intended for a Raspberry PI Pico W.  It builds a basic NTP server that derives time from a GPS receiver.  In this case, the GPS is a Garmin etrex with its serial interface connected to a Pico W via a USB adapter, TRENDNET TU-S9.  I implemented a crude P2303 device driver to configure and received serial data from the adapter.  TinyGPS parses the time from the etrex packets and the time is stored and relayed to NTP clients that send queries to UDP port 123.

UDP_Client_Server, contained in this repository, is a prior project that I developed that is intended to create client/server objects that broadcast their support of a service (a port number, in this case 123).  The server, on creation, broadcasts that it handles NTP.  A pico client on the same local network needing the service listens on port 9720 and when it sees the NTP service broadcast, stores the address of the server that supports it.

This is a very basic NTP server and its purpose is to provide time for a WSPR transmitter.  Ideally, WSPR transmitters transmit one second into even minutes of the hour.  So, although time is somewhat important in the protocol, there is some wiggle room in the decoding process such that time that is within a second of true UTC time is close enough.

Parts:
  - Raspberry PI Pico W
  - Computer capable of programming the Pico W

Software:
  1)  Install the Pico SDK on the development computer
  2)  git clone https://github.com/mbroihier/time_server
  3)  cd time_server
  4)  mkdir build
  5)  cd build
  6)  cmake .. -D WIFI_SSID="your ssid" -D WIFI_PASSWORD="your wifi passwork" -D UDP_PORT="123"
  7)  make
  8)  install time_server.uf2 onto the Pico W
  9)  connect the USB to the P2303 device/etrex

Once started, this application connects to the WIFI network.  Then it enumerates the attached USB device expecting it to be a P2303.  Once enough stable packets are received from the GPS, it broadcasts that it can perform the NTP service.  Any NTP client that makes a request, receives a reply with the current time.

Eventually I will replace the etrex with a more standard GPS module that is able to provide its output directly to one of the Pico serial interfaces.