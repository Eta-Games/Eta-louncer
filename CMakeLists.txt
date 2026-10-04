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
)

target_include_directories(ETALauncher PRIVATE src)
target_link_libraries(ETALauncher PRIVATE Qt6::Widgets Qt6::Network)

# Copia il logo accanto all'eseguibile
add_custom_command(TARGET ETALauncher POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/assets/logo.png"
        "$<TARGET_FILE_DIR:ETALauncher>/logo.png"
)
