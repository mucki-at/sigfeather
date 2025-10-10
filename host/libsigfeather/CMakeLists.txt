find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB libusb REQUIRED)

add_library(libsigfeather devicemanager.cpp libsigfeather.cpp)
target_include_directories(libsigfeather PRIVATE ${LIBUSB_STATIC_INCLUDE_DIRS})
target_include_directories(libsigfeather INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(libsigfeather INTERFACE ${LIBUSB_STATIC_LIBRARIES})
target_link_directories(libsigfeather INTERFACE ${LIBUSB_STATIC_LIBRARY_DIRS})
