#!/usr/bin/env python3
# -*- coding:utf-8 -*-

import os
import signal
import glob
# import multiprocessing
from multiprocessing import Pool
# import time

num_robots = [i for i in range(50, 350, 50)]

img_folder = "/Users/yang/repos/swarm_formation/stage4/Stage/formation/img/"
#  img_folder = "/home/yang/stage4/Stage/formation/img/"
imgs = glob.glob(img_folder + "*.png")


def stage(num, reps):
    for i in range(reps):  # 100 runs each
        stg_cmd = "stage {} {} ./formation{}.world".format(num, img, num)
        os.system(stg_cmd)


def worker(x, img):
    # use gtimeout for OSX
    stg_cmd = "gtimeout 5s stage {} {} ./formation{}.world".format(x, img, x)
    #  stg_cmd = "timeout 5s stage {} {} ./formation{}.world".format(x, img, x)
    os.system(stg_cmd)


def worker2(num, img):
    stg_cmd = "stage {} {} ./formation{}.world".format(num, img, num)
    os.system(stg_cmd)


# ps aux | grep auto_exp | awk {'print $2'} | xargs kill -9

if __name__ == '__main__':
    # print("Current process %s" %os.getpid())
    # pool = multiprocessing.Pool() # use all available cores
    signal.signal(signal.SIGPIPE, signal.SIG_DFL)
    pool = Pool(processes=20)  # set number of workers
    print(pool._processes)
    for x in num_robots:
        for img in imgs:
            pool.apply_async(worker, args=(
                x,
                img,
            ))
    pool.close()
    pool.join()
    print('Done!')
