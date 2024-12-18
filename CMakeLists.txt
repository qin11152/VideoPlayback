cmake_minimum_required(VERSION 3.20)

project(VideoPlayback)

set(BUILD_AS_EXECUTABLE ON)

if(BUILD_AS_EXECUTABLE)
    message("Build as EXE")
else()
    message("Build as DLL")
endif()

set(CMAKE_INCLUDE_CURRENT_DIR ON)

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

file(GLOB_RECURSE module_file ${CMAKE_CURRENT_SOURCE_DIR}/VideoPlayback/module/*.cpp)
file(GLOB_RECURSE ui_file ${CMAKE_CURRENT_SOURCE_DIR}/VideoPlayback/ui/*.cpp)
file(GLOB_RECURSE test_file ${CMAKE_CURRENT_SOURCE_DIR}/VideoPlayback/unitest/*.cpp)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/VideoPlayback src_file)

if(BUILD_AS_EXECUTABLE)
    if(WIN32)
        set(CMAKE_PREFIX_PATH "D:/developtool/qt/5.15.2/msvc2019_64")
    elseif(UNIX)
        set(CMAKE_PREFIX_PATH "/home/qin/developTool/Qt5.14.0/5.14.0/gcc_64")
    endif()

    find_package(Qt5 COMPONENTS Core Gui OpenGL Widgets Multimedia REQUIRED)

    # 开启qt需要的uic，moc，rcc
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
endif()

if(UNIX||APPLE)
    list(APPEND module_file ${CMAKE_CURRENT_SOURCE_DIR}/include/blackmagic_linux/DeckLinkAPIDispatch.cpp)
elseif(WIN32)
    list(APPEND module_file ${CMAKE_CURRENT_SOURCE_DIR}/include/blackmagic_win/DeckLinkAPI_i.c)
    list(APPEND module_file ${CMAKE_CURRENT_SOURCE_DIR}/VideoPlayback/module/MXF++/avid_mxf_info.c)
endif()

if(BUILD_AS_EXECUTABLE)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/out/bin)
    add_executable(VideoPlayback ${src_file} ${module_file} ${ui_file} ${test_file})
    if(WIN32)
        # 判断是debug还是release
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            target_link_libraries(VideoPlayback Qt5::Core Qt5::Gui Qt5::OpenGL Qt5::Widgets Qt5::Multimedia yuv gtestd gmockd avformat avcodec avutil swscale swresample spdlogd MXFD MXFD++)
        else()
            target_link_libraries(VideoPlayback Qt5::Core Qt5::Gui Qt5::OpenGL Qt5::Widgets Qt5::Multimedia yuv gtest gmock avformat avcodec avutil swscale swresample spdlog MXF MXF++) 
        endif()
    elseif(UNIX)
        message("module_file: ${module_file}")
        target_link_libraries(VideoPlayback Qt5::Core Qt5::Gui Qt5::OpenGL Qt5::Widgets Qt5::Multimedia pthread yuv gtest gmock avformat avcodec avutil swscale swresample z X11 fdk-aac openjp2 x264 x265 rtmp dl spdlog)
    endif()
else()
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/out/lib)
    list(REMOVE_ITEM src_file ${CMAKE_CURRENT_SOURCE_DIR}/VideoPlayback/main.cpp)
    list(REMOVE_ITEM module_file ${CMAKE_CURRENT_SOURCE_DIR}/VideoPlayback/module/BlackMagic/DeckLinkDeviceDiscovery/DeckLinkDeviceDiscovery.cpp)
    list(REMOVE_ITEM test_file ${CMAKE_CURRENT_SOURCE_DIR}/VideoPlayback/unitest/VideoPlaybackTest.cpp)
    add_library(VideoPlayback SHARED ${src_file} ${module_file} ${test_file})
    target_compile_definitions(VideoPlayback PRIVATE
    MY_DLL_EXPORT
    )
    target_compile_definitions(VideoPlayback PRIVATE MYLIBRARY_EXPORTS)
    if(WIN32)
        # 判断是debug还是release
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            target_link_libraries(VideoPlayback yuv gtestd gmockd avformat avcodec avutil swscale swresample spdlogd MXFD MXFD++)
        else()
            target_link_libraries(VideoPlayback yuv gtest gmock avformat avcodec avutil swscale swresample spdlog MXF MXF++) 
        endif()
    elseif(UNIX)
        target_link_libraries(VideoPlayback pthread yuv gtest gmock avformat avcodec avutil swscale swresample z X11 fdk-aac openjp2 x264 x265 rtmp dl spdlog)
    endif()
endif()