#include "encoder.hpp"

extern "C"
{
#include <libavcodec/avcodec.h>
// #include <libavformat/avformat.h>
// #include <libavutil/timestamp.h>
#include <libavutil/opt.h>
    // #include <libavutil/pixdesc.h>
}

#include "globals.hpp"

Encoder::Encoder(Metadata metadata) : m_metadata(metadata)
{
    init();
}

Encoder::~Encoder()
{
    avcodec_free_context(&m_codec_context);
}

void Encoder::push_frame(uint8_t const *data, size_t size)
{
    spdlog::trace("Received raw frame into encoder");
    return;
}

void Encoder::push_frame(const AVFrame &frame)
{
    spdlog::trace("Received full-featured frame into encoder");
    return;
}

void Encoder::init()
{
    static const char *CODEC_NAME = "h264_v4l2m2m";

#if NATIVE_CODEC
    m_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
#else
    m_coder = avcodec_find_encoder_by_name(CODEC_NAME);
#endif

    if (!m_codec)
    {
        spdlog::critical("Coder '{}' not found", CODEC_NAME);
        throw;
    }
    spdlog::info("Coder was found succesfully: {}", m_codec->long_name);

    m_codec_context = avcodec_alloc_context3(m_codec);
    if (!m_codec_context)
    {
        spdlog::critical("Failed to allocate coder context");
        throw;
    }

    m_codec_context->bit_rate = 4096000;
    m_codec_context->width = m_metadata.width;
    m_codec_context->height = m_metadata.height;
    m_codec_context->time_base = (AVRational){1, FPS};
    m_codec_context->framerate = (AVRational){FPS, 1};

    /* emit one intra frame every ten frames
     * check frame pict_type before passing frame
     * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
     * then gop_size is ignored and the output of encoder
     * will always be I frame irrespective to gop_size
     */
    m_codec_context->gop_size = 10;
    m_codec_context->max_b_frames = 1;
    m_codec_context->pix_fmt = ENCODER_SRC_FORMAT;
    // av_opt_set(m_codec_context->priv_data, "preset", "slow", 0);

    auto ret = avcodec_open2(m_codec_context, m_codec, nullptr);
    if (ret < 0)
    {
        spdlog::critical("Failed to open coder: {}", "err2str");
        throw;
    }

    spdlog::info("Coder opened succesfully");
}
