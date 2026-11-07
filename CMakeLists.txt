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

# Network: P8 v2 online plugin manifest sources (RemoteManifestSource).
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Network Quick QuickControls2 Test)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBMPV REQUIRED IMPORTED_TARGET mpv)

# Core library: libmpv engine wrapper, QML video item, config parsing,
# playback history, the P4 mpv compatibility layer, the P5 quality engine,
# the P6 media library, the P7 music mode and the P8 plugin store.
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
    src/quality/QualityProfileEngine.cpp
    src/quality/QualityController.cpp
    src/quality/InterpolationDecision.cpp
    src/quality/InterpolationController.cpp
    src/quality/IccController.cpp
    src/music/LrcParser.cpp
    src/music/MusicController.cpp
    src/library/LibraryStore.cpp
    src/library/LibraryScanWorker.cpp
    src/library/LibraryModel.cpp
    src/library/ThumbnailService.cpp
    src/plugins/PluginManifest.cpp
    src/plugins/PluginGitRunner.cpp
    src/plugins/PluginManager.cpp
    src/plugins/PluginRemoteFetcher.cpp
    src/plugins/RemoteManifestSource.cpp
    src/update/UpdateChecker.cpp
)
target_include_directories(emo_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(emo_core
    PUBLIC Qt6::Core Qt6::Gui Qt6::Network Qt6::Quick Qt6::QuickControls2
           PkgConfig::LIBMPV)
target_compile_definitions(emo_core PRIVATE QT_NO_CAST_FROM_ASCII)

qt_add_executable(emoplay src/main.cpp)
# The alias of each file is its path relative to the source tree, so
# PREFIX "/" puts qml/Main.qml at qrc:/qml/Main.qml (see main.cpp).
qt_add_resources(emoplay "qml" PREFIX "/" FILES
    qml/Main.qml
    qml/ImportWizard.qml
    qml/MusicPage.qml
    qml/LibraryDrawer.qml
    qml/PluginDrawer.qml
    qml/InterpolationPanel.qml
    qml/IccPanel.qml
    qml/PluginRemoteSection.qml
    qml/UpdateBadge.qml
)
# P8 plugin store: the built-in manager.json-compatible catalog, read by
# PluginManager from qrc:/assets/plugins/builtin_sources.json (see
# PluginManager::defaultCatalogPath(); the resource alias is the file path
# under PREFIX "/", same as the qml block above).
qt_add_resources(emoplay "plugins" PREFIX "/" FILES
    assets/plugins/builtin_sources.json
)
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

# --------------------------------------------------------------------------
# CPack (P9): minimal DEB/RPM plumbing. This is *minimal* on purpose: it
# packages the emoplay binary produced by a normal Linux build without
# claiming a complete dependency list — the Qt6/libmpv runtime dependencies
# are not fully expressed here (shlibdeps automation is off), so these
# packages may not install on an arbitrary distro out of the box. The
# blessed Linux delivery channel remains the AppImage built by release.yml;
# the DEB/RPM generators exist so local package smoke tests and future
# distro packaging have a starting point.
set(CPACK_PACKAGE_NAME "emoplay")
set(CPACK_PACKAGE_VENDOR "EmoPlay")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/emoeem/mpv-emo")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Cross-platform media player built on libmpv and Qt6/QML")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
# DEB requires a maintainer; RPM reuses it for the packager field.
set(CPACK_PACKAGE_CONTACT "EmoPlay <emoplay@users.noreply.github.com>")
# DEB: metadata only, no dependency list (see the minimal note above).
set(CPACK_DEBIAN_PACKAGE_SECTION "video")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
# RPM: MIT matches LICENSE at the repository root.
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Multimedia")
set(CPACK_GENERATOR "DEB;RPM")
include(CPack)
