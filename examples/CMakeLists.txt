# Examples CMakeLists.txt

# Realistic MOSFET device example (merged and optimized)
add_executable(mosfet_example mosfet_example.cpp)
target_link_libraries(mosfet_example semiconductor_device)

# Basic geometric shapes and operations example
add_executable(basic_shapes_example basic_shapes_example.cpp)
target_link_libraries(basic_shapes_example semiconductor_device)

# Conformal meshing example with hierarchical mesh sizing
add_executable(conformal_mesh_example conformal_mesh_example.cpp)
target_link_libraries(conformal_mesh_example semiconductor_device)

# Simple conformal mesh demonstration with working VTK export
add_executable(simple_conformal_demo simple_conformal_demo.cpp)
target_link_libraries(simple_conformal_demo semiconductor_device)

# Fine conformal mesh example with deduplication and proper VTK export
add_executable(fine_conformal_mesh_example fine_conformal_mesh_example.cpp)
target_link_libraries(fine_conformal_mesh_example semiconductor_device)

# Ultra-fine mesh example with forced dense meshing
add_executable(ultra_fine_mesh_example ultra_fine_mesh_example.cpp)
target_link_libraries(ultra_fine_mesh_example semiconductor_device)

# Advanced mesh refinement with OpenCASCADE built-in capabilities
add_executable(advanced_mesh_refinement_example advanced_mesh_refinement_example.cpp)
target_link_libraries(advanced_mesh_refinement_example semiconductor_device)

# Simplified MOSFET example with minimal output and single VTK file
add_executable(simple_mosfet_example simple_mosfet_example.cpp)
target_link_libraries(simple_mosfet_example semiconductor_device)

# Trapezoid with NURBS shoulders geometry example
add_executable(trapezoid_nurbs_example trapezoid_nurbs_example.cpp)
target_link_libraries(trapezoid_nurbs_example semiconductor_device)

# Realistic MOSFET with NURBS gate profile example
add_executable(realistic_gate_profile_example realistic_gate_profile_example.cpp)
target_link_libraries(realistic_gate_profile_example semiconductor_device)

# Extrusion-based MOSFET example: split source-drain bar using gate footprint
add_executable(extrusion_split_mosfet_example extrusion_split_mosfet_example.cpp)
target_link_libraries(extrusion_split_mosfet_example semiconductor_device)

# FinFET extrusion example
add_executable(finfet_extrusion_example finfet_extrusion_example.cpp)
target_link_libraries(finfet_extrusion_example semiconductor_device)
