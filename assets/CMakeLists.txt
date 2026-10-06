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
    src/core/FirestoreClient.cpp
    src/core/PresenceManager.cpp
    src/core/BroadcastManager.cpp
    src/core/RepoUpdater.cpp
    src/core/ThemeManager.cpp
    src/ui/MainWindow.cpp
    src/ui/LoginWidget.cpp
    src/ui/ProfileWidget.cpp
    src/ui/GameCardWidget.cpp
    src/ui/InstallProgressDialog.cpp
    src/ui/SettingsDialog.cpp
    src/ui/ManageDialog.cpp
    src/ui/BroadcastWidget.cpp
    src/ui/ToggleSwitch.cpp
    # Header con Q_OBJECT elencati esplicitamente: così AUTOMOC li elabora sempre (anche dopo un aggiornamento "a pezzi")
    src/core/AuthManager.h
    src/core/FirestoreClient.h
    src/core/PresenceManager.h
    src/core/BroadcastManager.h
    src/core/RepoUpdater.h
    src/ui/ProfileWidget.h
    src/ui/BroadcastWidget.h
    resources.qrc          # logo + font incorporati nell'exe
)

# ── Commit con cui viene compilato il launcher ────────────────────────────
# RepoUpdater lo confronta con la repo su GitHub per proporre l'aggiornamento del launcher.
# (se git non c'è, il controllo del launcher viene semplicemente saltato)
set(ETA_BUILD_COMMIT "")
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE ETA_BUILD_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    # rilancia CMake quando cambia il commit, così il valore resta aggiornato
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/.git/HEAD")
endif()
target_compile_definitions(ETALauncher PRIVATE ETA_BUILD_COMMIT="${ETA_BUILD_COMMIT}")

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
