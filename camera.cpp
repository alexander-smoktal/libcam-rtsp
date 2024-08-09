#include "camera.hpp"

#include <thread>
#include <chrono>
#include <functional>

#include <spdlog/spdlog.h>

static const size_t FPS = 24;
static const auto INTERVAL = std::chrono::milliseconds(1000 / FPS);

Camera::Camera()
{
    m_manager->start();

    auto cameras = m_manager->cameras();
    if (cameras.empty())
    {
        spdlog::critical("Failed to find the camera");
        throw;
    }

    m_camera = cameras[0];
    spdlog::info("Found camera: {}", m_camera->id());

    auto config = m_camera->generateConfiguration({libcamera::StreamRole::Raw});

    if (!config)
    {
        spdlog::critical("Failed to get raw config");
        throw;
    }

    auto stream_config = config->at(0);
    spdlog::info("Config format {}", stream_config.pixelFormat.toString());

    if (m_camera->acquire() != 0)
    {
        spdlog::critical("Failed to acquire camera");
        throw;
    }

    if (m_camera->configure(config.get()) != 0)
    {
        spdlog::critical("Failed to configure camera");
        throw;
    }

    auto streams = m_camera->streams();
    if (streams.empty())
    {
        spdlog::critical("Failed to find camera streams");
        throw;
    }

    spdlog::info("Found {} streams", streams.size());
    auto stream = *streams.begin();

    spdlog::info("Stream size: {}", stream->configuration().size.toString());

    m_request = m_camera->createRequest();
    m_buffer_allocator = std::make_unique<libcamera::FrameBufferAllocator>(m_camera);

    if (int res = m_buffer_allocator->allocate(stream); res <= 0)
    {
        spdlog::critical("Failed to allocate frame: {}", res);
        m_camera->release();
        return;
    }

    m_request->addBuffer(stream, m_buffer_allocator->buffers(stream).begin()->get());

    m_camera->start();
    m_camera->requestCompleted.connect(this, &Camera::on_frame_received);

    m_worker = std::thread(std::bind(&Camera::worker_thread, this));
}

Camera::~Camera()
{
    m_worker.join();
    m_camera->stop();
    m_camera->release();
    m_camera.reset();
}

void Camera::on_frame_received(libcamera::Request *request)
{
    auto buffer = request->buffers().begin()->second;
    auto buffer_data = m_dma_mapper.readBuffer(*buffer);

    uint8_t data[4] = {buffer_data.data[0],
                       buffer_data.data[1],
                       buffer_data.data[buffer_data.size - 2],
                       buffer_data.data[buffer_data.size - 1]};

    spdlog::info("Request handled w data: {:x} {:x} {:x} {:x}", data[0], data[1], data[2], data[3]);
    m_request_pending.store(false, std::memory_order_release);
}

void Camera::worker_thread()
{
    for (auto counter = 0; counter < 13; counter++)
    {
        m_request->reuse(libcamera::Request::ReuseBuffers);

        if (!m_request_pending)
        {
            spdlog::info("Requesting a frame");
            if (m_camera->queueRequest(m_request.get()) != 0)
            {
                spdlog::warn("Failed to queue request");
            }

            m_request_pending.store(true, std::memory_order_acquire);
        }
        else
        {
            spdlog::info("Requesting is pending");
        }

        auto x = std::chrono::steady_clock::now() + INTERVAL;
        std::this_thread::sleep_until(x);
    }
}