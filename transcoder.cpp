#include "transcoder.hpp"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/timestamp.h>
#include <libavutil/opt.h>
}

#include "globals.hpp"

Transcoder::Transcoder(Metadata metadata) : m_metadata(metadata)
{
    init_decoder();
    init_coder();
}

Transcoder::~Transcoder()
{
    avcodec_free_context(&m_coder_context);
    av_parser_close(m_decode_parser);
    avcodec_free_context(&m_decoder_context);
}

void Transcoder::init_decoder()
{
    if (m_metadata.format != Format::MJPEG)
    {
        return;
    }

    m_decoder = avcodec_find_decoder(AV_CODEC_ID_MJPEG);
    if (!m_decoder)
    {
        spdlog::critical("Decoder '{}' not found", "JPEG");
        throw;
    }
    spdlog::info("Decoder was found succesfully: {}", m_decoder->long_name);

    m_decode_parser = av_parser_init(m_decoder->id);
    if (!m_decode_parser)
    {
        spdlog::critical("Failed to allocate decode parser");
        throw;
    }

    m_decoder_context = avcodec_alloc_context3(m_decoder);

    auto ret = avcodec_open2(m_decoder_context, m_decoder, nullptr);
    if (ret < 0)
    {
        spdlog::critical("Failed to open decoder: {}", "err2str");
        throw;
    }

    spdlog::info("Decoder opened succesfully");
}

void Transcoder::init_coder()
{
    static const char *CODEC_NAME = "h264_v4l2m2m";

#if NATIVE_CODEC
    m_coder = avcodec_find_encoder(AV_CODEC_ID_H264);
#else
    m_codec = avcodec_find_encoder_by_name(CODEC_NAME);
#endif

    if (!m_coder)
    {
        spdlog::critical("Coder '{}' not found", CODEC_NAME);
        throw;
    }
    spdlog::info("Coder was found succesfully: {}", m_coder->long_name);

    m_coder_context = avcodec_alloc_context3(m_coder);
    if (!m_coder_context)
    {
        spdlog::critical("Failed to allocate coder context");
        throw;
    }

    m_coder_context->bit_rate = 4096000;
    m_coder_context->width = m_metadata.width;
    m_coder_context->height = m_metadata.height;
    m_coder_context->time_base = (AVRational){1, FPS};
    m_coder_context->framerate = (AVRational){FPS, 1};

    /* emit one intra frame every ten frames
     * check frame pict_type before passing frame
     * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
     * then gop_size is ignored and the output of encoder
     * will always be I frame irrespective to gop_size
     */
    m_coder_context->gop_size = 10;
    m_coder_context->max_b_frames = 1;

    AVPixelFormat format = AV_PIX_FMT_YUV420P;
    if (m_metadata.format == Format::NV12)
    {
        format = AV_PIX_FMT_NV12;
    }

    m_coder_context->pix_fmt = AV_PIX_FMT_YUV420P;
    // av_opt_set(m_codec_context->priv_data, "preset", "slow", 0);

    auto ret = avcodec_open2(m_coder_context, m_coder, nullptr);
    if (ret < 0)
    {
        spdlog::critical("Failed to open coder: {}", "err2str");
        throw;
    }

    spdlog::info("Coder opened succesfully");
}
