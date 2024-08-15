#include "streamer.hpp"

#include <spdlog/spdlog.h>

Streamer::Streamer(const AVCodec *in_codec)
{
    avformat_network_init();
    init("rtmp://0.0.0.0:5000", in_codec);
}

Streamer::~Streamer()
{
    av_write_trailer(m_format_context);
    avio_close(m_format_context->pb);
    avformat_free_context(m_format_context);
}

void Streamer::push_packet(AVPacket *packet)
{
    auto ret = av_interleaved_write_frame(m_format_context, packet);
    if (ret < 0)
    {
        spdlog::warn("Error muxing packet");
    }
}

void Streamer::init(const char *server_addr, const AVCodec *in_codec)
{
    avformat_alloc_output_context2(&m_format_context, NULL, "flv", server_addr); // RTMP
    if (!m_format_context)
    {
        spdlog::critical("Could not create output context");
        throw;
    }

    m_format = m_format_context->oformat;

    // Create output AVStream according to input AVStream
    AVStream *out_stream = avformat_new_stream(m_format_context, in_codec);
    if (!out_stream)
    {
        spdlog::critical("Failed allocating output stream");
        throw;
    }

    av_dump_format(m_format_context, 0, server_addr, 1);

    auto ret = avio_open(&m_format_context->pb, server_addr, AVIO_FLAG_WRITE);
    if (ret < 0)
    {
        spdlog::critical("Could not open output URL '%s'", server_addr);
        throw;
    }

    ret = avformat_write_header(m_format_context, nullptr);
    if (ret < 0)
    {
        spdlog::critical("Error occurred when opening output URL");
        throw;
    }
}

void Streamer::print_supported_format()
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