cmake_minimum_required(VERSION 3.22)
project(fps_booster_scaffold LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(fps_booster SHARED fps_booster.cpp)

find_library(log-lib log)
find_library(egl-lib EGL)
find_library(glesv2-lib GLESv2)
find_library(dl-lib dl)

target_link_libraries(fps_booster
    ${log-lib}
    ${egl-lib}
    ${glesv2-lib}
    ${dl-lib}
)
