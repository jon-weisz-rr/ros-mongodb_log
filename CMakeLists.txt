cmake_minimum_required(VERSION 2.8.3)
project(mongodb_log)

find_package(catkin REQUIRED COMPONENTS
  roscpp roslib)

find_package(Boost REQUIRED COMPONENTS thread filesystem)

set(COMPILE_DEBUG 0)

#if(mongodb-clients_FOUND)
#  MESSAGE(STATUS "Found mongodb-clients, will use that as the underlying interface to mongodb.")
#  add_definitions(-DUSE_MONGO_DEFAULT=1)
#else()
#  add_definitions(-DUSE_MONGO_DEFAULT=0)
#endif()

if(CMAKE_COMPILER_IS_GNUCXX)
  if(COMPILE_DEBUG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pg")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -O2")
  endif()
endif()

catkin_package(
  INCLUDE_DIRS ${catkin_INCLUDE_DIRS}
  LIBRARIES
  CATKIN_DEPENDS roscpp roslib 
)

#add_custom_command(OUTPUT ./src/mongoversion.h 
#    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/find-mongo-version.cmake)

execute_process(COMMAND python ${CMAKE_CURRENT_SOURCE_DIR}/find-mongo-version.py ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE FOO)
#file(WRITE ./src/mongoversion.h "#define USE_DEFAULT_MONGODB \"${_output}\"")

message(STATUS "Default MongoDB interface present: ${FOO}\n")

if(${FOO} MATCHES "True")
  message(STATUS "Using default MongoDB interface.\n")
  set(mongo_LIBRARIES mongoclient)
  set(bson_INCLUDE_DIRS )
  set(mongo_INCLUDE_DIRS )
else()
  message(STATUS "Using MongoCXX interface.\n")
  set(mongo_LIBRARIES mongocxx bsoncxx)
  #find_package(bsoncxx)
  #find_package(mongocxx)
  set(bson_INCLUDE_DIRS /usr/local/include/bsoncxx/v_noabi)
  set(mongo_INCLUDE_DIRS /usr/local/include/mongocxx/v_noabi)
  #set(bson_INCLUDE_DIRS ${bsoncxx_INCLUDE_DIRS})
  #set(mongo_INCLUDE_DIRS ${mongocxx_INCLUDE_DIRS})
endif()

include_directories(
  ${catkin_INCLUDE_DIRS}
  ${bson_INCLUDE_DIRS}
  ${mongo_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIRS})

add_executable(mongodb_log_tf src/mongodb_log_tf.cpp)
add_executable(mongodb_log_pcl src/mongodb_log_pcl.cpp)
add_executable(mongodb_log_img src/mongodb_log_img.cpp)
add_executable(mongodb_log_cimg src/mongodb_log_cimg.cpp)
#add_executable(mongodb_log_desig src/mongodb_log_desig.cpp)
add_executable(benchmark_produce_tf src/benchmark/produce_tf.cpp)

#add_dependencies(mongodb_log_desig designator_integration_generate_messages_cpp)

target_link_libraries(mongodb_log_tf
  ${catkin_LIBRARIES}
  ssl
  crypto
  ${mongo_LIBRARIES}
  ${Boost_LIBRARIES}
)

target_link_libraries(mongodb_log_pcl
  ${catkin_LIBRARIES}
  ssl
  crypto
  ${mongo_LIBRARIES}
  ${Boost_LIBRARIES}
)

target_link_libraries(mongodb_log_img
  ${catkin_LIBRARIES}
  ssl
  crypto
  ${mongo_LIBRARIES}
  ${Boost_LIBRARIES}
)

target_link_libraries(mongodb_log_cimg
  ${catkin_LIBRARIES}
  ssl
  crypto
  ${mongo_LIBRARIES}
  ${Boost_LIBRARIES}
)

target_link_libraries(benchmark_produce_tf
  ${catkin_LIBRARIES}
  ssl
  crypto
  ${mongo_LIBRARIES}
  ${Boost_LIBRARIES}
)
