# metering_time_server 


This repository contains C++ code intended for a Raspberry PI Pico W.  It builds a basic NTP server that derives time from a GPS receiver and a metering server.  In this case, the GPS used is the m5stack Unit GPS V1.1.  It has a 3.3 v serial interface that can be directly connected to a Pico W. TinyGPSPlus parses the time from the GPS packets and the time is stored and relayed to NTP clients that send queries to UDP port 123.

In addition to replying to NTP requests, this server also responds to temperature requests to the metering server.  The metering server is a general purpose server that collects metering information (in this case temperature of the pico) and sends it to clients that make requests from port 567.  An index is sent in the request message and the server replies with the content of the metering array.

UDP_Client_Server, contained in this repository, is a prior project that I developed that is intended to create client/server objects that broadcast their support of services (port numbers, in this case 123 and 567).  The server, on creation, broadcasts that it handles NTP and metering.  A pico client on the same local network, needing one of the services, listens on port 9720 and when it sees the service broadcast, stores the address of the server that supports it.

This is a very basic NTP server and its original purpose was to provide time for a WSPR transmitter.  Ideally, WSPR transmitters transmit one second into even minutes of the hour.  So, although time is somewhat important in the protocol, there is some wiggle room in the decoding process such that time that is within a second of true UTC time is close enough.

Parts:
  - Raspberry PI Pico W
  - Computer capable of programming the Pico W
  - m5stack GPS V1.1
  - patch wires

Software:
  1)  Install the Pico SDK on the development computer
  2)  git clone https://github.com/mbroihier/metering_time_server
  3)  cd metering_time_server
  4)  mkdir build
  5)  cd build
  6)  cmake .. -D WIFI_SSID="your ssid" -D WIFI_PASSWORD="your wifi passwork" -D UDP_PORT="123" -D UDP_PORT2="567"
  7)  make
  8)  install metering_time_server.uf2 onto the Pico W (udp_metering_client.uf2 is an example client)
  9)  connect GPS power to 3.3v, pin 36 on the pico
  10) connect ground of the GPS to ground on the pico (pick one)
  11) connect transmit on the GPS to serial receive, pin 2 on the pico

When the pico is attached to power, this application connects to the WIFI network and the GPS.  Once enough stable packets are received from the GPS, it broadcasts that it can perform the NTP and metering services.  Any NTP client that makes a request, receives a reply with the current time.  Any metering client that makes a request, receives the current temperature and the number of temperature requests that have been processed by the server.
