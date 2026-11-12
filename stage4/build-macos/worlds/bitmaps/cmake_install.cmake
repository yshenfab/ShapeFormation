# Install script for directory: /Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/bitmaps

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/Users/yang/repos/SSCI2027/code/stage4/stage-install")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "RELEASE")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/stage/worlds/bitmaps" TYPE FILE FILES
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/bitmaps/889_05.png"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/bitmaps/SFU_1200x615.png"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/bitmaps/SRI-AIC-kwing.png"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/bitmaps/autolab.png"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/bitmaps/cave.png"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/bitmaps/cave_compact.png"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/bitmaps/cave_filled.png"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/bitmaps/empty.png"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/bitmaps/frieburg.png"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/bitmaps/ghost.png"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/bitmaps/hospital.png"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/bitmaps/hospital_section.png"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/bitmaps/human_outline.png"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/bitmaps/mbicp.png"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/bitmaps/rink.png"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/bitmaps/sal2.png"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/bitmaps/simple_rooms.png"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/bitmaps/space_invader.png"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/bitmaps/submarine.png"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/bitmaps/submarine_small.png"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/bitmaps/table.png"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/bitmaps/uoa_robotics_lab.png"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/Users/yang/repos/SSCI2027/code/stage4/build-macos/worlds/bitmaps/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
