cmake_minimum_required(VERSION 3.16)
project(BatchGitCommand LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ──────────────────────────────────────────────
# Qt install prefix – set CMAKE_PREFIX_PATH env var or override here.
# Visual Studio's "Open Folder" CMake integration configures with no
# extra arguments, so without this default it cannot find Qt. Appending
# (rather than only setting when undefined) also survives VS auto-injecting
# a vcpkg toolchain file, which populates CMAKE_PREFIX_PATH on its own and
# would otherwise cause this default to be skipped.
# ──────────────────────────────────────────────
set(BATCHGITCOMMAND_DEFAULT_QT_PREFIX "C:/Users/leeyo/Qt/6.9.0/msvc2022_64")
if(NOT BATCHGITCOMMAND_DEFAULT_QT_PREFIX IN_LIST CMAKE_PREFIX_PATH)
    list(APPEND CMAKE_PREFIX_PATH "${BATCHGITCOMMAND_DEFAULT_QT_PREFIX}")
    message(STATUS "Adding default Qt prefix to CMAKE_PREFIX_PATH: ${BATCHGITCOMMAND_DEFAULT_QT_PREFIX}")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(BatchGitCommand WIN32
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/LogPanel.h
    src/LogPanel.cpp
    src/GeneratePage.h
    src/GeneratePage.cpp
    src/ClonePage.h
    src/ClonePage.cpp
    src/SyncPage.h
    src/SyncPage.cpp
    src/CommitPage.h
    src/CommitPage.cpp
    src/CloneWorker.h
    src/CloneWorker.cpp
    src/SyncWorker.h
    src/SyncWorker.cpp
    src/CommitWorker.h
    src/CommitWorker.cpp
    src/GitProcess.h
    src/GitProcess.cpp
)

target_link_libraries(BatchGitCommand PRIVATE Qt6::Widgets)

if(WIN32)
    target_compile_definitions(BatchGitCommand PRIVATE UNICODE _UNICODE)
    target_compile_options(BatchGitCommand PRIVATE /utf-8)
endif()

# ──────────────────────────────────────────────
# Deploy Qt DLLs/plugins next to the exe after every build, so running
# or debugging straight from Visual Studio (out/build/...) works without
# manually adding Qt's bin directory to PATH.
# ──────────────────────────────────────────────
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt
        HINTS "${Qt6_DIR}/../../../bin" "${CMAKE_PREFIX_PATH}/bin"
    )
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET BatchGitCommand POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations "$<TARGET_FILE:BatchGitCommand>"
            COMMENT "Deploying Qt runtime dependencies next to the executable"
        )
    else()
        message(WARNING "windeployqt not found; the exe may not run standalone without Qt's bin dir on PATH")
    endif()
endif()
