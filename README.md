# IpCamBox
IpCams restream toolset

## Internals

```

                        +-------------------+        Protobuffers
                        |   Control Server  | <----------------------+
                        +-------------------+                        |
                                  ^                                  |
                                  |                                  |
                                  v                                  v
+---------+  RTSP Play  +-------------------+   RTSP Record   +-------------+  RTSP Play  +---------+
|   You   |------------>|  Restream Server  |<----------------|  DeviceBox  |------------>|  IpCam  |
+---------+             +-------------------+                 +-------------+             +---------+

```
## Build
* `sudo apt install git cmake libspdlog-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libgstrtspserver-1.0-dev libprotobuf-dev libcurl4-openssl-dev libssl-dev libpq-dev protobuf-compiler`
* `git clone https://github.com/RSATom/IpCamBox.git`
* `cd IpCamBox  && mkdir build && cd build && cmake .. && make -j4 && cd ..`

## Demo

* run `Test` application
* run
`gst-launch-1.0 rtspsrc location=rtsps://localhost:8100/bars tls-validation-flags=0 ! decodebin ! autovideosink`
