cmake_minimum_required(VERSION 3.21)
project(ColorConverter VERSION 1.1.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)
include(CTest)

if(MSVC)
    add_compile_options(/utf-8 /W4)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Mathematics and controller are shared by the application and its tests.
add_library(color_core STATIC
    src/core/ColorModels.h
    src/core/ColorConverter.h
    src/core/ColorConverter.cpp
    src/controller/ColorController.h
    src/controller/ColorController.cpp
)
target_include_directories(color_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_library(color_view STATIC
    src/view/MainWindow.h
    src/view/MainWindow.cpp
)
target_link_libraries(color_view PUBLIC color_core Qt6::Widgets)

add_executable(ColorConverter WIN32 src/main.cpp)
target_link_libraries(ColorConverter PRIVATE color_view)
target_compile_definitions(ColorConverter PRIVATE COLORCONVERTER_VERSION="${PROJECT_VERSION}")
set_target_properties(ColorConverter PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/ColorConverter"
)

if(BUILD_TESTING)
    find_package(Qt6 6.2 REQUIRED COMPONENTS Test)
    add_executable(ColorConverterTests tests/ColorConverterTests.cpp)
    target_link_libraries(ColorConverterTests PRIVATE color_view Qt6::Test)
    set_target_properties(ColorConverterTests PROPERTIES
        AUTOMOC ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/Tests"
    )
    add_test(NAME ColorConverterTests COMMAND ColorConverterTests)
    set_tests_properties(ColorConverterTests PROPERTIES TIMEOUT 60)
    if(UNIX AND NOT APPLE)
        set_tests_properties(ColorConverterTests PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        )
    endif()
endif()

# A normal build only compiles the targets. Qt Creator supplies the selected
# kit's runtime environment when launching the application or its tests.
install(TARGETS ColorConverter RUNTIME DESTINATION .)

# Deploy inside CPack's staging directory, only when preparing a release.
# In particular, do not force --debug for a Debug EXE linked to release Qt DLLs.
if(WIN32)
    file(GENERATE
        OUTPUT "${CMAKE_BINARY_DIR}/DeployWindows-$<CONFIG>.cmake"
        CONTENT [=[
if(NOT CMAKE_INSTALL_CONFIG_NAME STREQUAL "Release")
    message(FATAL_ERROR "Select Release in Qt Creator before creating a distribution.")
endif()

# qmake is used only to query paths, never to build the project.
set(_qmake "$<TARGET_FILE:Qt6::qmake>")
foreach(_property IN ITEMS QT_INSTALL_BINS QT_INSTALL_PLUGINS)
    execute_process(
        COMMAND "${_qmake}" -query "${_property}"
        RESULT_VARIABLE _query_result
        OUTPUT_VARIABLE _query_value
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT _query_result EQUAL 0 OR _query_value STREQUAL "")
        message(FATAL_ERROR "Cannot query ${_property} from the selected Qt kit: ${_qmake}")
    endif()
    set(_${_property} "${_query_value}")
endforeach()

set(_windeployqt "${_QT_INSTALL_BINS}/windeployqt.exe")
set(_platform_plugin "${_QT_INSTALL_PLUGINS}/platforms/qwindows.dll")
if(NOT EXISTS "${_windeployqt}")
    message(FATAL_ERROR "windeployqt is missing: ${_windeployqt}. Repair this Qt kit.")
endif()
if(NOT EXISTS "${_platform_plugin}")
    message(FATAL_ERROR
        "The Release Windows platform plugin is missing: ${_platform_plugin}. "
        "Repair the selected Qt MinGW kit using Qt Maintenance Tool."
    )
endif()

# Modern Qt supports an explicit qtpaths executable. Use the same kit rather
# than an unrelated Qt installation that might occur earlier in PATH.
execute_process(COMMAND "${_windeployqt}" --help
    RESULT_VARIABLE _help_result OUTPUT_VARIABLE _deploy_help
)
if(NOT _help_result EQUAL 0)
    message(FATAL_ERROR "Cannot start ${_windeployqt}. Check the selected Qt kit.")
endif()
set(_path_arguments)
if(_deploy_help MATCHES "--qtpaths")
    set(_qtpaths "${_QT_INSTALL_BINS}/qtpaths.exe")
    if(NOT EXISTS "${_qtpaths}")
        message(FATAL_ERROR "qtpaths is missing: ${_qtpaths}. Repair this Qt kit.")
    endif()
    list(APPEND _path_arguments --qtpaths "${_qtpaths}")
endif()

set(_deploy_dir "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}")
message(STATUS "Deploying Release dependencies from ${_QT_INSTALL_BINS}")
message(STATUS "Windows platform plugin: ${_platform_plugin}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        --unset=QT_PLUGIN_PATH --unset=QT_QPA_PLATFORM_PLUGIN_PATH
        "${_windeployqt}" ${_path_arguments}
        --release --compiler-runtime --no-translations
        --dir "${_deploy_dir}"
        "${_deploy_dir}/$<TARGET_FILE_NAME:ColorConverter>"
    RESULT_VARIABLE _deploy_result
)
if(NOT _deploy_result EQUAL 0)
    message(FATAL_ERROR "windeployqt failed. The Release ZIP was not created.")
endif()
if(NOT EXISTS "${_deploy_dir}/platforms/qwindows.dll")
    message(FATAL_ERROR "The deployed application has no platforms/qwindows.dll. Packaging stopped.")
endif()
]=]
    )
    install(SCRIPT "${CMAKE_BINARY_DIR}/DeployWindows-$<CONFIG>.cmake")
endif()

install(FILES README.md DESTINATION .)

set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_NAME "ColorConverter")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "ColorConverter-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/release")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
set(CPACK_VERBATIM_VARIABLES ON)
include(CPack)

# Generated only in the build directory; the source archive needs no scripts.
file(GENERATE
    OUTPUT "${CMAKE_BINARY_DIR}/CreateRelease-$<CONFIG>.cmake"
    CONTENT "
if(NOT \"$<CONFIG>\" STREQUAL \"Release\")
    message(FATAL_ERROR \"Select the Release build configuration in Qt Creator.\")
endif()
execute_process(
    COMMAND \"${CMAKE_CPACK_COMMAND}\" --config \"${CMAKE_BINARY_DIR}/CPackConfig.cmake\" -C Release
    RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR \"Release packaging failed. See the build output above.\")
endif()
"
)
add_custom_target(CreateRelease
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_BINARY_DIR}/CreateRelease-$<CONFIG>.cmake"
    DEPENDS ColorConverter
    COMMENT "Creating the release ZIP"
    USES_TERMINAL
    VERBATIM
)
