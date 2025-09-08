# Examples CMakeLists.txt

# Simple MOSFET example
add_executable(mosfet_example mosfet_example.cpp)
target_link_libraries(mosfet_example semiconductor_device)

# Basic geometric shapes example
add_executable(basic_shapes_example basic_shapes_example.cpp)
target_link_libraries(basic_shapes_example semiconductor_device)
