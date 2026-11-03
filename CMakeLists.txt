cmake_minimum_required(VERSION 3.16)
set(LINKAMP_PROJECT_VERSION "$ENV{LINKAMP_BUILD_VERSION}")
if(NOT LINKAMP_PROJECT_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+")
    set(LINKAMP_PROJECT_VERSION 1.6.3)
endif()
project(Linkamp VERSION ${LINKAMP_PROJECT_VERSION} LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOMOC ON)
if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)
endif()

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Concurrent Gui Multimedia MultimediaWidgets Network Widgets)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(_linkamp_linux_integrations_default ON)
else()
    set(_linkamp_linux_integrations_default OFF)
endif()
option(LINKAMP_ENABLE_LINUX_INTEGRATIONS "Build Linux PipeWire, CD, Bluetooth and Spotify integrations" ${_linkamp_linux_integrations_default})

if(LINKAMP_ENABLE_LINUX_INTEGRATIONS)
    set(LINKAMP_HAS_LINUX_INTEGRATIONS ON)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS DBus)
    find_package(Python3 REQUIRED COMPONENTS Development.Embed)
    find_package(ALSA REQUIRED)
    find_package(PkgConfig REQUIRED)

    pkg_check_modules(PIPEWIRE REQUIRED IMPORTED_TARGET libpipewire-0.3)
    pkg_check_modules(SPA REQUIRED IMPORTED_TARGET libspa-0.2)
    pkg_check_modules(PULSEAUDIO REQUIRED IMPORTED_TARGET libpulse libpulse-simple)
    pkg_check_modules(TAGLIB REQUIRED IMPORTED_TARGET taglib)
    pkg_check_modules(ARCHIVE REQUIRED IMPORTED_TARGET libarchive)
    pkg_check_modules(CDIO_PARANOIA REQUIRED IMPORTED_TARGET libcdio_paranoia libcdio_cdda)
    pkg_check_modules(DISCID REQUIRED IMPORTED_TARGET libdiscid)
else()
    set(LINKAMP_HAS_LINUX_INTEGRATIONS OFF)
    find_package(TagLib CONFIG REQUIRED)
    find_package(LibArchive REQUIRED)
endif()

qt_standard_project_setup()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/audiosource-base)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/audiosource-coordinator)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/audiosourcepython)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/audiosourcecdnative)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/audiosourcefile)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/shared)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/view-basewindow)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/view-menu)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/view-player)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/view-playlist)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/view-equalizer)

qt_add_executable(player WIN32 MACOSX_BUNDLE
    src/audiosource-base/audiosource.cpp
    src/audiosource-base/audiosource.h
    src/audiosource-base/audiosourcewspectrumcapture.h
    src/audiosource-coordinator/audiosourcecoordinator.cpp
    src/audiosource-coordinator/audiosourcecoordinator.h
    src/audiosourcefile/audiosourcefile.cpp
    src/audiosourcefile/audiosourcefile.h
    src/audiosourcefile/mediaplayer.cpp
    src/audiosourcefile/mediaplayer.h
    src/view-player/controlbuttonswidget.cpp
    src/view-player/controlbuttonswidget.h
    src/view-player/controlbuttonswidget.ui
    src/view-player/scrolltext.cpp
    src/view-player/scrolltext.h
    src/view-player/spectrumwidget.cpp
    src/view-player/spectrumwidget.h
    src/view-player/playerview.cpp
    src/view-player/playerview.h
    src/view-player/playerview.ui
    src/view-basewindow/desktopbasewindow.cpp
    src/view-basewindow/desktopbasewindow.h
    src/view-basewindow/desktopbasewindow.ui
    src/view-basewindow/desktopplayerwindow.cpp
    src/view-basewindow/desktopplayerwindow.h
    src/view-basewindow/desktopplayerwindow.ui
    src/view-basewindow/embeddedbasewindow.cpp
    src/view-basewindow/embeddedbasewindow.h
    src/view-basewindow/embeddedbasewindow.ui
    src/view-basewindow/mainwindow.cpp
    src/view-basewindow/mainwindow.h
    src/view-basewindow/titlebar.cpp
    src/view-basewindow/titlebar.h
    src/view-basewindow/titlebar.ui
    src/view-playlist/filebrowsericonprovider.cpp
    src/view-playlist/filebrowsericonprovider.h
    src/view-playlist/classicplaylistwindow.cpp
    src/view-playlist/classicplaylistwindow.h
    src/view-playlist/playlistmodel.cpp
    src/view-playlist/playlistmodel.h
    src/view-playlist/playlistview.cpp
    src/view-playlist/playlistview.h
    src/view-playlist/playlistview.ui
    src/view-playlist/qmediaplaylist.cpp
    src/view-playlist/qmediaplaylist.h
    src/view-playlist/qmediaplaylist_p.cpp
    src/view-playlist/qmediaplaylist_p.h
    src/view-playlist/qplaylistfileparser.cpp
    src/view-playlist/qplaylistfileparser.h
    src/view-menu/mainmenuview.cpp
    src/view-menu/mainmenuview.h
    src/view-menu/mainmenuview.ui
    src/view-equalizer/equalizergraph.cpp
    src/view-equalizer/equalizergraph.h
    src/view-equalizer/equalizerpresetview.cpp
    src/view-equalizer/equalizerpresetview.h
    src/view-equalizer/equalizerview.cpp
    src/view-equalizer/equalizerview.h
    src/shared/scale.cpp
    src/shared/scale.h
    src/shared/systemaudiocontrol.cpp
    src/shared/systemaudiocontrol.h
    src/shared/systemequalizer.h
    src/shared/equalizerbands.h
    src/shared/equalizerpreset.cpp
    src/shared/equalizerpreset.h
    src/shared/fft.cpp
    src/shared/fft.h
    src/shared/util.cpp
    src/shared/util.h
    src/shared/linkampslider.h
    src/shared/linkampslider.cpp
    src/shared/winampskin.cpp
    src/shared/winampskin.h
    src/shared/updatemanager.cpp
    src/shared/updatemanager.h
    src/main.cpp
    uiassets.qrc
)

if(APPLE)
    configure_file(packaging/macos/Info.plist.in
        ${CMAKE_CURRENT_BINARY_DIR}/Linkamp-Info.plist @ONLY)
    set_target_properties(player PROPERTIES
        OUTPUT_NAME Linkamp
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/Linkamp-Info.plist
        MACOSX_BUNDLE_GUI_IDENTIFIER com.figarocool.linkamp
        MACOSX_BUNDLE_BUNDLE_NAME Linkamp
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    )
endif()

if(LINKAMP_HAS_LINUX_INTEGRATIONS)
    target_sources(player PRIVATE
        src/audiosource-base/audiosourcewspectrumcapture.cpp
        src/audiosourcecdnative/audiosourcecdnative.cpp
        src/audiosourcecdnative/audiosourcecdnative.h
        src/audiosourcecdnative/cdnativediscservice.cpp
        src/audiosourcecdnative/cdnativediscservice.h
        src/audiosourcecdnative/cdnativemetadataservice.cpp
        src/audiosourcecdnative/cdnativemetadataservice.h
        src/audiosourcecdnative/cdnativeplaybackengine.cpp
        src/audiosourcecdnative/cdnativeplaybackengine.h
        src/audiosourcecdnative/cdnativetrack.h
        src/audiosourcecdnative/cdpcmiodevice.cpp
        src/audiosourcecdnative/cdpcmiodevice.h
        src/audiosourcecdnative/cdpcmringbuffer.cpp
        src/audiosourcecdnative/cdpcmringbuffer.h
        src/audiosourcepython/audiosourcepython.cpp
        src/audiosourcepython/audiosourcepython.h
        src/shared/systemequalizer.cpp
    )
    target_link_libraries(player PRIVATE
        ALSA::ALSA
        PkgConfig::PIPEWIRE
        PkgConfig::SPA
        PkgConfig::PULSEAUDIO
        PkgConfig::CDIO_PARANOIA
        PkgConfig::DISCID
        Python3::Python
        Qt::DBus
        PkgConfig::TAGLIB
        PkgConfig::ARCHIVE
    )
    target_compile_definitions(player PRIVATE LINKAMP_HAS_LINUX_INTEGRATIONS=1)
else()
    target_sources(player PRIVATE
        src/audiosource-base/audiosourcewspectrumcapture_stub.cpp
        src/shared/systemequalizer_stub.cpp
    )
    target_link_libraries(player PRIVATE TagLib::TagLib LibArchive::LibArchive)
endif()

target_link_libraries(player PRIVATE
    Qt::Concurrent
    Qt::Core
    Qt::Gui
    Qt::Multimedia
    Qt::MultimediaWidgets
    Qt::Network
    Qt::Widgets
)

if(WIN32)
    target_link_libraries(player PRIVATE advapi32)
endif()

target_compile_definitions(player PRIVATE LINKAMP_VERSION="${PROJECT_VERSION}")

install(TARGETS player
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_generate_deploy_app_script(
    TARGET player
    FILENAME_VARIABLE deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
