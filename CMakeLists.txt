cmake_minimum_required(VERSION 2.8.12)

project(trackserver)

if( NOT CMAKE_BUILD_TYPE )
  set( CMAKE_BUILD_TYPE Release CACHE STRING
       "Type of build: None Debug Release RelWithDebInfo MinSizeRel."
       FORCE )
endif()

include_directories (
    ${PROJECT_SOURCE_DIR}
)

add_executable(server
    ${CMAKE_CURRENT_SOURCE_DIR}/server.c
)

add_executable(client
    ${CMAKE_CURRENT_SOURCE_DIR}/client.c
)

target_link_libraries(server pthread)
target_link_libraries(client pthread)
