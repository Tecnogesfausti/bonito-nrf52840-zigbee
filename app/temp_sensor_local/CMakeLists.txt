cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(temp_sensor_local)

if(APP_SLEEPY_TEST)
  target_compile_definitions(app PRIVATE
    APP_SLEEPY_TEST=1
  )
endif()

target_sources(app PRIVATE src/main.c)
target_include_directories(app PRIVATE include)
