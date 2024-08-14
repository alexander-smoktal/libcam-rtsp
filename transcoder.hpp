#pragma once

extern "C"
{
#include <libavutil/avutil.h>
#include <libavutil/pixfmt.h>
#include <libavcodec/codec.h>
#include <libavcodec/avcodec.h>
}

#include "metadata.hpp"

class Transcoder
{
public:
    Transcoder(Metadata metadata);
    ~Transcoder();

    void push_frame(uint8_t *data, size_t size);

private:
    void init_decoder();
    void init_coder();

    AVFrame *frame_from_jpeg(uint8_t *data, size_t size);
    static size_t find_jpeg_end(uint8_t *data, size_t size);

    Metadata m_metadata;

    const AVCodec *m_decoder = nullptr;
    AVCodecParserContext *m_decode_parser = nullptr;
    AVCodecContext *m_decoder_context = nullptr;

    const AVCodec *m_coder = nullptr;
    AVCodecContext *m_coder_context = nullptr;
};