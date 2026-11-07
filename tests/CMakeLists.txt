add_executable(test_config_parser test_config_parser.cpp)
target_link_libraries(test_config_parser PRIVATE emo_core Qt6::Test)
add_test(NAME test_config_parser COMMAND test_config_parser)

# Pure-logic compat tests: no mpv, no GUI, no real HOME access.
add_executable(test_compat_scanner test_compat_scanner.cpp)
target_link_libraries(test_compat_scanner PRIVATE emo_core Qt6::Test)
add_test(NAME test_compat_scanner COMMAND test_compat_scanner)

add_executable(test_import_rules test_import_rules.cpp)
target_link_libraries(test_import_rules PRIVATE emo_core Qt6::Test)
add_test(NAME test_import_rules COMMAND test_import_rules)

add_executable(test_app_settings test_app_settings.cpp)
target_link_libraries(test_app_settings PRIVATE emo_core Qt6::Test)
add_test(NAME test_app_settings COMMAND test_app_settings)

add_executable(test_mpv_key_name test_mpv_key_name.cpp)
target_link_libraries(test_mpv_key_name PRIVATE emo_core Qt6::Test)
add_test(NAME test_mpv_key_name COMMAND test_mpv_key_name)

add_executable(test_compat_controller test_compat_controller.cpp)
target_link_libraries(test_compat_controller PRIVATE emo_core Qt6::Test)
add_test(NAME test_compat_controller COMMAND test_compat_controller)

# P5 quality engine decision table: pure logic, no mpv, no GUI.
add_executable(test_quality_profile_engine test_quality_profile_engine.cpp)
target_link_libraries(test_quality_profile_engine PRIVATE emo_core Qt6::Test)
add_test(NAME test_quality_profile_engine COMMAND test_quality_profile_engine)

# Quality application path: drives a real idle libmpv core, headless.
add_executable(test_quality_controller test_quality_controller.cpp)
target_link_libraries(test_quality_controller PRIVATE emo_core Qt6::Test)
add_test(NAME test_quality_controller COMMAND test_quality_controller)
set_tests_properties(test_quality_controller PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 120)

# P5 v2 interpolation decision table: pure logic, no mpv, no GUI.
add_executable(test_interpolation_decision test_interpolation_decision.cpp)
target_link_libraries(test_interpolation_decision PRIVATE emo_core Qt6::Test)
add_test(NAME test_interpolation_decision COMMAND test_interpolation_decision)

# P5 v2 interpolation application path: drives a real idle libmpv core
# (vf change-list probe / fallback), headless.
add_executable(test_interpolation_controller test_interpolation_controller.cpp)
target_link_libraries(test_interpolation_controller PRIVATE emo_core Qt6::Test)
add_test(NAME test_interpolation_controller COMMAND test_interpolation_controller)
set_tests_properties(test_interpolation_controller PROPERTIES
    TIMEOUT 120)

# P5 v2 ICC application path: icc-profile round trip + P4 table cache
# redirect on a real idle libmpv core, headless.
add_executable(test_icc_controller test_icc_controller.cpp)
target_link_libraries(test_icc_controller PRIVATE emo_core Qt6::Test)
add_test(NAME test_icc_controller COMMAND test_icc_controller)
set_tests_properties(test_icc_controller PROPERTIES
    TIMEOUT 120)

add_executable(test_mpv_engine test_mpv_engine.cpp)
target_link_libraries(test_mpv_engine PRIVATE emo_core Qt6::Test)
# No GUI is needed: these tests exercise the libmpv handle and Qt signals only.
add_test(NAME test_mpv_engine COMMAND test_mpv_engine)
set_tests_properties(test_mpv_engine PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 120)

# P7 music mode: .lrc lyric parser is pure logic (no mpv, no GUI).
add_executable(test_lrc_parser test_lrc_parser.cpp)
target_link_libraries(test_lrc_parser PRIVATE emo_core Qt6::Test)
add_test(NAME test_lrc_parser COMMAND test_lrc_parser)

# P7 music-mode decision rules: pure logic, no mpv needed for these cases.
add_executable(test_music_controller test_music_controller.cpp)
target_link_libraries(test_music_controller PRIVATE emo_core Qt6::Test)
add_test(NAME test_music_controller COMMAND test_music_controller)
set_tests_properties(test_music_controller PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 120)

# P7 music page: headless instantiation of qml/MusicPage.qml (the CI smoke
# test boots Main.qml only — the loader stays inactive without audio).
add_executable(test_music_page test_music_page.cpp)
target_link_libraries(test_music_page PRIVATE emo_core Qt6::Test)
target_compile_definitions(test_music_page PRIVATE
    EMOPLAY_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME test_music_page COMMAND test_music_page)
set_tests_properties(test_music_page PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 60)

add_executable(test_history_store test_history_store.cpp)
target_link_libraries(test_history_store PRIVATE emo_core Qt6::Test)
# HistoryStore is pure Qt logic (plus an idle libmpv core); runs headless.
add_test(NAME test_history_store COMMAND test_history_store)
set_tests_properties(test_history_store PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 60)

# P8 plugin manifest parsing: pure logic, no git, no network.
add_executable(test_plugin_manifest test_plugin_manifest.cpp)
target_link_libraries(test_plugin_manifest PRIVATE emo_core Qt6::Test)
add_test(NAME test_plugin_manifest COMMAND test_plugin_manifest)
# The test also validates the shipped builtin catalog straight from the
# source tree (the qrc copy is compiled into the app target only).
target_compile_definitions(test_plugin_manifest PRIVATE
    EMOPLAY_SOURCE_BUILTIN_CATALOG="${CMAKE_CURRENT_SOURCE_DIR}/../assets/plugins/builtin_sources.json")

# P8 plugin manager: path assembly, scripts merge and the git state machine
# driven by a synchronous fake PluginGitRunner (no network, no real git).
add_executable(test_plugin_manager test_plugin_manager.cpp)
target_link_libraries(test_plugin_manager PRIVATE emo_core Qt6::Test)
add_test(NAME test_plugin_manager COMMAND test_plugin_manager)

# P8 v2 online manifest sources: remote JSON parsing (fixture), the
# AppData/plugin-sources cache round trip and the local+remote merge, driven
# by a synchronous fake PluginRemoteFetcher (headless, no real HTTP).
add_executable(test_remote_manifest_source test_remote_manifest_source.cpp)
target_link_libraries(test_remote_manifest_source PRIVATE emo_core Qt6::Test)
target_compile_definitions(test_remote_manifest_source PRIVATE
    EMOPLAY_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME test_remote_manifest_source COMMAND test_remote_manifest_source)

# P6 media library: pure scan logic, folder list persistence, the worker
# thread scan and the QML list model. QtCore-only; runs headless.
add_executable(test_library_store test_library_store.cpp)
target_link_libraries(test_library_store PRIVATE emo_core Qt6::Test)
add_test(NAME test_library_store COMMAND test_library_store)
set_tests_properties(test_library_store PROPERTIES
    TIMEOUT 60)

# P6 v2 thumbnails: cache-key hashing, seek/duration parsing, queue
# scheduling and the ffmpeg probe/generation chain. The chain runs against a
# stub shell script — never a real ffmpeg; the missing-binary degradation
# path uses a nonexistent binary. QtCore-only; runs headless.
add_executable(test_thumbnail_service test_thumbnail_service.cpp)
target_link_libraries(test_thumbnail_service PRIVATE emo_core Qt6::Test)
add_test(NAME test_thumbnail_service COMMAND test_thumbnail_service)
set_tests_properties(test_thumbnail_service PROPERTIES
    TIMEOUT 60)

# P9 update checker: tag parsing, version comparison and URL assembly are
# pure logic; the network path runs against a local 127.0.0.1 TCP server,
# so no request ever leaves the machine.
add_executable(test_update_checker test_update_checker.cpp)
target_link_libraries(test_update_checker PRIVATE emo_core Qt6::Test)
add_test(NAME test_update_checker COMMAND test_update_checker)
