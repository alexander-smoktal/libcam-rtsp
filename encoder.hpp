#pragma once

#include <cstdio>

extern "C"
{
#include <libavutil/avutil.h>
#include <libavcodec/codec.h>
#include <libavcodec/avcodec.h>
}

#include "metadata.hpp"
#include "iframe_sink.hpp"

class Encoder final : public IFrameSink
{
public:
    Encoder(Metadata metadata);
    ~Encoder();

    void push_frame(const uint8_t *data, size_t size, uint64_t pts_usec) override;
    void push_frame(const AVFrame *frame) override;

private:
    void init();

    Metadata m_metadata;

    const AVCodec *m_codec = nullptr;
    AVCodecContext *m_codec_context = nullptr;
    AVPacket *m_packet = av_packet_alloc();

    FILE *f;
};