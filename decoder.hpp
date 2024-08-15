#pragma once

#include <memory>

extern "C"
{
#include <libavutil/pixfmt.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

#include "metadata.hpp"
#include "iframe_sink.hpp"
#include "encoder.hpp"

class Decoder final : public IFrameSink
{
public:
    Decoder(Metadata metadata);
    ~Decoder();

    void push_frame(const uint8_t *data, size_t size, uint64_t pts_usec) override;
    void push_frame(const AVFrame *frame) override;

private:
    void init();
    void init_scaler();

    bool fill_frame_from_jpeg(uint8_t const *data, size_t size);
    bool covert_frame_format();
    static size_t find_jpeg_end(uint8_t const *data, size_t size);

    Metadata m_metadata;

    std::unique_ptr<Encoder> m_encoder;

    // Codec
    const AVCodec *m_codec = nullptr;
    AVCodecParserContext *m_codec_parser = nullptr;
    AVCodecContext *m_codec_context = nullptr;

    // Format converter
    SwsContext *m_scale_context = nullptr;

    // Data
    AVPacket *m_packet = av_packet_alloc();
    AVFrame *m_jpeg_frame = av_frame_alloc();
    AVFrame *m_yuv_frame = av_frame_alloc();
};