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

add_executable(test_mpv_engine test_mpv_engine.cpp)
target_link_libraries(test_mpv_engine PRIVATE emo_core Qt6::Test)
# No GUI is needed: these tests exercise the libmpv handle and Qt signals only.
add_test(NAME test_mpv_engine COMMAND test_mpv_engine)
set_tests_properties(test_mpv_engine PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 120)

add_executable(test_history_store test_history_store.cpp)
target_link_libraries(test_history_store PRIVATE emo_core Qt6::Test)
# HistoryStore is pure Qt logic (plus an idle libmpv core); runs headless.
add_test(NAME test_history_store COMMAND test_history_store)
set_tests_properties(test_history_store PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 60)
