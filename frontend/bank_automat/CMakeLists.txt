cmake_minimum_required(VERSION 3.16)

project(Pankkiautomaatti LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt6 (or Qt5) package
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network SerialPort)

# If using Qt5, uncomment the following line and comment the Qt6 line above
# find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Network SerialPort)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Source files
set(SOURCES
    main.cpp
    mainwindow.cpp
)

# Header files
set(HEADERS
    mainwindow.h
)

# Create the executable
add_executable(Pankkiautomaatti
    ${SOURCES}
    ${HEADERS}
)

# Link Qt libraries
target_link_libraries(Pankkiautomaatti PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::SerialPort
)

# If using Qt5, adjust the target_link_libraries to:
# target_link_libraries(RfidScanner PRIVATE
#     Qt5::Core
#     Qt5::Gui
#     Qt5::Widgets
#     Qt5::Network
#     Qt5::SerialPort
# )

# Set include directories (optional, usually handled by Qt)
target_include_directories(Pankkiautomaatti PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Install the executable (optional)
install(TARGETS Pankkiautomaatti
    RUNTIME DESTINATION bin
)
