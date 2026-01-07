cmake_minimum_required(VERSION 3.16)
project(TumourTracker)

find_package(ITK REQUIRED)
include(${ITK_USE_FILE})

# Main program (if you still want it)
add_executable(TumourTracker src/main.cpp)
target_link_libraries(TumourTracker ${ITK_LIBRARIES})

# Intensity normalization tool
add_executable(normalize_intensity src/normalize_intensity.cpp)
target_link_libraries(normalize_intensity ${ITK_LIBRARIES})

# Align organ scan
add_executable(rigid_register src/rigid_register.cpp)
target_link_libraries(rigid_register ${ITK_LIBRARIES})