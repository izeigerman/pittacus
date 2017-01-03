#
# Copyright 2016-2017 Iaroslav Zeigerman
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
cmake_minimum_required(VERSION 3.6)
project(pittacus)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")

include_directories(src)
file(GLOB SOURCE_FILES "src/*.c")

add_library(pittacus_obj OBJECT ${SOURCE_FILES})
add_library(pittacus SHARED $<TARGET_OBJECTS:pittacus_obj>)
add_library(pittacus_static STATIC $<TARGET_OBJECTS:pittacus_obj>)

set(INSTALL_INCLUDE_FILES ./src/gossip.h ./src/network.h ./src/config.h ./src/errors.h)
install(FILES ${INSTALL_INCLUDE_FILES} DESTINATION include/pittacus)
install (TARGETS pittacus pittacus_static
         LIBRARY DESTINATION lib
         ARCHIVE DESTINATION lib)

# Build demos.
add_executable(demo_seed_node demos/demo_seed_node.c $<TARGET_OBJECTS:pittacus_obj>)
add_executable(demo_node demos/demo_node.c $<TARGET_OBJECTS:pittacus_obj>)
