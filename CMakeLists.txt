cmake_minimum_required(VERSION 3.20)
project(llfc_ndt LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_definitions(BOOST_ALL_NO_LIB BOOST_JSON_NO_LIB _WIN32_WINNT=0x0A00)

# This machine has an upstream gRPC debug build. Override these cache paths on
# other machines or use the documented vcpkg dependency workflow.
set(NDT_GRPC_SOURCE "D:/boost/grpc" CACHE PATH "gRPC source root")
set(NDT_GRPC_BUILD "D:/boost/grpc/visualpro" CACHE PATH "gRPC VS2022 build root")
set(NDT_BOOST_ROOT "D:/boost/boost_1_90_0/boost_1_90_0" CACHE PATH "Boost include root")
set(NDT_MYSQL_ROOT "D:/boost/mysql/mysql-connector-c++-8.3.0-winx64-debug/mysql-connector-c++-8.3.0-winx64" CACHE PATH "MySQL Connector/C++ root")

set(PROTO ${CMAKE_CURRENT_SOURCE_DIR}/proto/ndt.proto)
set(GEN ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GEN})
set(PROTOC ${NDT_GRPC_BUILD}/third_party/protobuf/Debug/protoc.exe CACHE FILEPATH "protoc executable")
set(GRPC_PLUGIN ${NDT_GRPC_BUILD}/Debug/grpc_cpp_plugin.exe CACHE FILEPATH "grpc_cpp_plugin executable")
add_custom_command(OUTPUT ${GEN}/ndt.pb.cc ${GEN}/ndt.pb.h ${GEN}/ndt.grpc.pb.cc ${GEN}/ndt.grpc.pb.h
  COMMAND ${PROTOC} --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/proto --cpp_out=${GEN}
          --grpc_out=${GEN} --plugin=protoc-gen-grpc=${GRPC_PLUGIN} ${PROTO}
  DEPENDS ${PROTO} VERBATIM)

file(GLOB_RECURSE ALL_GRPC_LIBS "${NDT_GRPC_BUILD}/*.lib")
set(GRPC_DEBUG_LIBS)
foreach(lib IN LISTS ALL_GRPC_LIBS)
  if(lib MATCHES "/Debug/" AND NOT lib MATCHES "grpc_csharp|grpc_plugin_support|test|benchmark")
    list(APPEND GRPC_DEBUG_LIBS ${lib})
  endif()
endforeach()
list(LENGTH GRPC_DEBUG_LIBS GRPC_LIB_COUNT)
message(STATUS "Using ${GRPC_LIB_COUNT} local gRPC debug libraries")
add_library(ndt_common STATIC server/common/NdtCommon.cpp server/common/AsioIOServicePool.cpp
    ${GEN}/ndt.pb.cc ${GEN}/ndt.grpc.pb.cc)
target_include_directories(ndt_common PUBLIC server/common ${GEN} ${NDT_BOOST_ROOT}
    ${NDT_GRPC_SOURCE}/include ${NDT_GRPC_SOURCE}/third_party/protobuf/src
    ${NDT_GRPC_SOURCE}/third_party/abseil-cpp ${NDT_GRPC_SOURCE}/third_party/re2
    ${NDT_GRPC_SOURCE}/third_party/address_sorting/include)
target_link_libraries(ndt_common PUBLIC ${GRPC_DEBUG_LIBS} ws2_32 bcrypt crypt32)

function(ndt_server name source)
  add_executable(${name} ${source})
  target_link_libraries(${name} PRIVATE ndt_common)
  add_custom_command(TARGET ${name} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${NDT_GRPC_BUILD}/third_party/zlib/Debug/zlibd.dll $<TARGET_FILE_DIR:${name}>)
endfunction()
ndt_server(GateServer server/GateServer/NdtGateServer.cpp)
ndt_server(StatusServer server/StatusServer/NdtStatusServer.cpp)
ndt_server(AuthServer server/AuthServer/NdtAuthServer.cpp)
target_include_directories(AuthServer PRIVATE ${NDT_MYSQL_ROOT}/include/jdbc)
target_link_libraries(AuthServer PRIVATE ${NDT_MYSQL_ROOT}/lib64/vs14/debug/mysqlcppconn.lib)
add_custom_command(TARGET AuthServer POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${NDT_MYSQL_ROOT}/lib64/debug/mysqlcppconn-9-vs14.dll
    ${NDT_MYSQL_ROOT}/lib64/libssl-3-x64.dll
    ${NDT_MYSQL_ROOT}/lib64/libcrypto-3-x64.dll
    $<TARGET_FILE_DIR:AuthServer>)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/server/AcqServer/NdtAcqServer.cpp)
  ndt_server(AcqServer server/AcqServer/NdtAcqServer.cpp)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/server/ProcServer/NdtProcServer.cpp)
  ndt_server(ProcServer server/ProcServer/NdtProcServer.cpp)
  target_sources(ProcServer PRIVATE server/ProcServer/Fft.cpp)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/server/FileServer/NdtFileServer.cpp)
  ndt_server(FileServer server/FileServer/NdtFileServer.cpp)
endif()

enable_testing()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/fft_test.cpp)
  add_executable(fft_test tests/fft_test.cpp server/ProcServer/Fft.cpp)
  add_test(NAME fft COMMAND fft_test)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/rpc_probe.cpp)
  add_executable(rpc_probe tests/rpc_probe.cpp)
  target_link_libraries(rpc_probe PRIVATE ndt_common)
  add_custom_command(TARGET rpc_probe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${NDT_GRPC_BUILD}/third_party/zlib/Debug/zlibd.dll $<TARGET_FILE_DIR:rpc_probe>)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/protocol_test.cpp)
  add_executable(protocol_test tests/protocol_test.cpp)
  target_link_libraries(protocol_test PRIVATE ndt_common)
  add_test(NAME protocol COMMAND protocol_test)
endif()
