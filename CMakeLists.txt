cmake_minimum_required(VERSION 3.20)

project(VideoPlayback)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# 开启qt需要的uic，moc，rcc
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/ffmpeg)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/libMXF++)

if(UNIX)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/blackmagic_linux)
elseif(WIN32)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/blackmagic_win)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/VideoPlayback)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/VideoPlayback/ui)

link_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

if(WIN32)
    set(CMAKE_PREFIX_PATH "D:/developtool/qt/5.15.2/msvc2019_64")
elseif(UNIX)
    set(CMAKE_PREFIX_PATH "/home/qin/developTool/Qt5.14.0/5.14.0/gcc_64")
endif()

find_package(Qt5 COMPONENTS Core Gui OpenGL Widgets Multimedia REQUIRED)

file(GLOB_RECURSE src_file ${CMAKE_CURRENT_SOURCE_DIR}/VideoPlayback/*.cpp)
file(GLOB_RECURSE c_file ${CMAKE_CURRENT_SOURCE_DIR}/VideoPlayback/*.c)

message(STATUS "src_file: ${src_file}")

if(UNIX)
    list(APPEND src_file ${CMAKE_CURRENT_SOURCE_DIR}/include/blackmagic_linux/DeckLinkAPIDispatch.cpp)
elseif(WIN32)
    list(APPEND src_file ${CMAKE_CURRENT_SOURCE_DIR}/include/blackmagic_win/DeckLinkAPI_i.c)
endif()

if(WIN32)
    add_executable(VideoPlayback ${src_file} ${c_file})
elseif(UNIX)
    add_executable(VideoPlayback ${src_file})
endif()

if(WIN32)
    # 判断是debug还是release
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_link_libraries(VideoPlayback Qt5::Core Qt5::Gui Qt5::OpenGL Qt5::Widgets Qt5::Multimedia yuv gtestd gmockd avformat avcodec avutil swscale swresample spdlogd MXFD MXFD++)
    else()
        target_link_libraries(VideoPlayback Qt5::Core Qt5::Gui Qt5::OpenGL Qt5::Widgets Qt5::Multimedia yuv gtest gmock avformat avcodec avutil swscale swresample spdlog MXF MXF++) 
    endif()
elseif(UNIX)
    target_link_libraries(VideoPlayback Qt5::Core Qt5::Gui Qt5::OpenGL Qt5::Widgets Qt5::Multimedia pthread yuv gtest gmock avformat avcodec avutil swscale swresample z X11 fdk-aac openjp2 x264 x265 rtmp dl spdlog)
endif()
