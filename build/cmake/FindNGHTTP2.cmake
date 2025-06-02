# FindNGHTTP2.cmake
# Try to find nghttp2 and its ASIO bindings
# Once done this will define:
#  NGHTTP2_FOUND        - System has nghttp2
#  NGHTTP2_INCLUDE_DIRS - The nghttp2 include directories
#  NGHTTP2_LIBRARIES    - The libraries needed to use nghttp2
#  NGHTTP2_DEFINITIONS  - Compiler switches required for using nghttp2

find_package(PkgConfig QUIET)
pkg_check_modules(PC_NGHTTP2 QUIET libnghttp2)
set(NGHTTP2_DEFINITIONS ${PC_NGHTTP2_CFLAGS_OTHER})

# Find nghttp2 core library
find_path(NGHTTP2_INCLUDE_DIR nghttp2/nghttp2.h
          HINTS ${PC_NGHTTP2_INCLUDEDIR} ${PC_NGHTTP2_INCLUDE_DIRS}
          PATH_SUFFIXES include)

find_library(NGHTTP2_LIBRARY NAMES nghttp2
             HINTS ${PC_NGHTTP2_LIBDIR} ${PC_NGHTTP2_LIBRARY_DIRS})

# Find nghttp2-asio library
find_path(NGHTTP2_ASIO_INCLUDE_DIR nghttp2/asio_http2_server.h
          HINTS ${PC_NGHTTP2_INCLUDEDIR} ${PC_NGHTTP2_INCLUDE_DIRS}
          PATH_SUFFIXES include)

find_library(NGHTTP2_ASIO_LIBRARY NAMES nghttp2_asio
             HINTS ${PC_NGHTTP2_LIBDIR} ${PC_NGHTTP2_LIBRARY_DIRS})

# Handle the QUIETLY and REQUIRED arguments and set NGHTTP2_FOUND
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NGHTTP2 DEFAULT_MSG
                                  NGHTTP2_LIBRARY NGHTTP2_INCLUDE_DIR)

if(NGHTTP2_FOUND)
  set(NGHTTP2_LIBRARIES ${NGHTTP2_LIBRARY})
  set(NGHTTP2_INCLUDE_DIRS ${NGHTTP2_INCLUDE_DIR})
  
  if(NGHTTP2_ASIO_LIBRARY AND NGHTTP2_ASIO_INCLUDE_DIR)
    set(NGHTTP2_ASIO_FOUND TRUE)
    list(APPEND NGHTTP2_LIBRARIES ${NGHTTP2_ASIO_LIBRARY})
    list(APPEND NGHTTP2_INCLUDE_DIRS ${NGHTTP2_ASIO_INCLUDE_DIR})
  else()
    set(NGHTTP2_ASIO_FOUND FALSE)
    message(STATUS "nghttp2 asio bindings not found")
  endif()
  
  # Create imported targets
  if(NOT TARGET nghttp2::nghttp2)
    add_library(nghttp2::nghttp2 UNKNOWN IMPORTED)
    set_target_properties(nghttp2::nghttp2 PROPERTIES
      IMPORTED_LOCATION "${NGHTTP2_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${NGHTTP2_INCLUDE_DIR}"
      INTERFACE_COMPILE_OPTIONS "${PC_NGHTTP2_CFLAGS_OTHER}")
  endif()
  
  if(NGHTTP2_ASIO_FOUND AND NOT TARGET nghttp2::asio_http2)
    add_library(nghttp2::asio_http2 UNKNOWN IMPORTED)
    set_target_properties(nghttp2::asio_http2 PROPERTIES
      IMPORTED_LOCATION "${NGHTTP2_ASIO_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${NGHTTP2_ASIO_INCLUDE_DIR}"
      INTERFACE_LINK_LIBRARIES "nghttp2::nghttp2")
  endif()
endif()

mark_as_advanced(NGHTTP2_INCLUDE_DIR NGHTTP2_LIBRARY
                 NGHTTP2_ASIO_INCLUDE_DIR NGHTTP2_ASIO_LIBRARY)