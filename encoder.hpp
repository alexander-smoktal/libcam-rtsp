#pragma once

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

    void push_frame(uint8_t const *data, size_t size) override;
    void push_frame(const AVFrame &frame) override;

private:
    void init();

    Metadata m_metadata;

    const AVCodec *m_codec = nullptr;
    AVCodecContext *m_codec_context = nullptr;
};