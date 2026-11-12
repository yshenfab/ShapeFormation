import random
import os


def gen_robots(num_robots):
    fout = open(f'gen_robot_pose_{num_robots}.txt', 'w')
    #  map_size = num_robots / 5 + 40
    map_size = int((int(num_robots / 100) + 8) * 10)
    max_range = map_size / 2
    min_range = -max_range
    print(num_robots, min_range, max_range)
    print(f"map: {map_size}x{map_size}")
    robot_name = "flocker"
    for i in range(1, num_robots + 1):
        x = round(random.uniform(min_range, max_range), 2)
        y = round(random.uniform(min_range, max_range), 2)
        line = "{}(pose [{} {} 0.0 0.0])\n".format(robot_name, str(x), str(y))
        fout.write(line)

    fout.close()
    cmd = f'cat gen_robot_pose_{num_robots}.txt >> formation{num_robots}.world'
    os.system(cmd)


if __name__ == "__main__":
    #  for x in range(50, 550, 50):
    for x in range(700, 1100, 100):
        gen_robots(x)
