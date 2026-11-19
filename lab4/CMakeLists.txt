cmake_minimum_required(VERSION 3.21)
project(ParserLab VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets)
if(MSVC)
    add_compile_options(/utf-8 /W4)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(raster_core STATIC
    src/raster/IRasterTarget.h
    src/raster/RasterEngine.h
    src/raster/RasterEngine.cpp
    src/model/VirtualGrid.h
    src/model/VirtualGrid.cpp
)
target_include_directories(raster_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_library(raster_ui STATIC
    src/view/RasterWidget.h
    src/view/RasterWidget.cpp
    src/view/BenchmarkWidget.h
    src/view/BenchmarkWidget.cpp
    src/view/MainWindow.h
    src/view/MainWindow.cpp
)
target_link_libraries(raster_ui PUBLIC raster_core Qt6::Core Qt6::Gui Qt6::Widgets)

add_executable(ParserLab WIN32 src/main.cpp)
target_link_libraries(ParserLab PRIVATE raster_ui)
set_target_properties(ParserLab PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/ParserLab")

install(TARGETS ParserLab RUNTIME DESTINATION .)
install(FILES README.md DESTINATION .)
set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_NAME "ParserLab")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "ParserLab-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/release")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
include(CPack)

if(WIN32)
    file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/DeployWindows-$<CONFIG>.cmake" CONTENT [=[
if(NOT CMAKE_INSTALL_CONFIG_NAME STREQUAL "Release")
    message(FATAL_ERROR "Select Release before packaging.")
endif()
set(_qmake "$<TARGET_FILE:Qt6::qmake>")
execute_process(COMMAND "${_qmake}" -query QT_INSTALL_BINS OUTPUT_VARIABLE _qt_bins OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND "${_qmake}" -query QT_INSTALL_PLUGINS OUTPUT_VARIABLE _qt_plugins OUTPUT_STRIP_TRAILING_WHITESPACE)
set(_windeployqt "${_qt_bins}/windeployqt.exe")
execute_process(COMMAND "${_windeployqt}" --release --compiler-runtime --no-translations --dir "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/ParserLab.exe" RESULT_VARIABLE _result)
if(NOT _result EQUAL 0)
    message(FATAL_ERROR "windeployqt failed")
endif()
]=])
    install(SCRIPT "${CMAKE_BINARY_DIR}/DeployWindows-$<CONFIG>.cmake")
endif()


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
    DEPENDS ParserLab
    USES_TERMINAL
    VERBATIM
)
