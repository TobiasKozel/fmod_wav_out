cmake_minimum_required(VERSION 3.9.4)

project(
	fmod_wav_out
	VERSION 0.0.1
	DESCRIPTION "Out put plugin to write a wav file"
	LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 14)

include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/fmod)
link_directories(${CMAKE_SOURCE_DIR}/lib)

add_library(fmod_wav_out SHARED "fmod_wav_out.cpp")

if (WIN32)
	target_link_libraries(fmod_wav_out fmod_vc)
else()
	target_link_libraries(fmod_wav_out fmod)
endif()
