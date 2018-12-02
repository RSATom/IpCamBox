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

## Demo

* run `Test` application
* run
`gst-launch-1.0 rtspsrc location=rtsps://localhost:8100/bars tls-validation-flags=0 ! decodebin ! autovideosink`
