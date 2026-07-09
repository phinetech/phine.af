# northbound/CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

# Add the PCF Handler component
if(ENABLE_PCF_HANDLER)
	add_subdirectory(pcf_handler)
endif()

# Add future southbound handlers when implementations are available
if(ENABLE_NEF_HANDLER AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/nef_handler/CMakeLists.txt")
	add_subdirectory(nef_handler)
endif()

if(ENABLE_UDR_HANDLER AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/udr_handler/CMakeLists.txt")
	add_subdirectory(udr_handler)
endif()