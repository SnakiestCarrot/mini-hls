
#include <algorithm>
#include <libavutil/avutil.h>
#include <libavutil/rational.h>
#include <ostream>
extern "C" {
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
#include <libavformat/avio.h>
#include <libavformat/avformat.h>
}

#include <cstdio>
#include <cstdint>
#include <cmath>
#include <string>
#include <deque>
#include <fstream>
#include <chrono>
#include <thread>
#include <filesystem>

struct SegmentInfo {
    std::string filename;
    double duration;
};

void write_playlist(const std::deque<SegmentInfo>& segments, double max_segment_duration, int64_t first_segment_index, bool is_end) {
    std::string playlist_str = "";
    int64_t segment_duration = static_cast<int>(std::ceil(max_segment_duration));
    
    playlist_str.append(
        "#EXTM3U\n#EXT-X-VERSION:3\n#EXT-X-TARGETDURATION:" + std::to_string(segment_duration) +"\n#EXT-X-MEDIA-SEQUENCE:" + std::to_string(first_segment_index) + "\n"
    );

    for (size_t i = 0; i < segments.size(); i++) {
        SegmentInfo s = segments.at(i);
        playlist_str.append("#EXTINF:" + std::to_string(s.duration) + "\n" + s.filename + "\n");
    }
    if (is_end) {
        playlist_str.append("#EXT-X-ENDLIST\n");
    }
    std::ofstream ofs("playlist.m3u8.tmp");
    ofs << playlist_str;
    ofs.close();
    if (std::rename("playlist.m3u8.tmp", "playlist.m3u8")) {
        std::fprintf(stderr, "Failed to replace playlist file.\n");
    } 
}

AVFormatContext* start_segment(AVFormatContext* input_ctx, std::string segment_name) { 
    AVFormatContext* out_ctx = nullptr;
    avformat_alloc_output_context2(&out_ctx, nullptr, "mpegts", segment_name.c_str()); 

 
    for (unsigned i = 0; i < input_ctx->nb_streams; i++) {
        AVStream* in_stream = input_ctx->streams[i];
        AVStream* out_stream = avformat_new_stream(out_ctx, nullptr);
        avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
        out_stream->time_base = in_stream->time_base;
    }

    avio_open(&out_ctx->pb, segment_name.c_str(), AVIO_FLAG_WRITE);
    avformat_write_header(out_ctx, nullptr);
    return out_ctx;
}

void end_segment(AVFormatContext* out_ctx) {
    av_write_trailer(out_ctx);
    avio_closep(&out_ctx->pb);
    avformat_free_context(out_ctx);
}

void clean_stale_output(const std::filesystem::path& dir) {
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        const std::string name = entry.path().filename().string();
        bool is_stale_segment = name.rfind("segment", 0) == 0 && name.size() > 3
            && name.compare(name.size() - 3, 3, ".ts") == 0;
        bool is_stale_playlist = name == "playlist.m3u8" || name == "playlist.m3u8.tmp";
        if (is_stale_segment || is_stale_playlist) {
            std::filesystem::remove(entry.path());
        }
    }
}

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
    std::string input_path;
    double target_segment_duration = 6.0;
    int64_t target_number_segments = 3;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--segment-duration" && i + 1 < argc) {
            target_segment_duration = std::stod(argv[++i]);
        } else if (arg == "--window-size" && i + 1 < argc) {
            target_number_segments = std::stoll(argv[++i]);
        } else if (input_path.empty()) {
            input_path = arg;
        } else {
            std::fprintf(stderr, "unrecognized argument: %s\n", arg.c_str());
            return 1;
        }
    }

    if (input_path.empty()) {
        std::fprintf(stderr,
            "usage: %s <input> [--segment-duration <seconds>] [--window-size <count>]\n"
            "  --segment-duration  target length per segment in seconds (default: 6.0)\n"
            "  --window-size       number of segments kept in the live playlist (default: 3)\n",
            argv[0]);
        return 1;
    }

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

    int video_stream_index = -1;
    for (int i = 0; i < input_ctx->nb_streams; i++) {
        AVMediaType type = input_ctx->streams[i]->codecpar->codec_type;
        if (type == AVMEDIA_TYPE_VIDEO) {
               video_stream_index = i;
               break;
        }
    }

    clean_stale_output(std::filesystem::current_path());

    std::deque<SegmentInfo> segments;

    AVPacket* pkt = av_packet_alloc();   // allocate once, outside the loop
    
    std::int64_t segment_index = 0;
    std::int64_t cur_segment_start_pts = 0;

    std::string segment_name = "segment" + std::to_string(segment_index) + ".ts";

    AVFormatContext* out_ctx = start_segment(input_ctx, segment_name);

    int64_t cur_first_segment_index = 0;
    std::int64_t last_video_pts = 0;
    std::int64_t last_video_pkt_duration = 0;
    double max_duration = 0.0;
    bool is_capture_ended = false;

    auto capture_start_time = std::chrono::steady_clock::now();
    bool first_video_pkt_seen = false;
    std::int64_t first_video_pts = 0;

    while (av_read_frame(input_ctx, pkt) >= 0) {
        bool is_video_pkt = pkt->stream_index == video_stream_index;
        bool is_keyframe_pkt = pkt->flags & AV_PKT_FLAG_KEY;

        if (is_video_pkt) {
            last_video_pts = pkt->pts;
            last_video_pkt_duration = pkt->duration;

            if (!first_video_pkt_seen) {
                first_video_pts = pkt->pts;
                first_video_pkt_seen = true;
            }

            double stream_elapsed = (pkt->pts - first_video_pts) * av_q2d(input_ctx->streams[video_stream_index]->time_base);
            double wall_elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - capture_start_time).count();
            if (stream_elapsed > wall_elapsed) {
                std::this_thread::sleep_for(std::chrono::duration<double>(stream_elapsed - wall_elapsed));
            }
        }

        if (is_video_pkt && is_keyframe_pkt) {
            double elapsed_time = (pkt->pts - cur_segment_start_pts) * av_q2d(input_ctx->streams[video_stream_index]->time_base);
            if (elapsed_time >= target_segment_duration) {
                max_duration = std::max(max_duration, elapsed_time);
                end_segment(out_ctx);
                
                SegmentInfo cur_seg = SegmentInfo(segment_name, elapsed_time);
                segments.push_back(cur_seg);
                
                if (segments.size() > target_number_segments) {
                    segments.pop_front();
                    cur_first_segment_index++;
                }
                
                write_playlist(segments, max_duration, cur_first_segment_index, is_capture_ended);
                segment_index++;
                segment_name = "segment" + std::to_string(segment_index) + ".ts";

                cur_segment_start_pts = pkt->pts;

                out_ctx = start_segment(input_ctx, segment_name); 
            }
        }

        av_packet_rescale_ts(pkt,
            input_ctx->streams[pkt->stream_index]->time_base,
            out_ctx->streams[pkt->stream_index]->time_base);

        av_interleaved_write_frame(out_ctx, pkt);

        
        

        av_packet_unref(pkt);   // release pkt's internal buffer, ready for reuse
    }
    is_capture_ended = true;

    double last_segment_duration = (last_video_pts + last_video_pkt_duration - cur_segment_start_pts) * av_q2d(input_ctx->streams[video_stream_index]->time_base);
    SegmentInfo cur_seg = SegmentInfo(segment_name, last_segment_duration);
    max_duration = std::max(max_duration, last_segment_duration);
    end_segment(out_ctx);
    segments.push_back(cur_seg);
 
    if (segments.size() > target_number_segments) {
        segments.pop_front();
        cur_first_segment_index++;
    }

    write_playlist(segments, max_duration, cur_first_segment_index, is_capture_ended);
    
    av_packet_free(&pkt);

    avformat_close_input(&input_ctx);
    return 0;
}
