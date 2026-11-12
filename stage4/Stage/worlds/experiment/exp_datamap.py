#!/usr/bin/env python3
# -*- coding:utf-8 -*-

import os
from multiprocessing import Pool


def worker(num_bots, map_idx):
    map_size = int(int(num_bots) / 5 + 40)
    stg_cmd = "timeout 20m stage {} ./data/map_{}_{}.txt ./formation_world/formation{}.world".format(
        num_bots, map_size, map_idx, num_bots)
    os.system(stg_cmd)


# ps aux | grep auto_exp | awk {'print $2'} | xargs kill -9

if __name__ == '__main__':
    #  print("Current process %s" %os.getpid())
    #  pool = multiprocessing.Pool() # use all available cores
    pool = Pool(processes=3)  # number of workers
    #  print(pool._processes)
    num_robots = list(range(200, 250, 50))
    #  num_robots = list(range(250, 350, 50))
    for x in num_robots:
        for idx in range(0, 20):  # 100
            pool.apply_async(worker, args=(
                x,
                idx,
            ))
    pool.close()
    pool.join()
    print('Done!')
