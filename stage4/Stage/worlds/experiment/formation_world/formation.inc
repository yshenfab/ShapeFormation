define p2dxsonar sensor
(
  # define the size of each transducer [xsize ysize zsize] in meters
  size [ 0.01 0.05 0.01 ]
  # define the range bounds [min max]
  range [0 6.0]
  # define the angular field of view in degrees
  fov 360
  # define the number of samples spread over the fov
  samples 360
)

define p2dx_sonar_front ranger
(
  alwayson 1
  p2dxsonar( pose [ 0.0 0.0 0 0 ] )
  # p2dxsonar( pose [ 0.0 0.0 0 45 ] )
  # p2dxsonar( pose [ 0.0 0.0 0 90 ] )
  # p2dxsonar( pose [ 0.0 0.0 0 135 ] )
  # p2dxsonar( pose [ 0.0 0.0 0 180 ] )
  # p2dxsonar( pose [ 0.0 0.0 0 -45 ] )
  # p2dxsonar( pose [ 0.0 0.0 0 -90 ] )
  # p2dxsonar( pose [ 0.0 0.0 0 -135 ] )
)

define pioneer_base position
(
  color "black"			# Default color.
  drive "omni"		 	# Omnidirectional steering model.
  gui_nose 1                  	# Draw a nose on the robot so we can see which way it points
  obstacle_return 1           	# Participate in Stage collision detection.
  ranger_return 0.5            	# reflects sonar beams
  blob_return 1               	# Seen by blobfinders
  fiducial_return 1           	# Seen as "1" fiducial finders

  localization "gps"         	  
  localization_origin [0 0 0 0] 	# Start odometry at (0, 0, 0).

  # four DOF kinematics limits
  # [ xmin xmax ymin ymax zmin zmax amin amax ]				
  velocity_bounds [-1.0 1.0 -1.0 1.0 0 0 -90.0 90.0 ]
  # velocity_bounds [-0.5 0.5 -0.5 0.5 0 0 -90.0 90.0 ]
  # acceleration_bounds [-0.5 0.5 0 0 0 0 -90 90.0 ]
  acceleration_bounds [0 0 0 0 0 0 0 0]
)

define pioneer2dx_base_no_sonar pioneer_base
(
  # default size is [0.4 0.4 1]
  # actual size
  # size [0.44 0.38 0.22] # sizes from MobileRobots' web site
  # size [0.07 0.07 0.05] # epuck size
  # size [0.05 0.05 0.12]
  size [1.0 1.0 0.12] # large size
  # size [0.5 0.5 0.12] # large size

  # the pioneer's center of rotation is offset from its center of area
  # origin [-0.04 0 0 0]
  origin [0 0 0 0]

  # draw a nose on the robot so we can see which way it points
  gui_nose 1

  # estimated mass in KG
  mass 0.16
  # omnidirectional steering model
  drive "omni"
)

define pioneer2dx_base_front_sonar pioneer2dx_base_no_sonar
(
  # use the sonar array defined above with a small vertical offset to
  # drop the sensors into the robot body
  # p2dx_sonar_front( pose [0 0 -0.03 0] )
  p2dx_sonar_front( pose [0 0 0 0] ) 
)

define pioneer2dx_front_sonar pioneer2dx_base_front_sonar
(  
  # simplified Body shape:
  block( 
    points 8
    point[0] [1 0]
    point[1] [0.7071 0.7071]
    point[2] [0 1]
    point[3] [-0.7071 0.7071]
    point[4] [-1 0]
    point[5] [-0.7071 -0.7071]
    point[6] [0 -1]
    point[7] [0.7071 -0.7071]
    z [0 1]
  )
)

# Formation collision avoidance uses the fiducial model. This body-only
# variant avoids the unused 360-ray ranger cost in large swarms.
define pioneer2dx_no_sonar pioneer2dx_base_no_sonar
(
  block(
    points 8
    point[0] [1 0]
    point[1] [0.7071 0.7071]
    point[2] [0 1]
    point[3] [-0.7071 0.7071]
    point[4] [-1 0]
    point[5] [-0.7071 -0.7071]
    point[6] [0 -1]
    point[7] [0.7071 -0.7071]
    z [0 1]
  )
)
