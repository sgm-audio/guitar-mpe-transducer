add_executable(transducer_tests
    main_test.cpp
    test_ring_buffer.cpp
    test_transient_detector.cpp
    test_yin_pitch_tracker.cpp
    test_pitch_continuity.cpp
    test_release_velocity.cpp
    test_voice_allocator.cpp
    test_brightness_estimator.cpp
    test_articulation_classifier.cpp
    test_vibrato_detector.cpp
    test_parameter_manager.cpp
    test_ump_builder.cpp
    test_event_emitter.cpp
    test_plugin_lifecycle.cpp
    ../src/plugin.cpp
    ../src/plugin_entry.cpp
)

target_link_libraries(transducer_tests
    PRIVATE
        Catch2::Catch2WithMain
        clap
)

target_include_directories(transducer_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_compile_features(transducer_tests PRIVATE cxx_std_20)

include(CTest)
add_test(NAME transducer_tests COMMAND transducer_tests)
