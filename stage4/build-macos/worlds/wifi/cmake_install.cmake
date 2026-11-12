# Install script for directory: /Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/wifi

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
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/stage/worlds/wifi" TYPE FILE FILES
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/wifi/commando.world"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/wifi/hosp_wifi.world"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/wifi/hosp_wifi_5.world"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/wifi/wifi.world"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/wifi/wifi_itu.world"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/wifi/wifi_logdistance.world"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/wifi/wifi_ray.world"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/wifi/wifi_simple.world"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/wifi/map.inc"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/Users/yang/repos/SSCI2027/code/stage4/build-macos/worlds/wifi/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
