# epuck.inc
# model of the epuck

# ranger sensors
define epuck_IR ranger
(
	# number of IR sensors
	scount 8

	# define the pose of each IR sensor [xpos ypos heading]
	spose[0] [0.030	-0.010	-15]	# fr right
	spose[1] [0.022	-0.025	-45]	# fr right 2
	spose[2] [0.0	-0.031	-90]	# right	
	spose[3] [-0.03   -0.015 -150]	# back right
	spose[4] [-0.03	0.015 150]	# back left
	spose[5] [0.0  	0.031 90]	# left
	spose[6] [0.022	0.025 45]	# fr left 2
	spose[7] [0.03	0.01 15]	# fr left
		
	# define the field of view of each IR sensor [range_min range_max view_angle]
	sview [0.0064 0.1 10]

	# define the size of each IR sensor [xsize ysize] in meters
	#ssize [0.007 0.002]
)

# fiducial
# define epuck_fid fiducial
# (
# 	# fiducial properties
# 	range_min 0.0
# 	range_max 0.2
# 	range_max_id 5.0
# 	fov 2*3.14159
	
# 	# model properties
# 	size [0.1 0.1 0.1]
# )

define epuck position
(
	obstacle_return 1
	ranger_return 1
	blob_return 1
	gui_outline 1
	gravity_return 1
	fiducial_return 1
	gui_nose 1
    
	# actual size
	size [0.07 0.07 0.045]
	
	# estimated mass in KG
	mass 0.3
	
	# Defining the shape of an epuck
	block
	(
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

	# differential steering model (diff/omin/car)
	drive "diff"
	
	localization "gps"
	# localization_origin [x y z theta]
	localization_origin [0 0 0 0]
	
	# sensors attached to epuck
	epuck_IR()
	#epuck_fid()
) 

