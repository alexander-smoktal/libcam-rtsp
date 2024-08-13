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

    auto config = m_camera->generateConfiguration({libcamera::StreamRole::VideoRecording});

    if (!config)
    {
        spdlog::critical("Failed to get raw config");
        throw;
    }

    auto stream_config = config->at(0);
    spdlog::info("Config {}", stream_config.toString());

    auto pixel_format = stream_config.toString();
    Metadata metadata{
        .format = Metadata::formatFromString(pixel_format),
        .width = stream_config.size.width,
        .height = stream_config.size.height};

    m_transcoder = std::make_unique<Transcoder>(metadata);

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

    allocate_buffers(stream);

    m_camera->start();
    m_camera->requestCompleted.connect(this, &Camera::on_frame_received);

    m_worker = std::thread(std::bind(&Camera::worker_thread, this));
}

Camera::~Camera()
{
    m_worker.join();
    m_camera->stop();
    m_requests_container.clear();
    m_buffer_allocator.reset();

    m_camera->release();
    m_camera.reset();
}

void Camera::allocate_buffers(libcamera::Stream *stream)
{
    m_buffer_allocator = std::make_unique<libcamera::FrameBufferAllocator>(m_camera);
    if (int res = m_buffer_allocator->allocate(stream); res <= 0)
    {
        spdlog::critical("Failed to allocate frame: {}", res);
        m_camera->release();
        throw;
    }
    spdlog::info("Allocated {} buffers", m_buffer_allocator->buffers(stream).size());

    for (auto &buffer : m_buffer_allocator->buffers(stream))
    {
        auto request = m_camera->createRequest();

        request->addBuffer(stream, buffer.get());
        m_available_requests.push_back(request.get());
        m_requests_container.emplace_back(std::move(request));
    }
}

libcamera::Request *Camera::next_buffer()
{

    if (m_available_requests.empty())
    {
        return nullptr;
    }

    auto result = m_available_requests.back();
    m_available_requests.pop_back();
    return result;
}

void Camera::on_frame_received(libcamera::Request *request)
{
    auto buffer = request->buffers().begin()->second;
    auto buffer_data = m_dma_mapper.readBuffer(*buffer);

    uint8_t data[4] = {buffer_data.data[0],
                       buffer_data.data[1],
                       buffer_data.data[buffer_data.size - 2],
                       buffer_data.data[buffer_data.size - 1]};

    spdlog::trace("Request handled w data: {:x} {:x} {:x} {:x}", data[0], data[1], data[2], data[3]);
    m_available_requests.push_back(request);
}

void Camera::worker_thread()
{
    for (auto counter = 0; counter < 13; counter++)
    {
        spdlog::trace("Iter count: {}", counter);
        auto request = next_buffer();

        if (request)
        {
            request->reuse(libcamera::Request::ReuseBuffers);

            spdlog::trace("Requesting a frame");
            if (m_camera->queueRequest(request) != 0)
            {
                spdlog::warn("Failed to queue request");
            }
        }
        else
        {
            spdlog::trace("Requesting is pending");
        }

        auto x = std::chrono::steady_clock::now() + INTERVAL;
        std::this_thread::sleep_until(x);
    }
}