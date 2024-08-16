#pragma once

extern "C"
{
#include <libavformat/avformat.h>
}

class Streamer
{
public:
    Streamer(const AVCodecParameters *codec_params);
    ~Streamer();
    void push_packet(AVPacket *packet);

private:
    void init(const AVCodecParameters *codec_params);
    void print_supported_protocols();
    void connection_listener();

    AVFormatContext *m_format_context = nullptr;
    AVIOContext *m_io_context = NULL;
    uint64_t m_time_base = 0;
};
