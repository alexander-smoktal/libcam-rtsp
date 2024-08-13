#pragma once

#include <cstddef>

#include <spdlog/spdlog.h>

extern "C"
{
#include <libavutil/pixfmt.h>
}

enum class Format
{
    YUV420,
    NV12,
    MJPEG,
};

struct Metadata
{
    Format format;
    size_t width;
    size_t height;

    static Format formatFromString(std::string &format)
    {
        spdlog::debug("Pixel format string: {}", format);

        if (format.contains("MJPEG"))
        {
            return Format::MJPEG;
        }
        else if (format.contains("YUV420"))
        {
            return Format::YUV420;
        }
        else if (format.contains("NV12"))
        {
            return Format::NV12;
        }
        else
        {
            spdlog::critical("Invalid pizel format");
            throw;
        }
    }
};