find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB libusb REQUIRED)

set(protocol_headers ${CMAKE_CURRENT_LIST_DIR}/../../device/lib/protocol)

add_library(sigfeather sigfeather.cpp devicemanager.cpp device.cpp)
target_include_directories(sigfeather PRIVATE ${LIBUSB_STATIC_INCLUDE_DIRS} ${protocol_headers})
target_include_directories(sigfeather INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(sigfeather INTERFACE ${LIBUSB_STATIC_LIBRARIES})
target_link_directories(sigfeather INTERFACE ${LIBUSB_STATIC_LIBRARY_DIRS})
