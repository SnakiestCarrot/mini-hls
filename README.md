# mini-hls

A small HLS live-streaming pipeline: a C++ segmenter paired with a Java
serving layer.

## Pieces

- **segmenter/** (C++, CMake) — reads a video file via libavformat/libavcodec,
  cuts it into fixed-duration `.ts` segments, writes a rolling `playlist.m3u8`
  (HLS manifest) as segments land.
- **server/** (Java, Gradle) — plain `com.sun.net.httpserver` HTTP server
  that serves the manifest + segments, simulating a CDN edge. Sends
  `Cache-Control: no-cache` on the playlist and long-lived immutable caching
  on segments.

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
./segmenter/build/segmenter <path-to-video> [--segment-duration <seconds>] [--window-size <count>]
```

- `--segment-duration` — target length per segment in seconds (default: `6.0`)
- `--window-size` — number of segments kept in the live playlist (default: `3`)

It paces itself to the source's own timestamps (not an instant file rip), so
a plain file behaves like a live source — segments land roughly one every
`--segment-duration`. The playlist keeps only the last `--window-size`
segments and omits `EXT-X-ENDLIST` until the source actually ends, matching
real live-HLS behavior. Any stale `segment*.ts`/`playlist.m3u8` left over
from a previous run in the same directory are wiped on startup.

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
