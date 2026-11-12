# Install script for directory: /Users/yang/repos/SSCI2027/code/stage4/Stage/worlds

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
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/stage/worlds" TYPE FILE FILES
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/amcl-sonar.cfg"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/autolab.cfg"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/camera.cfg"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/epuck.cfg"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/everything.cfg"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/lsp_test.cfg"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/mbicp.cfg"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/nd.cfg"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/roomba.cfg"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/simple.cfg"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/test.cfg"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/uoa_robotics_lab.cfg"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/vfh.cfg"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/wavefront-remote.cfg"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/wavefront.cfg"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/wifi.cfg"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/SFU.world"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/autolab.world"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/bigbob.world"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/camera.world"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/circuit.world"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/empty.world"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/epuck.world"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/everything.world"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/fasr.world"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/fasr2.world"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/fasr_plan.world"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/large.world"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/lsp_test.world"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/mbicp.world"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/pioneer_flocking.world"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/pioneer_follow.world"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/pioneer_walle.world"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/roomba.world"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/sensor_noise_demo.world"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/sensor_noise_module_demo.world"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/simple.world"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/uoa_robotics_lab.world"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/wifi.world"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/beacons.inc"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/chatterbox.inc"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/epuck.inc"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/formation.inc"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/hokuyo.inc"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/irobot.inc"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/map.inc"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/objects.inc"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/pantilt.inc"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/pioneer.inc"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/sick.inc"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/ubot.inc"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/uoa_robotics_lab_models.inc"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/walle.inc"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/cfggen.sh"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/test.sh"
    "/Users/yang/repos/SSCI2027/code/stage4/Stage/worlds/worldgen.sh"
    )
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/Users/yang/repos/SSCI2027/code/stage4/build-macos/worlds/benchmark/cmake_install.cmake")
  include("/Users/yang/repos/SSCI2027/code/stage4/build-macos/worlds/bitmaps/cmake_install.cmake")
  include("/Users/yang/repos/SSCI2027/code/stage4/build-macos/worlds/wifi/cmake_install.cmake")

endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/Users/yang/repos/SSCI2027/code/stage4/build-macos/worlds/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
