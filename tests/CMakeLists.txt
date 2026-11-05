add_executable(transducer_tests
    main_test.cpp
    test_ring_buffer.cpp
)

target_link_libraries(transducer_tests
    PRIVATE
        Catch2::Catch2WithMain
        clap
)

target_include_directories(transducer_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_compile_features(transducer_tests PRIVATE cxx_std_20)

include(CTest)
add_test(NAME transducer_tests COMMAND transducer_tests)
