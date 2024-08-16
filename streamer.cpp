#include "streamer.hpp"

#include <spdlog/spdlog.h>

#include "globals.hpp"

Streamer::Streamer(const AVCodecParameters *codec_params)
{
    // print_supported_protocols();
    avformat_network_init();
    init(codec_params);
}

Streamer::~Streamer()
{
    av_write_trailer(m_format_context);
    avio_close(m_format_context->pb);
    avformat_free_context(m_format_context);
}

void Streamer::push_packet(AVPacket *packet)
{
    auto pb = m_format_context->pb;

    if (!m_format_context->pb)
    {
        return;
    }

    if (m_time_base == 0)
    {
        m_time_base = packet->pts;
    }

    packet->pts -= m_time_base;
    packet->dts -= m_time_base;

    spdlog::trace("Frame sending: {} {}", packet->pts, packet->dts);
    auto ret = av_interleaved_write_frame(m_format_context, packet);
    if (ret < 0)
    {
        spdlog::warn("Error muxing packet");
    }
}

void Streamer::init(const AVCodecParameters *codec_params)
{
    avformat_alloc_output_context2(&m_format_context, NULL, "flv", STREAM_URL); // RTMP
    if (!m_format_context)
    {
        spdlog::critical("Could not create output context");
        throw;
    }

    // Create output AVStream according to input AVStream
    AVStream *out_stream = avformat_new_stream(m_format_context, nullptr);
    avcodec_parameters_copy(out_stream->codecpar, codec_params);
    if (!out_stream)
    {
        spdlog::critical("Failed allocating output stream");
        throw;
    }

    av_dump_format(m_format_context, 0, STREAM_URL, 1);

    auto worker = std::thread(std::bind(&Streamer::connection_listener, this));
    worker.detach();
}

void Streamer::print_supported_protocols()
{
    void *opaque = NULL;
    const char *name;

    spdlog::info("Supported file protocols:\nInput:");
    while ((name = avio_enum_protocols(&opaque, 0)))
    {
        spdlog::info("  {}", name);
    }

    printf("Output:\n");
    while ((name = avio_enum_protocols(&opaque, 1)))
    {
        spdlog::info("  {}", name);
    }
}

void Streamer::connection_listener()
{
    AVDictionary *options = nullptr;
    av_dict_set(&options, "rtmp_listen", "1", 0);
    av_dict_set(&options, "rtmp_live", "live", 0);

    // while (true)
    // {
    auto ret = avio_open2(&m_format_context->pb, STREAM_URL, AVIO_FLAG_WRITE, nullptr, &options);
    if (ret < 0)
    {
        char error[AV_ERROR_MAX_STRING_SIZE] = {};
        av_strerror(ret, error, AV_ERROR_MAX_STRING_SIZE);

        spdlog::critical("Could not open output URL: {}: {}", STREAM_URL, error);
        throw;
    }

    ret = avformat_write_header(m_format_context, nullptr);
    if (ret < 0)
    {
        spdlog::critical("Error occurred when opening output URL");
        throw;
    }

    spdlog::info("Incoming connection");

    // std::this_thread::sleep_for(std::chrono::seconds(30));
}