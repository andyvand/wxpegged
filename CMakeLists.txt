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

    add_executable(wxpegged MACOSX_BUNDLE
        wxpegged.cpp
        help_mac.mm
        ${PEGGED_ICNS})

    set_target_properties(wxpegged PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "Pegged"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.pegged.wxpegged"
        MACOSX_BUNDLE_ICON_FILE pegged.icns
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
        MACOSX_BUNDLE_COPYRIGHT "(C) 1989-1990 Mike Blaylock; wxWidgets port 2026"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in")

    # Copy the Pegged.help book into the .app's Contents/Resources.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Pegged.help")
        add_custom_command(TARGET wxpegged POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/Pegged.help"
                "$<TARGET_BUNDLE_CONTENT_DIR:wxpegged>/Resources/Pegged.help")
    endif()

    target_link_libraries(wxpegged PRIVATE
        "-framework AppKit"
        "-framework Foundation")
elseif(WIN32)
    add_executable(wxpegged WIN32 wxpegged.cpp pegged.rc)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/wxpeggedchm/wxpegged.chm")
        add_custom_command(TARGET wxpegged POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/wxpeggedchm/wxpegged.chm"
                "$<TARGET_FILE_DIR:wxpegged>/wxpegged.chm")
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/wxpeggedchm/wxpegged.chm"
                DESTINATION bin)
    endif()
elseif(UNIX)
    add_executable(wxpegged wxpegged.cpp)
    install(TARGETS wxpegged RUNTIME DESTINATION bin)
    install(FILES     ${CMAKE_CURRENT_SOURCE_DIR}/wxpegged.desktop      DESTINATION share/applications)
    install(FILES     ${CMAKE_CURRENT_SOURCE_DIR}/wxpegged.metainfo.xml DESTINATION share/metainfo)
    install(FILES     ${CMAKE_CURRENT_SOURCE_DIR}/wxpegged.6            DESTINATION share/man/man6)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/sizes/                 DESTINATION share/icons/hicolor)
else()
    add_executable(wxpegged wxpegged.cpp)
endif()

target_link_libraries(wxpegged PRIVATE ${wxWidgets_LIBRARIES})

install(TARGETS wxpegged RUNTIME DESTINATION bin BUNDLE DESTINATION .)
