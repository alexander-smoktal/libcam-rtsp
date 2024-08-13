cmake_minimum_required(VERSION 3.27)

project(libcam-rtsp)

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.81 COMPONENTS)
find_package(spdlog 1.10)

set(TARGET_NAME libcam-rtsp)

add_executable(${TARGET_NAME} main.cpp camera.cpp mmaped_dmabuf.cpp)

set_property(TARGET ${TARGET_NAME} PROPERTY CXX_STANDARD 23)
target_include_directories(${TARGET_NAME} PRIVATE ${Boost_INCLUDE_DIRS} ${spdlog_INCLUDE_DIRS} /usr/include/libcamera/)
target_link_libraries(${TARGET_NAME} ${Boost_LIBRARIES} ${spdlog_LIBRARIES} -lcamera -lcamera-base -lfmt)