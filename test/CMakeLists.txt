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

include_directories(../src)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -std=gnu99")
set(TEST_SHARED_SOURCE_FILES test_utils.c)
set(TEST_SOURCE_FILES messages_test.c vector_clock_test.c member_test.c)

add_library(pittacus_test_obj OBJECT ${TEST_SHARED_SOURCE_FILES})

foreach(TEST_SRC ${TEST_SOURCE_FILES})
    get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SRC}
                   $<TARGET_OBJECTS:pittacus_obj>
                   $<TARGET_OBJECTS:pittacus_test_obj>)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach(TEST_SRC)
