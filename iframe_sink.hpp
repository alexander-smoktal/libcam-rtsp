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
    virtual void push_frame(const uint8_t *data, size_t size, uint64_t pts_usec) = 0;
    virtual void push_frame(const AVFrame *frame) = 0;
};
