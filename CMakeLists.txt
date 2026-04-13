cmake_minimum_required(VERSION 3.12)
project(wxpegged CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# wxWidgets 3.x, any backend (gtk / osx_cocoa / msw).
find_package(wxWidgets REQUIRED COMPONENTS core base)

if(NOT WIN32)
    include(${wxWidgets_USE_FILE})
endif()

if(APPLE)
    set(MACOSX_BUNDLE_ICON_FILE pegged.icns)
    set(PEGGED_ICNS "${CMAKE_CURRENT_SOURCE_DIR}/pegged.icns")
    set_source_files_properties(${PEGGED_ICNS} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
    add_executable(wxpegged MACOSX_BUNDLE wxpegged.cpp ${PEGGED_ICNS})
    set_target_properties(wxpegged PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "Pegged"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.pegged.wxpegged"
        MACOSX_BUNDLE_ICON_FILE pegged.icns)
elseif(WIN32)
    add_executable(wxpegged WIN32 wxpegged.cpp pegged.rc)
elseif(UNIX)
    add_executable(wxpegged wxpegged.cpp)
    install(TARGETS wxpegged RUNTIME DESTINATION bin)
    install(FILES     ${CMAKE_CURRENT_SOURCE_DIR}/wxpegged.desktop      DESTINATION share/applications)
    install(FILES     ${CMAKE_CURRENT_SOURCE_DIR}/wxpegged.metainfo.xml DESTINATION share/metainfo)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/sizes/                 DESTINATION share/icons/hicolor)
else()
    add_executable(wxpegged wxpegged.cpp)
endif()

target_link_libraries(wxpegged PRIVATE ${wxWidgets_LIBRARIES})

install(TARGETS wxpegged RUNTIME DESTINATION bin BUNDLE DESTINATION .)
