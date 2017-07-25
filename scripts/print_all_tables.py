import subprocess
import glob
import sys
import os

current_path = os.getcwd() + "/"

directories = glob.glob(current_path+ "*")

#clean
directories = [x for x in directories if "/test" not in x]

for f in directories:
    seed = f.split("-")[-1]
    with open(f+"/metadata", "r") as meta:
        print meta.read()
    subprocess.call("python ../scripts/print_statistics.py {0} {1}".format(f,seed) ,shell=True)
