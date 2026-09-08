set(CMAKE_SYSTEM_NAME iOS)

if(NOT CMAKE_GENERATOR STREQUAL "Xcode")
    message(FATAL_ERROR "Vanilla's iOS toolchain requires the Xcode generator")
endif()

if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    execute_process(
        COMMAND xcodebuild -version
        RESULT_VARIABLE _vanilla_xcode_version_result
        OUTPUT_VARIABLE _vanilla_xcode_version_output
        ERROR_VARIABLE _vanilla_xcode_version_error
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT _vanilla_xcode_version_result EQUAL 0)
        message(FATAL_ERROR
            "Could not determine the selected Xcode version: ${_vanilla_xcode_version_error}"
        )
    endif()

    string(REGEX MATCH "Xcode ([0-9]+(\\.[0-9]+)*)" _vanilla_xcode_version_match
        "${_vanilla_xcode_version_output}"
    )
    if(NOT CMAKE_MATCH_1)
        message(FATAL_ERROR
            "Could not parse the selected Xcode version from: ${_vanilla_xcode_version_output}"
        )
    endif()

    if(CMAKE_MATCH_1 VERSION_GREATER_EQUAL 26.0)
        set(_vanilla_ios_deployment_target 15.0)
    else()
        set(_vanilla_ios_deployment_target 12.0)
    endif()
    set(CMAKE_OSX_DEPLOYMENT_TARGET "${_vanilla_ios_deployment_target}" CACHE STRING
        "Minimum iOS deployment version"
    )
endif()
