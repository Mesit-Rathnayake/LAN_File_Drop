cmake_minimum_required(VERSION 3.16)

project(LANFileDrop LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(LANFileDrop
	main.cpp
	discovery/UdpBroadcast.cpp
	discovery/UdpListner.cpp
	transfer/Sender.cpp
	transfer/Receiver.cpp
)

if (WIN32)
	target_link_libraries(LANFileDrop PRIVATE ws2_32)
endif()
