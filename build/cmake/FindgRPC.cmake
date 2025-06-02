# FindgRPC.cmake - Find gRPC package
#
# This module finds an installed gRPC package.
#
# It sets the following variables:
#  gRPC_FOUND - Set to true if gRPC is found
#  gRPC_INCLUDE_DIRS - Include directories
#  gRPC_LIBRARIES - Libraries to link against
#  gRPC_VERSION - gRPC version
#  gRPC::grpc++ - Imported target for gRPC C++ library
#  gRPC::grpc_cpp_plugin - Imported target for gRPC C++ code generator plugin

# Try to find package config
find_package(gRPC CONFIG QUIET)
if(gRPC_FOUND)
    message(STATUS "Found gRPC via config file: ${gRPC_CONFIG}")
    
    # Ensure the imported targets exist
    if(NOT TARGET gRPC::grpc++)
        message(WARNING "gRPC::grpc++ target not defined in config, creating manually")
        add_library(gRPC::grpc++ INTERFACE IMPORTED)
        set_target_properties(gRPC::grpc++ PROPERTIES
            INTERFACE_LINK_LIBRARIES "gRPC::grpc"
        )
    endif()
    
    if(NOT TARGET gRPC::grpc_cpp_plugin)
        message(WARNING "gRPC::grpc_cpp_plugin target not defined in config, creating manually")
        add_executable(gRPC::grpc_cpp_plugin IMPORTED)
        set_target_properties(gRPC::grpc_cpp_plugin PROPERTIES
            IMPORTED_LOCATION "${gRPC_CPP_PLUGIN_EXECUTABLE}"
        )
    endif()
    
    return()
endif()

# If package config not found, try to find manually
find_path(gRPC_INCLUDE_DIR grpcpp/grpcpp.h
    HINTS
        $ENV{GRPC_ROOT}/include
        /usr/local/include
        /usr/include
)

find_library(gRPC_LIBRARY
    NAMES grpc++ grpc++_unsecure
    HINTS
        $ENV{GRPC_ROOT}/lib
        /usr/local/lib
        /usr/lib
)

find_program(gRPC_CPP_PLUGIN
    NAMES grpc_cpp_plugin
    HINTS
        $ENV{GRPC_ROOT}/bin
        /usr/local/bin
        /usr/bin
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(gRPC
    DEFAULT_MSG
    gRPC_LIBRARY gRPC_INCLUDE_DIR gRPC_CPP_PLUGIN
)

if(gRPC_FOUND AND NOT TARGET gRPC::grpc++)
    add_library(gRPC::grpc++ UNKNOWN IMPORTED)
    set_target_properties(gRPC::grpc++ PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${gRPC_INCLUDE_DIR}"
        IMPORTED_LOCATION "${gRPC_LIBRARY}"
    )
endif()

if(gRPC_FOUND AND NOT TARGET gRPC::grpc_cpp_plugin)
    add_executable(gRPC::grpc_cpp_plugin IMPORTED)
    set_target_properties(gRPC::grpc_cpp_plugin PROPERTIES
        IMPORTED_LOCATION "${gRPC_CPP_PLUGIN}"
    )
endif()

mark_as_advanced(gRPC_INCLUDE_DIR gRPC_LIBRARY gRPC_CPP_PLUGIN)

set(gRPC_INCLUDE_DIRS ${gRPC_INCLUDE_DIR})
set(gRPC_LIBRARIES ${gRPC_LIBRARY})