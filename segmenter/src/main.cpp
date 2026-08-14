extern "C" {
#include <libavformat/avformat.h>
}

#include <cstdio>
#include <string>

// Segmenter entry point.
//
// Plan:
//   1. Open input (file path or v4l2 device) via avformat_open_input.
//   2. Open an output AVFormatContext per segment using the "mpegts" muxer.
//   3. Demux input packets, remux into the current segment, cutting to a
//      new segment file every N seconds (keyframe-aligned).
//   4. After each segment closes, rewrite playlist.m3u8 with the new
//      segment listed (EXT-X-TARGETDURATION, EXTINF, EXT-X-MEDIA-SEQUENCE).
int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <input>\n", argv[0]);
        return 1;
    }

    const std::string input_path = argv[1];

    AVFormatContext* input_ctx = nullptr;
    if (avformat_open_input(&input_ctx, input_path.c_str(), nullptr, nullptr) < 0) {
        std::fprintf(stderr, "failed to open input: %s\n", input_path.c_str());
        return 1;
    }

    if (avformat_find_stream_info(input_ctx, nullptr) < 0) {
        std::fprintf(stderr, "failed to read stream info\n");
        avformat_close_input(&input_ctx);
        return 1;
    }

    std::printf("opened %s, %u streams\n", input_path.c_str(), input_ctx->nb_streams);

    // TODO: segment loop (see plan above).

    avformat_close_input(&input_ctx);
    return 0;
}
