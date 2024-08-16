#include "camera.hpp"

#include <thread>
#include <chrono>
#include <functional>
#include <ctime>
#include <csignal>

#include <spdlog/spdlog.h>

#include "globals.hpp"
#include "metadata.hpp"
#include "encoder.hpp"
#include "decoder.hpp"

static const auto INTERVAL = std::chrono::milliseconds(1000 / FPS);

std::atomic_bool s_run = true;
void signal_handler(int signal)
{
    s_run = false;
}

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

    if (metadata.format == Format::MJPEG)
    {
        m_sink.reset(static_cast<IFrameSink *>(new Decoder(metadata)));
    }
    else
    {
        m_sink.reset(static_cast<IFrameSink *>(new Encoder(metadata)));
    }

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
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
}

Camera::~Camera()
{
    m_worker.join();
    m_camera->stop();
    m_requests_container.clear();
    m_buffer_allocator.reset();

    m_camera->release();
    m_camera.reset();

    m_sink->push_frame(nullptr, 0, 0);
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
    auto frame_timestamp_nsec = buffer->metadata().timestamp;
    auto sequence = buffer->metadata().sequence;

    // Fixe problem with non-monotonical pts
    if (sequence <= m_seq)
    {
        return;
    }

    m_seq = sequence;

    auto buffer_data = m_dma_mapper.readBuffer(*buffer);

    auto bytes_used = buffer->metadata().planes().begin()->bytesused;

    if (m_presentation_start_time == 0)
    {
        spdlog::debug("Frame start timestamp: {}", frame_timestamp_nsec);
        m_presentation_start_time = frame_timestamp_nsec;
    }

    uint64_t pts_usec = (frame_timestamp_nsec - m_presentation_start_time) / 1000000;

    spdlog::trace("Frame metadata bytes used: {}. Timestamp: {}, Seq: {}", bytes_used, pts_usec, sequence);

    m_sink->push_frame(buffer_data.data, bytes_used, pts_usec);
    m_available_requests.push_back(request);
}

void Camera::worker_thread()
{
    while (s_run)
    {
        auto request = next_buffer();

        if (request)
        {
            request->reuse(libcamera::Request::ReuseBuffers);

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

    spdlog::info("Stopping worker service");
}
