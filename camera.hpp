#pragma once

#include <thread>
#include <memory>

#include <libcamera/camera_manager.h>
#include <libcamera/camera.h>
#include <libcamera/framebuffer_allocator.h>
#include <libcamera/base/signal.h>
#include <libcamera/base/span.h>

#include "mmaped_dmabuf.hpp"

class Camera final
{
public:
    Camera();
    Camera(const Camera &other) = delete;
    Camera &operator==(const Camera &other) = delete;
    ~Camera();

private:
    void on_frame_received(libcamera::Request *request);
    void worker_thread();

    MmapedDmaBuf m_dma_mapper = {};
    std::shared_ptr<libcamera::Camera> m_camera = nullptr;
    std::unique_ptr<libcamera::CameraManager> m_manager = std::make_unique<libcamera::CameraManager>();
    std::unique_ptr<libcamera::Request> m_request = nullptr;
    std::unique_ptr<libcamera::FrameBufferAllocator> m_buffer_allocator = nullptr;
    std::thread m_worker = {};
};