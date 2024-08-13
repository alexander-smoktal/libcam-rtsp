#pragma once

#include <unordered_map>
#include <cstdio>
#include <span>

#include <libcamera/framebuffer.h>

struct PlaneData
{
    uint8_t *data;
    size_t size;
};

struct MappedBufferInfo
{
    PlaneData data;
    size_t dmabufLength;
};

class MmapedDmaBuf
{
public:
    MmapedDmaBuf() = default;
    MmapedDmaBuf(const MmapedDmaBuf &other) = delete;
    MmapedDmaBuf &operator==(const MmapedDmaBuf &other) = delete;
    ~MmapedDmaBuf();

    const PlaneData &readBuffer(const libcamera::FrameBuffer &buffer);

private:
    std::unordered_map<int, MappedBufferInfo> m_mappings = {};
};