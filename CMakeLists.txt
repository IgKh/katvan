cmake_minimum_required(VERSION 3.16)
project(katvan LANGUAGES CXX)

set(KATVAN_VERSION "0.1.0")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(src)

install(TARGETS katvan DESTINATION bin)
install(FILES katvan.desktop DESTINATION share/applications)
install(FILES assets/katvan.svg DESTINATION share/icons/hicolor/scalable/apps)
install(FILES assets/katvan_48x48.png DESTINATION share/icons/hicolor/48x48/apps RENAME katvan.png)
