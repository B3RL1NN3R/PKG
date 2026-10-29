cmake_minimum_required(VERSION 3.21)
project(ImageProcessor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets Concurrent)

include(CTest)

if(MSVC)
    add_compile_options(/utf-8 /W4)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# --- Ядро обработки ---
add_library(image_core STATIC
    src/model/ImageBuffer.h
    src/model/ImageBuffer.cpp
    src/processing/Filters.h
    src/processing/Filters.cpp
    src/processing/Morphology.h
    src/processing/Morphology.cpp
    src/processing/ParallelProcessor.h
    src/processing/ParallelProcessor.cpp
)
target_include_directories(image_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(image_core PUBLIC Qt6::Core Qt6::Gui Qt6::Concurrent)

# --- UI ---
add_library(image_ui STATIC
    src/view/MainWindow.h
    src/view/MainWindow.cpp
)
target_link_libraries(image_ui PUBLIC image_core Qt6::Widgets Qt6::Concurrent)

# --- Исполняемый файл ---
add_executable(ImageProcessor WIN32 src/main.cpp)
target_link_libraries(ImageProcessor PRIVATE image_ui)
target_compile_definitions(ImageProcessor PRIVATE IMAGEPROCESSOR_VERSION="${PROJECT_VERSION}")
set_target_properties(ImageProcessor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/ImageProcessor"
)

# --- Тесты ---
if(BUILD_TESTING)
    find_package(Qt6 6.2 REQUIRED COMPONENTS Test)
    add_executable(FiltersTests tests/FiltersTests.cpp)
    target_link_libraries(FiltersTests PRIVATE image_core Qt6::Test)
    set_target_properties(FiltersTests PROPERTIES
        AUTOMOC ON
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/Tests"
    )
    add_test(NAME FiltersTests COMMAND FiltersTests)
    set_tests_properties(FiltersTests PROPERTIES TIMEOUT 60)
endif()

# --- Установка и CPack ---
install(TARGETS ImageProcessor RUNTIME DESTINATION .)
install(FILES README.md DESTINATION .)

set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_NAME "ImageProcessor")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "ImageProcessor-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/release")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
set(CPACK_VERBATIM_VARIABLES ON)
include(CPack)

# --- Скрипт деплоя под Windows ---
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
set(_deploy_dir "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        --unset=QT_PLUGIN_PATH --unset=QT_QPA_PLATFORM_PLUGIN_PATH
        "${_windeployqt}"
        --release --compiler-runtime --no-translations
        --dir "${_deploy_dir}"
        "${_deploy_dir}/$<TARGET_FILE_NAME:ImageProcessor>"
    RESULT_VARIABLE _deploy_result
)
if(NOT _deploy_result EQUAL 0)
    message(FATAL_ERROR "windeployqt failed")
endif()
]=]
    )
    install(SCRIPT "${CMAKE_BINARY_DIR}/DeployWindows-$<CONFIG>.cmake")
endif()

# --- Цель CreateRelease ---
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
    DEPENDS ImageProcessor
    USES_TERMINAL
    VERBATIM
)