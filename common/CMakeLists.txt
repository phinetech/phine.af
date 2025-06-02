# CMakeLists.txt for the Common Components
#
# This file configures the build for all common components shared across
# the AF microservice. It includes utilities, models, protobuf definitions,
# dependency injection framework, and communication abstraction layer.

# Add subdirectories for each common component
add_subdirectory(cpp_utils)
add_subdirectory(models)
add_subdirectory(protos)
add_subdirectory(di)
add_subdirectory(communication)
add_subdirectory(config)
add_subdirectory(component)
# add_subdirectory(logging)

# Create a common library target that depends on all common components
add_library(af_common INTERFACE)
target_link_libraries(af_common INTERFACE
    af_cpp_utils
    af_models
    af_protos
    af_di
    af_communication
    af_config
    # af_logging
)

# Set include directories
target_include_directories(af_common INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)