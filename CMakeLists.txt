cmake_minimum_required(VERSION 3.16)
project(wxpegged C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_SYSTEM_NAME MATCHES "^Retro(68|PPC)$")
    # =======================================================================
    #  Classic Mac OS (Retro68): 68k (Retro68) or PowerPC (RetroPPC)
    #
    #  Produces Pegged.bin (MacBinary), Pegged.APPL and Pegged.dsk (an HFS
    #  disk image) for the one architecture; retro68/build.sh builds both and
    #  merges them into a single fat (68k + PowerPC) application.
    # =======================================================================
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
    include(HostCompile)

    set(BIN2C ${CMAKE_BINARY_DIR}/bin2c)
    host_compile(${CMAKE_CURRENT_SOURCE_DIR}/bin2c.c ${BIN2C})

    # The HTML help (the macOS help book's pages), zipped and embedded, shown
    # in a wxHtmlWindow: classic Mac OS has no browser to hand it to.
    set(HELP_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Pegged.help/Contents/Resources/en.lproj)
    file(GLOB HELP_FILES RELATIVE ${HELP_SRC_DIR}
         ${HELP_SRC_DIR}/*.htm ${HELP_SRC_DIR}/*.html
         ${HELP_SRC_DIR}/img/*.png ${HELP_SRC_DIR}/lib/*.png)
    list(SORT HELP_FILES)
    set(HELP_DEPS)
    foreach(_f ${HELP_FILES})
        list(APPEND HELP_DEPS ${HELP_SRC_DIR}/${_f})
    endforeach()
    set(HELP_ZIP ${CMAKE_CURRENT_BINARY_DIR}/help.zip)
    add_custom_command(
        OUTPUT ${HELP_ZIP}
        COMMAND ${CMAKE_COMMAND} -E tar cf ${HELP_ZIP} --format=zip ${HELP_FILES}
        WORKING_DIRECTORY ${HELP_SRC_DIR}
        DEPENDS ${HELP_DEPS}
        COMMENT "Zipping Pegged Help (Retro68)")
    set(HELP_DATA_H ${CMAKE_CURRENT_BINARY_DIR}/help_data.h)
    add_custom_command(
        OUTPUT ${HELP_DATA_H}
        COMMAND ${BIN2C} ${HELP_ZIP} ${HELP_DATA_H} help_data
        DEPENDS ${HELP_ZIP} ${BIN2C}
        COMMENT "Embedding Pegged Help as C array (Retro68)")

    # wxRetro68 (wxUniversal on QuickDraw) from the sysroot's wx-config, never
    # the host's: FindwxWidgets is unreliable when cross-compiling.
    if(NOT wxWidgets_CONFIG_EXECUTABLE)
        set(wxWidgets_CONFIG_EXECUTABLE ${CMAKE_SYSTEM_PREFIX_PATH}/bin/wx-config)
    endif()
    set(WX_COMPONENTS html core base)
    execute_process(COMMAND ${wxWidgets_CONFIG_EXECUTABLE} --cxxflags ${WX_COMPONENTS}
        OUTPUT_VARIABLE WX_CXXFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE _wxr1)
    execute_process(COMMAND ${wxWidgets_CONFIG_EXECUTABLE} --libs ${WX_COMPONENTS}
        OUTPUT_VARIABLE WX_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE _wxr2)
    if(_wxr1 OR _wxr2)
        message(FATAL_ERROR "wx-config failed: ${wxWidgets_CONFIG_EXECUTABLE}")
    endif()
    separate_arguments(WX_CXXFLAGS_LIST NATIVE_COMMAND "${WX_CXXFLAGS}")
    separate_arguments(WX_LIBS_LIST NATIVE_COMMAND "${WX_LIBS}")

    add_executable(wxpegged
        wxpegged.cpp
        retro68/help_retro68.cpp
        ${HELP_DATA_H})
    target_compile_options(wxpegged PRIVATE ${WX_CXXFLAGS_LIST}
        -ffunction-sections -fdata-sections)
    target_include_directories(wxpegged PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
    target_link_options(wxpegged PRIVATE -Wl,--gc-sections)
    target_link_libraries(wxpegged PRIVATE
        -Wl,--start-group ${WX_LIBS_LIST} -Wl,--end-group)

    # Relink when the wx libraries change (wx-config gives flags, so CMake
    # does not know about them): the full paths, plus -l names found in the
    # -L directories.
    set(WX_LIB_FILES)
    set(_wx_lib_dirs)
    foreach(_arg ${WX_LIBS_LIST})
        if(_arg MATCHES "^-L(.+)$")
            list(APPEND _wx_lib_dirs "${CMAKE_MATCH_1}")
        elseif(_arg MATCHES "\\.a$" AND EXISTS "${_arg}")
            list(APPEND WX_LIB_FILES "${_arg}")
        elseif(_arg MATCHES "^-l(.+)$")
            foreach(_dir ${_wx_lib_dirs})
                if(EXISTS "${_dir}/lib${CMAKE_MATCH_1}.a")
                    list(APPEND WX_LIB_FILES "${_dir}/lib${CMAKE_MATCH_1}.a")
                    break()
                endif()
            endforeach()
        endif()
    endforeach()
    list(REMOVE_DUPLICATES WX_LIB_FILES)
    set_property(TARGET wxpegged APPEND PROPERTY LINK_DEPENDS ${WX_LIB_FILES})

    # -DWXPEGGED_TRACE=ON: log startup to "wxtest.log" next to the
    # application and shut the Mac down once it is up, for unattended
    # emulator runs (retro68/run-basilisk.sh).
    option(WXPEGGED_TRACE "Trace startup to wxtest.log (classic Mac OS testing)" OFF)
    if(WXPEGGED_TRACE)
        target_sources(wxpegged PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/retro68/trace_retro68.cpp)
        target_compile_definitions(wxpegged PRIVATE WXPEGGED_TRACE=1)
    endif()

    set(WXPEGGED_REZ_DEFINES)
    if(CMAKE_SYSTEM_NAME STREQUAL "RetroPPC")
        set(WXPEGGED_REZ_DEFINES WXPEGGED_PPC=1)
    endif()

    include(Retro68Application)
    retro68_application(wxpegged
        NAME "Pegged"
        CREATOR WXPG
        RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/retro68/wxpegged.r
        REZ_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/retro68
        REZ_DEFINES ${WXPEGGED_REZ_DEFINES})
    return()
endif()

# Build the Qt port instead of the wxWidgets version. Android is Qt-only, so
# building for Android (the qt-cmake toolchain sets ANDROID) forces it on.
option(USE_QT "Build the Qt port (qtpegged.cpp) instead of wxWidgets" OFF)
if(ANDROID)
    set(USE_QT ON)
endif()

if(USE_QT)
    # -----------------------------------------------------------------------
    # Qt Widgets build (desktop + Android via androiddeployqt)
    # -----------------------------------------------------------------------
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
    qt_standard_project_setup()

    qt_add_executable(wxpegged MANUAL_FINALIZATION qtpegged.cpp)
    target_link_libraries(wxpegged PRIVATE Qt6::Widgets)

    if(ANDROID)
        set_target_properties(wxpegged PROPERTIES
            QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
            QT_ANDROID_TARGET_SDK_VERSION 34
            QT_ANDROID_MIN_SDK_VERSION 28
            QT_ANDROID_VERSION_NAME "1.0"
            QT_ANDROID_VERSION_CODE 1)
    else()
        # Desktop: keep the macOS .app bundle behaviour consistent with the wx build.
        if(APPLE)
            set_target_properties(wxpegged PROPERTIES
                MACOSX_BUNDLE TRUE
                MACOSX_BUNDLE_BUNDLE_NAME "Pegged"
                MACOSX_BUNDLE_GUI_IDENTIFIER "org.pegged.pegged"
                MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
                MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0")
        endif()
        install(TARGETS wxpegged
            RUNTIME DESTINATION bin
            BUNDLE  DESTINATION .)
    endif()

    qt_finalize_executable(wxpegged)
else()
    # -----------------------------------------------------------------------
    # wxWidgets build (macOS / Windows / Linux) -- original behaviour
    # -----------------------------------------------------------------------
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
endif()
