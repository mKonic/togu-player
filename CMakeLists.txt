cmake_minimum_required(VERSION 3.28)
project(togu-player LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Sql Network Concurrent Qml Quick QuickControls2 OpenGL Test WebEngineQuick)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)
pkg_check_modules(ARCHIVE REQUIRED IMPORTED_TARGET libarchive)
qt_standard_project_setup(REQUIRES 6.8)

execute_process(COMMAND sh ${CMAKE_SOURCE_DIR}/scripts/version.sh name
  OUTPUT_VARIABLE TOGU_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)

# Tokenizer: lindera with embedded IPADIC, built by cargo.
set(JPTOK_DIR ${CMAKE_SOURCE_DIR}/rust/jptok)
set(JPTOK_LIB ${JPTOK_DIR}/target/release/libjptok.a)
add_custom_command(OUTPUT ${JPTOK_LIB}
  COMMAND cargo build --release --manifest-path ${JPTOK_DIR}/Cargo.toml
  DEPENDS ${JPTOK_DIR}/src/lib.rs ${JPTOK_DIR}/Cargo.toml
  COMMENT "Building jptok (lindera)" USES_TERMINAL)
add_custom_target(jptok_build DEPENDS ${JPTOK_LIB})
add_library(jptok STATIC IMPORTED)
set_target_properties(jptok PROPERTIES IMPORTED_LOCATION ${JPTOK_LIB})
add_dependencies(jptok jptok_build)

file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS src/core/*.cpp src/core/*.h)
add_library(togu_core STATIC ${CORE_SOURCES})
target_include_directories(togu_core PUBLIC src)
target_compile_definitions(togu_core PUBLIC TOGU_VERSION="${TOGU_VERSION}")
target_link_libraries(togu_core PUBLIC Qt6::Core Qt6::Gui Qt6::Sql Qt6::Network Qt6::Concurrent
  PkgConfig::ARCHIVE jptok dl pthread)

file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS src/app/*.cpp src/app/*.h)
qt_add_executable(togu-player ${APP_SOURCES})
file(GLOB_RECURSE QML_FILES RELATIVE ${CMAKE_SOURCE_DIR} CONFIGURE_DEPENDS src/qml/*.qml src/qml/*.js)
set_source_files_properties(src/qml/Theme.qml src/qml/Icons.qml src/qml/KeyMap.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(togu-player
  URI Togu
  VERSION 1.0
  QML_FILES ${QML_FILES}
  RESOURCES assets/fonts/inter.woff2 assets/fonts/fredoka.woff2
  NO_RESOURCE_TARGET_PATH)
target_include_directories(togu-player PRIVATE src/app)
target_link_libraries(togu-player PRIVATE togu_core Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::OpenGL Qt6::WebEngineQuick PkgConfig::MPV)
install(TARGETS togu-player RUNTIME DESTINATION bin)

enable_testing()
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
foreach(src ${TEST_SOURCES})
  get_filename_component(name ${src} NAME_WE)
  add_executable(${name} ${src})
  target_link_libraries(${name} PRIVATE togu_core Qt6::Test)
  target_compile_definitions(${name} PRIVATE TOGU_TEST_DATA="${CMAKE_SOURCE_DIR}/tests/data")
  add_test(NAME ${name} COMMAND ${name})
endforeach()

add_executable(togu-dev tools/togu-dev.cpp)
target_link_libraries(togu-dev PRIVATE togu_core)
