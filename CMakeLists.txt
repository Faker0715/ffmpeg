cmake_minimum_required(VERSION 3.27)
project(ffmpeg)

set(CMAKE_CXX_STANDARD 23)

set(FFMPEG_DIR /usr/local/opt/ffmpeg@5)
# 头文件搜索路径
include_directories(${FFMPEG_DIR}/include/)
# 动态链接库或静态链接库的搜索路径
link_directories(${FFMPEG_DIR}/lib/)
add_executable(ffmpeg main.cpp
        pcm2aac.cpp
        stream_writer.h
        stream_reader.h
        tools.cpp
        singleton_template.h)

target_link_libraries(ffmpeg
        swscale swresample avcodec avutil avdevice avfilter avformat
)