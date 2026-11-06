cmake_minimum_required(VERSION 3.21)

project(EmoPlay
    VERSION 0.1.0
    DESCRIPTION "Cross-platform media player built on libmpv and Qt6/QML"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Test)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBMPV REQUIRED IMPORTED_TARGET mpv)

# Core library: libmpv engine wrapper, QML video item, config parsing,
# playback history, and the P4 mpv compatibility layer.
# Kept as a separate target so unit tests can link it without the app entry point.
add_library(emo_core STATIC
    src/mpv/MpvEngine.cpp
    src/mpv/MpvItem.cpp
    src/config/MpvConfigParser.cpp
    src/history/HistoryStore.cpp
    src/compat/MpvProfileScanner.cpp
    src/compat/MpvImportRules.cpp
    src/compat/MpvKeyName.cpp
    src/compat/CompatController.cpp
    src/settings/AppSettings.cpp
)
target_include_directories(emo_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(emo_core
    PUBLIC Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickControls2 PkgConfig::LIBMPV)
target_compile_definitions(emo_core PRIVATE QT_NO_CAST_FROM_ASCII)

qt_add_executable(emoplay src/main.cpp)
# The alias of each file is its path relative to the source tree, so
# PREFIX "/" puts qml/Main.qml at qrc:/qml/Main.qml (see main.cpp).
qt_add_resources(emoplay "qml" PREFIX "/" FILES qml/Main.qml)
target_link_libraries(emoplay PRIVATE emo_core Qt6::QuickControls2)
target_compile_definitions(emoplay PRIVATE
    QT_NO_CAST_FROM_ASCII
    EMOPLAY_VERSION="${PROJECT_VERSION}")

# Packaging prerequisites for the release pipeline. Qt 6.4's
# qt_add_executable() does not set these automatically (that default only
# arrived in Qt 6.7), so they must be explicit:
#  - Windows: suppress the console window that would otherwise open.
#  - macOS: produce the .app bundle that macdeployqt processes.
if(WIN32)
    set_target_properties(emoplay PROPERTIES WIN32_EXECUTABLE TRUE)
elseif(APPLE)
    set_target_properties(emoplay PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "EmoPlay"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.emoplay.EmoPlay"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION "${PROJECT_VERSION}")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(emo_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(emoplay PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_subdirectory(tests)
