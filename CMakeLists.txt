cmake_minimum_required(VERSION 3.10)
project(AudioCapture)


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

SET(OUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bin)
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUT_DIR})
SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUT_DIR})
SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUT_DIR})

# Добавляем исполняемый файл
file(GLOB SOURCES "src/*.cpp")

add_executable(AudioCapture ${SOURCES})

# Включаем папку с заголовками
target_include_directories(AudioCapture PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/miniaudio
)

target_include_directories(${PROJECT_NAME} PRIVATE include)

# Windows библиотеки для miniaudio
if(WIN32)
    target_link_libraries(AudioCapture 
        dsound      # DirectSound
        ole32       # OLE
        winmm       # Windows Multimedia
        kernel32    # Windows Kernel API
        user32      # User API
        mfplat      # Media Foundation
        mfuuid      # Media Foundation UUIDs
    )
    
    # Для поддержки расширенных возможностей Windows
    if(MSVC)
        target_compile_options(AudioCapture PRIVATE /DWIN32_LEAN_AND_MEAN)
    endif()
endif()