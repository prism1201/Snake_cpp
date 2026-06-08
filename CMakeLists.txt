cmake_minimum_required(VERSION 3.19)

project(Snake_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Multimedia LinguistTools)

qt_standard_project_setup()

qt_add_executable(Snake_cpp
    WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/GameWidget.cpp
    src/Snake.cpp
    src/Food.cpp
    src/ScoreManager.cpp
    src/GameController.cpp
    include/GameWidget.h
    include/Snake.h
    include/Food.h
    include/ScoreManager.h
    include/GameController.h
)

target_include_directories(Snake_cpp PRIVATE include)

qt_add_translations(
    TARGETS Snake_cpp
    TS_FILES Snake_cpp_zh_CN.ts
)

target_link_libraries(Snake_cpp
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Multimedia
)

install(TARGETS Snake_cpp
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET Snake_cpp
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
