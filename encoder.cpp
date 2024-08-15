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

    static const char *FILENAME = "test.mpeg";
    // f = fopen(FILENAME, "wb");
    // if (!f)
    // {
    //     fprintf(stderr, "could not open %s\n", FILENAME);
    //     exit(1);
    // }
}

Encoder::~Encoder()
{
    static uint8_t endcode[] = {0, 0, 1, 0xb7};

    avcodec_free_context(&m_codec_context);

    // fwrite(endcode, 1, sizeof(endcode), f);
    // fclose(f);
}

void Encoder::push_frame(const uint8_t *data, size_t size, uint64_t pts_usec)
{
    spdlog::trace("Received raw frame into encoder");
    return;
}

void Encoder::push_frame(const AVFrame *frame)
{
    spdlog::trace("Received full-featured frame into encoder");

    auto ret = avcodec_send_frame(m_codec_context, frame);
    if (frame && ret < 0)
    {
        spdlog::error("Error sending a frame for encoding");
        throw;
    }

    if (!frame && ret == AVERROR_EOF)
    {
        ret = 0;
    }

    while (ret >= 0)
    {
        ret = avcodec_receive_packet(m_codec_context, m_packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0)
        {
            spdlog::error("Error during encoding");
            throw;
        }

        spdlog::trace("Encoded frame {} {}", m_packet->pts, m_packet->size);
        m_streamer->push_packet(m_packet);
        // fwrite(m_packet->data, 1, m_packet->size, f);
        av_packet_unref(m_packet);
    }

    return;
}

void Encoder::init()
{
    static const char *CODEC_NAME = "h264_v4l2m2m";

#if NATIVE_CODEC
    // m_codec = avcodec_find_encoder(AV_CODEC_ID_MPEG2VIDEO);
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

    m_streamer = std::make_unique<Streamer>(m_codec);

    spdlog::info("Coder opened succesfully");
}
