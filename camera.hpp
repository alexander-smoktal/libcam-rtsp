#pragma once

#include <thread>
#include <memory>
#include <atomic>
#include <vector>

#include <libcamera/camera_manager.h>
#include <libcamera/camera.h>
#include <libcamera/framebuffer_allocator.h>
#include <libcamera/base/signal.h>
#include <libcamera/base/span.h>

#include "mmaped_dmabuf.hpp"
#include "transcoder.hpp"

class Camera final
{
public:
    Camera();
    Camera(const Camera &other) = delete;
    Camera &operator==(const Camera &other) = delete;
    ~Camera();

private:
    void allocate_buffers(libcamera::Stream *stream);
    libcamera::Request *next_buffer();
    void on_frame_received(libcamera::Request *request);
    void worker_thread();

    MmapedDmaBuf m_dma_mapper = {};
    std::unique_ptr<Transcoder> m_transcoder = nullptr;
    std::unique_ptr<libcamera::CameraManager>
        m_manager = std::make_unique<libcamera::CameraManager>();
    std::shared_ptr<libcamera::Camera> m_camera = nullptr;
    std::unique_ptr<libcamera::FrameBufferAllocator> m_buffer_allocator = nullptr;
    std::thread m_worker = {};
    std::vector<std::unique_ptr<libcamera::Request>> m_requests_container = {};
    std::vector<libcamera::Request *> m_available_requests = {};
};