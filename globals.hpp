#pragma once

extern "C"
{
#include <libavutil/pixfmt.h>
}

static const int FPS = 25;
static const AVPixelFormat ENCODER_SRC_FORMAT = AV_PIX_FMT_YUV420P;
static const char *STREAM_URL = "rtmp://0.0.0.0";