cmake_minimum_required(VERSION 3.16)
project(ETALauncher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

add_executable(ETALauncher WIN32
    src/main.cpp
    src/core/Config.cpp
    src/core/GitCloner.cpp
    src/core/GameManager.cpp
    src/core/UpdateChecker.cpp
    src/core/ShortcutManager.cpp
    src/core/AuthManager.cpp
    src/core/ThemeManager.cpp
    src/ui/MainWindow.cpp
    src/ui/LoginWidget.cpp
    src/ui/ProfileWidget.cpp
    src/ui/GameCardWidget.cpp
    src/ui/InstallProgressDialog.cpp
    src/ui/SettingsDialog.cpp
    src/ui/ManageDialog.cpp
    src/ui/ToggleSwitch.cpp
    resources.qrc          # logo + font incorporati nell'exe
)

target_include_directories(ETALauncher PRIVATE src)
target_link_libraries(ETALauncher PRIVATE Qt6::Widgets Qt6::Network)

# ── FIX "Qt6Widgets.dll non è stato trovato" ─────────────────────────────
# Fuori da Qt Creator l'exe non trova le DLL di Qt. windeployqt le copia
# (con plugin e runtime MinGW) accanto all'eseguibile a ogni build.
if(WIN32)
    get_target_property(_qmake_exe Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_exe}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt6 windeployqt
        HINTS "${_qt_bin_dir}" "${Qt6_DIR}/../../../bin")

    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ETALauncher POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E env "PATH=${_qt_bin_dir};$ENV{PATH}"
                    "${WINDEPLOYQT_EXECUTABLE}"
                    --no-translations --no-system-d3d-compiler --no-opengl-sw
                    "$<TARGET_FILE:ETALauncher>"
            COMMENT "windeployqt: copio le DLL di Qt accanto a ETALauncher.exe")
    else()
        message(WARNING "windeployqt non trovato: l'exe funzionerà solo da Qt Creator")
    endif()
endif()
