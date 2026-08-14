# mini-hls

A small HLS live-streaming pipeline: a C++ segmenter paired with a Java
serving layer.

## Pieces

- **segmenter/** (C++, CMake) — reads a video file or v4l2 device via
  libavformat/libavcodec, cuts it into fixed-duration `.ts` segments, writes
  a rolling `playlist.m3u8` (HLS manifest) as segments land.
- **server/** (Java, Gradle) — plain `com.sun.net.httpserver` HTTP server
  that serves the manifest + segments, simulating a CDN edge. Stretch goal:
  fake a second bitrate rendition and serve a multi-variant playlist.

## Verify

Point `ffplay` or VLC at the manifest URL and confirm it plays as a live
stream:

```
ffplay http://localhost:8080/playlist.m3u8
```

## Build

```
# C++ segmenter
cd segmenter && cmake -B build && cmake --build build

# Java server
cd server && ./gradlew run
```
