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
    auto dmabufLength = plane.offset + plane.length;

    if (!m_mappings.contains(plane.fd.get()) || m_mappings[plane.fd.get()].dmabufLength != dmabufLength)
    {
        spdlog::info("New DMA mapping for {} fd", plane.fd.get());
        auto addr = mmap(nullptr, dmabufLength, PROT_READ, MAP_SHARED, plane.fd.get(), 0);

        PlaneData plane_data{
            .data = (uint8_t *)addr + plane.offset,
            .size = plane.length};

        m_mappings.emplace(plane.fd.get(), MappedBufferInfo{
                                               .data = std::move(plane_data),
                                               .dmabufLength = dmabufLength});
    }

    return m_mappings[plane.fd.get()].data;
}