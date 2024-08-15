#pragma once

extern "C"
{
#include <libavformat/avformat.h>
}

class Streamer
{
public:
    Streamer(const AVCodec *in_codec);
    ~Streamer();
    void push_packet(AVPacket *packet);

private:
    void init(const char *server_addr, const AVCodec *in_codec);
    void print_supported_format();

    AVFormatContext *m_format_context = nullptr;
    const AVOutputFormat *m_format = nullptr;
    AVIOContext *m_io_context = NULL;
};
