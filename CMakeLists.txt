cmake_minimum_required(VERSION 3.10)
project(WinCapture)


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

SET(OUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bin)
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUT_DIR})
SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUT_DIR})
SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUT_DIR})

# Добавляем исполняемый файл
file(GLOB SOURCES "src/*.cpp")

add_executable(WinCapture ${SOURCES})

# Включаем папку с заголовками
target_include_directories(WinCapture PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/miniaudio
)

target_include_directories(${PROJECT_NAME} PRIVATE include)

# Windows библиотеки для miniaudio
if(WIN32)
    target_link_libraries(WinCapture 
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
        target_compile_options(WinCapture PRIVATE /DWIN32_LEAN_AND_MEAN)

    # Используем статическую линковку Runtime библиотек
    # set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    
    # Или явно устанавливаем флаги
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT /O3 /Ob2 /DNDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT /O3 /Ob2 /DNDEBUG")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
    endif()
endif()