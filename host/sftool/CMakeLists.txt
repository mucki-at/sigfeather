find_package(Boost CONFIG REQUIRED COMPONENTS program_options)

add_executable(sftool main.cpp)
target_link_libraries(sftool sigfeather Boost::headers Boost::program_options)
