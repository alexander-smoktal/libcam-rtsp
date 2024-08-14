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

void Transcoder::push_frame(uint8_t *data, size_t size)
{
    AVFrame *frame = nullptr;
    if (m_decode_parser)
    {
        frame = frame_from_jpeg(data, size);
    }

    if (frame)
    {
        av_frame_free(&frame);
    }
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
    m_decode_parser->flags |= PARSER_FLAG_COMPLETE_FRAMES;

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

AVFrame *Transcoder::frame_from_jpeg(uint8_t *data, size_t size)
{
    auto jpeg_frame_size = find_jpeg_end(data, size);
    if (jpeg_frame_size < 0)
    {
        spdlog::error("Failed to fix frame sequence");
        return nullptr;
    }

    AVPacket *packet = av_packet_alloc();
    AVFrame *result = nullptr;

    auto ret = av_parser_parse2(m_decode_parser, m_decoder_context, &packet->data, &packet->size,
                                data, jpeg_frame_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
    if (ret < 0)
    {
        spdlog::error("Error while parsing JPEG frame");

        return nullptr;
    }

    spdlog::trace("Packet size: {}. Source size: {}. Read size: {}", packet->size, size, ret);

    if (packet->size)
    {
        ret = avcodec_send_packet(m_decoder_context, packet);
        if (ret < 0)
        {
            spdlog::error("Failed to send packet");
            av_packet_free(&packet);

            return nullptr;
        }

        result = av_frame_alloc();
        ret = avcodec_receive_frame(m_decoder_context, result);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            av_frame_free(&result);
            av_packet_free(&packet);

            return nullptr;
        }
        else if (ret < 0)
        {
            spdlog::error("Failed to decode packet");
            av_frame_free(&result);
            av_packet_free(&packet);

            return nullptr;
        }
    }
    else
    {
        spdlog::error("Empty JPEG packet");
        av_packet_free(&packet);

        return nullptr;
    }

    spdlog::debug("Frame successfully decoded");
    av_packet_free(&packet);
    return result;
}

// FFMPEG decoder finds frame end looking for the next frame start sequence (0xffd8),
// not current frame end (0xffd9). It obviously expects a bunch of well-formated frames going sequentially
// as an input. But, we have a buffer, which is padded and may even contain zeroes at the end of the data.
// This breaks the parser. So, we find frame end ourselves and set parser flag that we send complete frames.
size_t Transcoder::find_jpeg_end(uint8_t *data, size_t size)
{
    for (size_t n = size; n >= 3; n--)
    {
        if (n == 3)
        {
            return -1;
        }

        if (data[n - 1] == 0xd9 && data[n - 2] == 0xff)
        {
            spdlog::trace("JPEG frame true size: {}", n);
            return n;
        }
    }

    return -1;
}
