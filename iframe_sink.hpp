#pragma once

#include <cstddef>
#include <cstdint>

extern "C"
{
#include <libavutil/pixfmt.h>
#include <libavutil/frame.h>
}

class IFrameSink
{
public:
    virtual void push_frame(uint8_t const *data, size_t size) = 0;
    virtual void push_frame(const AVFrame &frame) = 0;
};
