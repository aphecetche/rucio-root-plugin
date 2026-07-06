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
  G__RucioROOT.cxx
  src/TRucioFile.cxx
  src/TRucioResolver.cxx
  src/TRucioUrl.cxx
)

ROOT_GENERATE_DICTIONARY(G__RucioROOT inc/TRucioFile.h LINKDEF inc/LinkDef.h)

add_library(RucioROOT SHARED ${RUCIO_ROOT_PLUGIN_SOURCES})
target_include_directories(RucioROOT
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(RucioROOT
  PUBLIC
    ROOT::Core
    ROOT::RIO
    ROOT::Net
    ROOT::NetxNG
    CURL::libcurl
)

target_link_libraries(RucioROOT PUBLIC nlohmann_json::nlohmann_json)

set_target_properties(RucioROOT PROPERTIES
  OUTPUT_NAME RucioROOT
  MACOSX_RPATH ON
)

install(TARGETS RucioROOT
  EXPORT RucioROOTTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/libRucioROOT.rootmap
  ${CMAKE_CURRENT_BINARY_DIR}/libRucioROOT_rdict.pcm
  DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY inc/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY etc/plugins DESTINATION etc)

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
