include(FetchContent)

# CLAP SDK
message(STATUS "Fetching CLAP SDK...")
FetchContent_Declare(
    clap
    GIT_REPOSITORY https://github.com/free-audio/clap.git
    GIT_TAG        1.2.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(clap)

# Catch2 for testing
message(STATUS "Fetching Catch2...")
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

if(TRANSDUCER_BUILD_JUCE)
    message(STATUS "Fetching JUCE...")
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.6
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(JUCE)

    message(STATUS "Fetching JIVE...")
    FetchContent_Declare(
        JIVE
        GIT_REPOSITORY https://github.com/ImJimmi/JIVE.git
        GIT_TAG        main
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(JIVE)

    if(TRANSDUCER_BUILD_JUCE_CLAP)
        message(STATUS "Fetching clap-juce-extensions...")
        FetchContent_Declare(
            clap-juce-extensions
            GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
            GIT_TAG        main
            GIT_SHALLOW    TRUE
        )
        FetchContent_MakeAvailable(clap-juce-extensions)
    endif()
endif()
