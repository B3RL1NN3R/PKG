cmake_minimum_required(VERSION 3.21)
project(ImageMetadataReader VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Widgets Concurrent)
include(CTest)

if(MSVC)
    add_compile_options(/utf-8 /W4)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(image_core STATIC
    src/model/ImageMetadata.h
    src/parser/ImageParser.h
    src/parser/ImageParser.cpp
)
target_include_directories(image_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(image_core PUBLIC Qt6::Core)

add_library(image_ui STATIC
    src/model/ImageTableModel.h
    src/model/ImageTableModel.cpp
    src/view/MainWindow.h
    src/view/MainWindow.cpp
)
target_link_libraries(image_ui PUBLIC image_core Qt6::Widgets Qt6::Concurrent)

add_executable(ImageMetadataReader WIN32 src/main.cpp)
target_link_libraries(ImageMetadataReader PRIVATE image_ui)
target_compile_definitions(ImageMetadataReader PRIVATE IMAGEMETADATAREADER_VERSION="${PROJECT_VERSION}")
set_target_properties(ImageMetadataReader PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/ImageMetadataReader"
)

if(BUILD_TESTING)
    find_package(Qt6 6.2 REQUIRED COMPONENTS Test)
    add_executable(ImageParserTests tests/ImageParserTests.cpp)
    target_link_libraries(ImageParserTests PRIVATE image_core Qt6::Test)
    set_target_properties(ImageParserTests PROPERTIES
        AUTOMOC ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/Tests"
    )
    add_test(NAME ImageParserTests COMMAND ImageParserTests)
    set_tests_properties(ImageParserTests PROPERTIES TIMEOUT 60)
endif()

install(TARGETS ImageMetadataReader RUNTIME DESTINATION .)

if(WIN32)
    file(GENERATE
        OUTPUT "${CMAKE_BINARY_DIR}/DeployWindows-$<CONFIG>.cmake"
        CONTENT [=[
if(NOT CMAKE_INSTALL_CONFIG_NAME STREQUAL "Release")
    message(FATAL_ERROR "Select Release in Qt Creator before creating a distribution.")
endif()

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
if(NOT EXISTS "${_windeployqt}")
    message(FATAL_ERROR "windeployqt is missing: ${_windeployqt}")
endif()

execute_process(COMMAND "${_windeployqt}" --help
    RESULT_VARIABLE _help_result OUTPUT_VARIABLE _deploy_help
)
if(NOT _help_result EQUAL 0)
    message(FATAL_ERROR "Cannot start ${_windeployqt}")
endif()

set(_path_arguments)
if(_deploy_help MATCHES "--qtpaths")
    set(_qtpaths "${_QT_INSTALL_BINS}/qtpaths.exe")
    if(NOT EXISTS "${_qtpaths}")
        message(FATAL_ERROR "qtpaths is missing: ${_qtpaths}")
    endif()
    list(APPEND _path_arguments --qtpaths "${_qtpaths}")
endif()

set(_deploy_dir "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        --unset=QT_PLUGIN_PATH --unset=QT_QPA_PLATFORM_PLUGIN_PATH
        "${_windeployqt}" ${_path_arguments}
        --release --compiler-runtime --no-translations
        --dir "${_deploy_dir}"
        "${_deploy_dir}/$<TARGET_FILE_NAME:ImageMetadataReader>"
    RESULT_VARIABLE _deploy_result
)
if(NOT _deploy_result EQUAL 0)
    message(FATAL_ERROR "windeployqt failed")
endif()
if(NOT EXISTS "${_deploy_dir}/platforms/qwindows.dll")
    message(FATAL_ERROR "The deployed application has no platforms/qwindows.dll")
endif()
]=]
    )
    install(SCRIPT "${CMAKE_BINARY_DIR}/DeployWindows-$<CONFIG>.cmake")
endif()

install(FILES README.md DESTINATION .)
install(FILES docs/Lab2_Report.docx docs/RequirementsCompliance.md DESTINATION docs OPTIONAL)

set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_NAME "ImageMetadataReader")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "ImageMetadataReader-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/release")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
set(CPACK_VERBATIM_VARIABLES ON)
include(CPack)

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
    message(FATAL_ERROR \"Release packaging failed.\")
endif()
"
)
add_custom_target(CreateRelease
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_BINARY_DIR}/CreateRelease-$<CONFIG>.cmake"
    DEPENDS ImageMetadataReader
    USES_TERMINAL
    VERBATIM
)
