cmake_minimum_required(VERSION 3.27)

project(libcam-rtsp)

set(Boost_USE_STATIC_LIBS ON)
find_package(PkgConfig)
find_package(Boost 1.81 COMPONENTS)
find_package(spdlog 1.10)

find_library(LIBFMT_LIBRARIES NAMES libfmt fmt)

pkg_check_modules(LIBCAMERA REQUIRED libcamera)
pkg_check_modules(LIBAV_CODEC REQUIRED libavcodec)
pkg_check_modules(LIBAV_FORMAT REQUIRED libavformat)
pkg_check_modules(LIBAV_FILTER REQUIRED libavfilter)
pkg_check_modules(LIBAV_UTIL REQUIRED libavutil)
pkg_check_modules(LIBAV_SWSCALE REQUIRED libswscale)

set(TARGET_NAME libcam-rtsp)

add_executable(${TARGET_NAME}
    main.cpp
    camera.cpp
    decoder.cpp
    encoder.cpp
    mmaped_dmabuf.cpp)

set_property(TARGET ${TARGET_NAME} PROPERTY CXX_STANDARD 23)

target_include_directories(${TARGET_NAME} PRIVATE ${Boost_INCLUDE_DIRS} ${spdlog_INCLUDE_DIRS}
    ${LIBCAMERA_INCLUDE_DIRS} ${LIBAV_CODEC_INCLUDE_DIRS}
    ${LIBAV_FORMAT_INCLUDE_DIRS} ${LIBAV_FILTER_INCLUDE_DIRS}
    ${LIBAV_UTIL_INCLUDE_DIRS} ${LIBAV_SWSCALE_INCLUDE_DIRS})

target_link_libraries(${TARGET_NAME} ${Boost_LIBRARIES} ${spdlog_LIBRARIES}
    ${LIBCAMERA_LIBRARIES} ${LIBFMT_LIBRARIES} ${LIBAV_CODEC_LIBRARIES}
    ${LIBAV_FORMAT_LIBRARIES} ${LIBAV_FILTER_LIBRARIES}
    ${LIBAV_UTIL_LIBRARIES} ${LIBAV_SWSCALE_LIBRARIES})

target_compile_definitions(${TARGET_NAME} PRIVATE NATIVE_CODEC=1 __STDC_CONSTANT_MACROS)
