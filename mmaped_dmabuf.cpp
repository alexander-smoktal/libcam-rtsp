#include "mmaped_dmabuf.hpp"

#include <sys/mman.h>

#include "spdlog/spdlog.h"

MmapedDmaBuf::~MmapedDmaBuf()
{
    for (auto &[key, value] : m_mappings)
    {
        munmap(value.data.data, value.dmabufLength);
    }
}

const PlaneData &MmapedDmaBuf::readBuffer(const libcamera::FrameBuffer &buffer)
{
    auto planes = buffer.planes();

    if (planes.size() > 1)
    {
        spdlog::critical("Encountered multiplane buffer. Num planes: {}", planes.size());
        throw;
    }

    auto &plane = *planes.begin();

    if (!m_mappings.contains(plane.fd.get()))
    {
        spdlog::info("New DMA mapping for {} fd", plane.fd.get());
        auto addr = mmap(nullptr, plane.offset + plane.length, PROT_READ, MAP_SHARED, plane.fd.get(), 0);

        PlaneData plane_data{
            .data = (uint8_t *)addr + plane.offset,
            .size = plane.length};

        m_mappings.emplace(plane.fd.get(), std::move(plane_data));
    }

    return m_mappings[plane.fd.get()].data;
}