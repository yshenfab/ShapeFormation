import random
from bridson import poisson_disc_samples

num_robots = [i for i in range(50, 550, 50)]
robot_name = "flocker"
robot_dist = 2.5  # >= safe dist


def old():
    for nr in num_robots:
        w = nr  # map width/height (square)
        filename = "gen_robot_pose_{}.txt".format(str(nr))
        fout = open(filename, 'w')
        pds = poisson_disc_samples(width=w - robot_dist / 2,
                                   height=w - robot_dist / 2,
                                   r=robot_dist)
        #  print(pds)
        print("{} robots, {} samples generated".format(nr, len(pds)))
        res = random.sample(pds, nr)

        if len(res) >= nr:
            for i in range(0, nr):
                x = round(res[i][0] - (w - robot_dist / 2) / 2, 2)
                y = round(res[i][1] - (w - robot_dist / 2) / 2, 2)
                line = "{}(pose [{} {} 0.0 0.0])\n".format(
                    robot_name, str(x), str(y))
                fout.write(line)

            fout.close()


def test(map_size, idx):
    safe_dist = 4.0
    pds = poisson_disc_samples(width=map_size, height=map_size, r=safe_dist)
    #  print("{} samples generated within {}x{} map".format(
    #  len(pds), map_size, map_size))
    # pds range: [0, map_size], adjust it to [-map_size/2, map_size/2]
    pds_new = [(round(x[0] - map_size / 2, 2), round(x[1] - map_size / 2, 2))
               for x in pds]

    #  num_samples = (map_size - 40) * 5
    num_samples = ((map_size / 10) - 8) * 100
    while len(pds_new) < num_samples:
        pds = poisson_disc_samples(width=map_size,
                                   height=map_size,
                                   r=safe_dist)
        pds_new = [(round(x[0] - map_size / 2,
                          2), round(x[1] - map_size / 2, 2)) for x in pds]

    fout = open(f'./data/map_{map_size}_{idx}.txt', 'w')
    fout.write(f'{len(pds_new)}\n')
    for x in pds_new:
        fout.write(f'{x[0]}\t{x[1]}\n')
    fout.close()


if __name__ == "__main__":
    # map 140, 500
    # map 150, 700
    # map 160, 800
    # map 170, 900
    # map 180, 1000
    sz = [i for i in range(150, 190, 10)]
    for x in sz:
        for i in range(100):
            test(x, i)
