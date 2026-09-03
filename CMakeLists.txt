cmake_minimum_required(VERSION 3.16)

project(RucioROOTPlugin VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ROOT REQUIRED COMPONENTS Core RIO Net NetxNG)
find_package(CURL REQUIRED)
include(${ROOT_USE_FILE})

include(FetchContent)

FetchContent_Declare(nlohmann_json
   URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
   URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
   DOWNLOAD_EXTRACT_TIMESTAMP FALSE
   EXCLUDE_FROM_ALL
   FIND_PACKAGE_ARGS
)
FetchContent_MakeAvailable(nlohmann_json)

set(RUCIO_ROOT_PLUGIN_SOURCES
  src/TRucioFile.cxx
  src/TRucioResolver.cxx
  src/TRucioUrl.cxx
)

set(RUCIO_ROOT_PLUGIN_HEADERS
  inc/TRucioFile.h
  inc/TRucioResolver.h
  inc/TRucioUrl.h
)

add_library(RucioROOT)

target_sources(
  RucioROOT
  PRIVATE ${RUCIO_ROOT_PLUGIN_SOURCES}
  PUBLIC FILE_SET HEADERS FILES ${RUCIO_ROOT_PLUGIN_HEADERS}
  BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

target_link_libraries(RucioROOT
  PUBLIC
    ROOT::Core
    ROOT::RIO
    ROOT::Net
    ROOT::NetxNG
    CURL::libcurl
    nlohmann_json::nlohmann_json
)

set_target_properties(RucioROOT PROPERTIES
  OUTPUT_NAME RucioROOT
  MACOSX_RPATH ON
)

root_generate_dictionary(G__RucioROOT ${RUCIO_ROOT_PLUGIN_HEADERS}
  MODULE RucioROOT
  LINKDEF inc/LinkDef.h
)

install(TARGETS RucioROOT
  EXPORT RucioROOTTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/libRucioROOT.rootmap
  ${CMAKE_CURRENT_BINARY_DIR}/libRucioROOT_rdict.pcm
  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY etc/plugins DESTINATION etc)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/.rootrc.in"
               "${CMAKE_CURRENT_BINARY_DIR}/.rootrc" @ONLY)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/.rootrc" DESTINATION etc)

option(RUCIO_ROOT_BUILD_TESTS "Build unit tests" ON)
if(RUCIO_ROOT_BUILD_TESTS)
  enable_testing()

  add_executable(test_rucio_url tests/test_rucio_url.cxx src/TRucioUrl.cxx)
  target_include_directories(test_rucio_url PRIVATE inc)
  target_link_libraries(test_rucio_url PRIVATE ROOT::Core)
  add_test(NAME rucio_url COMMAND test_rucio_url)

  add_executable(test_rucio_json_stream tests/test_rucio_json_stream.cxx src/TRucioResolver.cxx src/TRucioUrl.cxx)
  target_include_directories(test_rucio_json_stream PRIVATE inc)
  target_link_libraries(test_rucio_json_stream PRIVATE CURL::libcurl ROOT::Core)
  target_link_libraries(test_rucio_json_stream PRIVATE nlohmann_json::nlohmann_json)
  add_test(NAME rucio_json_stream COMMAND test_rucio_json_stream)
endif()
