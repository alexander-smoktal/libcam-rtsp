
#include "decoder.hpp"

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
}

#include "globals.hpp"

Decoder::Decoder(Metadata metadata)
    : m_metadata(metadata), m_encoder(std::make_unique<Encoder>(metadata))
{
    init();
}

Decoder::~Decoder()
{
    av_parser_close(m_codec_parser);
    avcodec_free_context(&m_codec_context);
    av_frame_free(&m_jpeg_frame);
    av_packet_free(&m_packet);
}

void Decoder::init()
{
    if (m_metadata.format != Format::MJPEG)
    {
        return;
    }

    m_codec = avcodec_find_decoder(AV_CODEC_ID_MJPEG);
    if (!m_codec)
    {
        spdlog::critical("Decoder '{}' not found", "JPEG");
        throw;
    }
    spdlog::info("Decoder was found succesfully: {}", m_codec->long_name);

    m_codec_parser = av_parser_init(m_codec->id);
    if (!m_codec_parser)
    {
        spdlog::critical("Failed to allocate decode parser");
        throw;
    }
    m_codec_parser->flags |= PARSER_FLAG_COMPLETE_FRAMES;
    m_codec_context = avcodec_alloc_context3(m_codec);

    auto ret = avcodec_open2(m_codec_context, m_codec, nullptr);
    if (ret < 0)
    {
        spdlog::critical("Failed to open decoder: {}", "err2str");
        throw;
    }

    spdlog::info("Decoder opened succesfully");
}

void Decoder::init_scaler()
{
    if (m_codec_context->pix_fmt < 0)
    {
        spdlog::critical("JPEG parser didn't return pix format");
        exit(1);
    }

    spdlog::debug("Decoder pix fmt: {}", m_codec_context->pix_fmt);

    m_scale_context = sws_getContext(m_metadata.width,
                                     m_metadata.height,
                                     m_codec_context->pix_fmt,
                                     m_metadata.width,
                                     m_metadata.height,
                                     ENCODER_SRC_FORMAT,
                                     SWS_BICUBIC,
                                     nullptr,
                                     nullptr,
                                     nullptr);

    if (!m_scale_context)
    {
        spdlog::critical("Failed to initialize pix format rescaler");
        throw;
    }

    m_yuv_frame->format = ENCODER_SRC_FORMAT;
    m_yuv_frame->width = m_metadata.width;
    m_yuv_frame->height = m_metadata.height;
    /* allocate the buffers for the frame data */
    if (av_frame_get_buffer(m_yuv_frame, 32) < 0)
    {
        spdlog::critical("Failed to allocate YUV frame buffers");
        throw;
    }

    spdlog::info("Rescaler initialized succesfully");
}

void Decoder::push_frame(uint8_t const *data, size_t size, uint64_t pts_usec)
{
    if (data == nullptr)
    {
        spdlog::info("Stream EOF");
        m_encoder->push_frame(nullptr);
        return;
    }

    if (fill_frame_from_jpeg(data, size))
    {
        if (covert_frame_format())
        {
            m_yuv_frame->pts = pts_usec;
            m_yuv_frame->pkt_dts = pts_usec;
            m_encoder->push_frame(m_yuv_frame);
        }
        else
        {
            spdlog::error("Failed to convert JPEG frame");
        }
    }
    else
    {
        spdlog::error("Failed to decode JPEG frame");
    }
}

void Decoder::push_frame(const AVFrame *frame)
{
    spdlog::critical("Received full-featured frame into JPEG decoder. This should never happen");
    exit(1);
}

bool Decoder::fill_frame_from_jpeg(const uint8_t *data, size_t size)
{
    auto jpeg_frame_size = find_jpeg_end(data, size);
    if (jpeg_frame_size < 0)
    {
        spdlog::error("Failed to fix frame sequence");
        return false;
    }

    auto ret = av_parser_parse2(m_codec_parser, m_codec_context, &m_packet->data, &m_packet->size,
                                data, jpeg_frame_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
    if (ret < 0)
    {
        spdlog::error("Error while parsing JPEG frame");
        return false;
    }

    spdlog::trace("Packet size: {}. Source size: {}. Read size: {}", m_packet->size, size, ret);

    if (m_packet->size)
    {
        ret = avcodec_send_packet(m_codec_context, m_packet);
        if (ret < 0)
        {
            spdlog::error("Failed to send packet");
            return false;
        }

        if (!m_scale_context)
        {
            init_scaler();
        }

        ret = avcodec_receive_frame(m_codec_context, m_jpeg_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            return false;
        }
        else if (ret < 0)
        {
            spdlog::error("Failed to decode packet");
            return false;
        }
    }
    else
    {
        spdlog::error("Empty JPEG packet");
        return false;
    }

    return true;
}

bool Decoder::covert_frame_format()
{
    auto res_lines = sws_scale(m_scale_context,
                               m_jpeg_frame->data, m_jpeg_frame->linesize,
                               0, m_metadata.height, m_yuv_frame->data, m_yuv_frame->linesize);

    if (res_lines <= 0)
    {
        spdlog::error("Failed to change frame pixel format");
        return false;
    }

    return true;
}

// FFMPEG decoder finds frame end looking for the next frame start sequence (0xffd8),
// not current frame end (0xffd9). It obviously expects a bunch of well-formated frames going sequentially
// as an input. But, we have a buffer, which is padded and may even contain zeroes at the end of the data.
// This breaks the parser. So, we find frame end ourselves and set parser flag that we send complete frames.
size_t Decoder::find_jpeg_end(uint8_t const *data, size_t size)
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
