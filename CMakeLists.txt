cmake_minimum_required(VERSION 4.0)
project(USB2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SYSTEMC_INC_DIR ${SYSTEMC_HOME}/include)
# set(SYSTEMC_LIB_DIR ${SYSTEMC_HOME}/lib/libsystemc.so)
find_package(SystemCLanguage REQUIRED PATHS ${SYSTEMC_HOME})

set(INC
        common/
        device/
        controller/
        cpu/
)

set(SRC
        # USB Device
        device/device.cpp
        device/device_tb.cpp

        # USB Controller
        controller/controller.cpp

        # CPU
        cpu/cpu.cpp
)

add_executable(usb main.cpp ${SRC})
target_include_directories(usb PUBLIC ${INC} ${SYSTEMC_INC_DIR})
target_link_libraries(usb SystemC::systemc)

# SystemC Lib
add_library(USB2
	SHARED
	${SRC}
)

target_include_directories(USB2
	PUBLIC
	${INC}
)

target_link_libraries(USB2
	PUBLIC
	SystemC::systemc
)