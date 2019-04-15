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
* `sudo apt install git cmake libspdlog-dev libconfig-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libgstrtspserver-1.0-dev libprotobuf-dev libcurl4-openssl-dev libssl-dev libpq-dev protobuf-compiler`
* `git clone https://github.com/RSATom/IpCamBox.git`
* `cd IpCamBox  && mkdir build && cd build && cmake .. && make -j4 && cd ..`

## Run
* `sudo apt install gstreamer1.0-plugins-bad gstreamer1.0-rtsp gstreamer1.0-plugins-base gstreamer1.0-plugins-ugly gstreamer1.0-libav gstreamer1.0-x gstreamer1.0-tools gstreamer1.0-plugins-base-apps libconfig9`

## Demo

* run `Test` application
* run
`gst-launch-1.0 rtspsrc location=rtsps://localhost:8100/bars tls-validation-flags=0 ! decodebin ! autovideosink`

## Online Demos
* `gst-play-1.0 rtsps://ipcambox.webchimera.org:8100/dlink931`
* `gst-play-1.0 rtsps://ipcambox.webchimera.org:8100/bars`
* Browser view: http://janus.ipcambox.webchimera.org/
