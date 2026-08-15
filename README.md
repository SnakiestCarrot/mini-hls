# mini-hls

A small HLS live-streaming pipeline: a C++ segmenter paired with a Java
serving layer.

## Pieces

- **segmenter/** (C++, CMake) — reads a video file via libavformat/libavcodec,
  cuts it into fixed-duration `.ts` segments, writes a rolling `playlist.m3u8`
  (HLS manifest) as segments land.
- **server/** (Java, Gradle) — plain `com.sun.net.httpserver` HTTP server
  that serves the manifest + segments, simulating a CDN edge. Stretch goal:
  fake a second bitrate rendition and serve a multi-variant playlist.

## Build

One-shot:

```
./build.sh
```

Builds the segmenter (CMake) and compiles the server (Gradle). Or do each
piece by hand:

```
# C++ segmenter
cd segmenter && cmake -B build && cmake --build build

# Java server
cd server && ./gradlew compileJava
```

## Run

Segmenter takes a video file and writes `.ts` segments plus `playlist.m3u8`
into `segmenter/`:

```
./segmenter/build/segmenter <path-to-video>
```

It paces itself to the source's own timestamps (not an instant file rip), so
a plain file behaves like a live source — segments land roughly one every
`target_segment_duration` (6s by default). The playlist keeps only the last
`target_number_segments` (3 by default) and omits `EXT-X-ENDLIST` until the
source actually ends, matching real live-HLS behavior. Both are constants
in `segmenter/src/main.cpp`, not CLI flags yet.

Server serves whatever's currently in `segmenter/` over HTTP. Run it from
`server/` (it resolves segments relative to `../segmenter`):

```
cd server && ./gradlew run
```

Run both together — segmenter writing, server serving the same directory
live — for the real use case.

## Verify

Point `ffplay` or VLC at the manifest URL and confirm it plays as a live
stream:

```
ffplay http://localhost:8080/playlist.m3u8
```
