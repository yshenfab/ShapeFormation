# adding opencv
cmake_minimum_required(VERSION 3.15)
add_compile_options(-std=c++11)
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})
# target_link_libraries(stage ${OpenCV_LIBS})

# original cmakelists file
SET( PLUGINS
  formation
)

add_library(connected_target_generation STATIC connected_target_generation.cc)
set_target_properties(connected_target_generation PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(target_image_generation STATIC target_image_generation.cc)
target_link_libraries(target_image_generation connected_target_generation ${OpenCV_LIBS})
set_target_properties(target_image_generation PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(target_spatial_index STATIC target_spatial_index.cc)
target_link_libraries(target_spatial_index connected_target_generation)
set_target_properties(target_spatial_index PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_executable(target_qualification target_qualification.cc)
target_link_libraries(target_qualification target_image_generation)
install(TARGETS target_qualification RUNTIME DESTINATION bin)

add_library(experiment_metrics STATIC experiment_metrics.cc)
set_target_properties(experiment_metrics PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(experiment_config STATIC experiment_config.cc)
set_target_properties(experiment_config PROPERTIES POSITION_INDEPENDENT_CODE ON)

# create a library module for each plugin and link libstage to each
foreach( PLUGIN ${PLUGINS} )
  ADD_LIBRARY( ${PLUGIN} MODULE ${PLUGIN}.cc)
endforeach( PLUGIN )

set_source_files_properties( ${PLUGINS} PROPERTIES 
  COMPILE_FLAGS "${FLTK_CFLAGS}" 
)

foreach( PLUGIN ${PLUGINS} )
  TARGET_LINK_LIBRARIES( ${PLUGIN} stage target_image_generation target_spatial_index connected_target_generation experiment_metrics experiment_config ${OPENGL_LIBRARIES} ${OpenCV_LIBS})
endforeach( PLUGIN )

# delete the "lib" prefix from the plugin libraries
SET_TARGET_PROPERTIES( ${PLUGINS} PROPERTIES PREFIX "" )

# install in <prefix>/lib
install( TARGETS ${PLUGINS} DESTINATION ${PROJECT_PLUGIN_DIR})

option(BUILD_FORMATION_TESTS "Build formation controller unit tests" OFF)
if(BUILD_FORMATION_TESTS)
  add_executable(collision_avoidance_test tests/collision_avoidance_test.cc)
  add_test(NAME formation_collision_avoidance COMMAND collision_avoidance_test)

  add_executable(connected_target_generation_test tests/connected_target_generation_test.cc)
  target_link_libraries(connected_target_generation_test connected_target_generation)
  add_test(NAME formation_connected_target_generation COMMAND connected_target_generation_test)

  add_executable(target_image_generation_test tests/target_image_generation_test.cc)
  target_compile_definitions(
    target_image_generation_test
    PRIVATE FORMATION_TEST_IMAGE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/test_img/apple.png")
  target_link_libraries(target_image_generation_test target_image_generation)
  add_test(NAME formation_target_image_generation COMMAND target_image_generation_test)

  add_executable(experiment_metrics_test tests/experiment_metrics_test.cc)
  target_link_libraries(experiment_metrics_test experiment_metrics)
  add_test(NAME formation_experiment_metrics COMMAND experiment_metrics_test)

  add_executable(experiment_config_test tests/experiment_config_test.cc)
  target_link_libraries(experiment_config_test experiment_config)
  add_test(NAME formation_experiment_config COMMAND experiment_config_test)

  add_executable(target_spatial_index_test tests/target_spatial_index_test.cc)
  target_link_libraries(target_spatial_index_test target_spatial_index)
  add_test(NAME formation_target_spatial_index COMMAND target_spatial_index_test)
endif()
