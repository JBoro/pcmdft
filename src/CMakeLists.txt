project(pcmdft)
cmake_minimum_required(VERSION 3.0)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Core REQUIRED)
find_package(Qt5Gui REQUIRED)
find_package(ALSA REQUIRED)
find_package(OpenCL REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_STANDARD 11)

include_directories(${Qt5Core_INCLUDE_DIRS} ${Qt5Widgets_INCLUDE_DIRS} ${Qt5Gui_INCLUDE_DIRS} ${ALSA_INCLUDE_DIRS})

find_library(QWT_LIBRARY qwt)

set(pcmdft_SRCS pcmdft.cpp main.cpp pcmthread.cpp pcmdftwindow.ui)
add_executable(pcmdft dftthread.cpp ${pcmdft_SRCS})
target_link_libraries(pcmdft Qt5::Widgets Qt5::Core Qt5::Gui ${ALSA_LIBRARIES} ${QWT_LIBRARY} ${OpenCL_LIBRARIES})
install(TARGETS pcmdft RUNTIME DESTINATION bin)
install(FILES rdft.cl DESTINATION bin)