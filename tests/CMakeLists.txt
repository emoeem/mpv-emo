add_executable(test_config_parser test_config_parser.cpp)
target_link_libraries(test_config_parser PRIVATE emo_core Qt6::Test)
add_test(NAME test_config_parser COMMAND test_config_parser)

add_executable(test_mpv_engine test_mpv_engine.cpp)
target_link_libraries(test_mpv_engine PRIVATE emo_core Qt6::Test)
# No GUI is needed: these tests exercise the libmpv handle and Qt signals only.
set_tests_properties(test_mpv_engine PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 120)
