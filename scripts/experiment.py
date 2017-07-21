import subprocess
import time
import argparse
import os, errno

tests = ["ECMP_RANDOM_FLOWLET", "ECMP_PER_FLOW", "ECMP_RANDOM"]
errors = [0, 0.001, 0.01, 0.05, 0.1]


parser = argparse.ArgumentParser()
group = parser.add_mutually_exclusive_group()

parser.add_argument('-r','--RunStep',
                    help = "Seed number for the random generators",
                    default = 1)

parser.add_argument('-o','--OutputFolder',
                    help = "Outputfolder name for the whole experiment",
                    default = "test")

parser.add_argument('-b', '--Bandwidth', help='',default=10)
parser.add_argument('-t', '--SimulationTime', help='',default=10)
parser.add_argument('-i', '--InterArrival', help='',default=1600)
parser.add_argument('-d', '--Distribution', help='', default="distributions/enterprise_conga_scaled_100.csv")

args = parser.parse_args()

#make dir
output_name_ns3 = args.OutputFolder + "-" + str(args.Bandwidth) + "-" + str(args.InterArrival) + "-" + str(args.RunStep)
folder_name = "outputs/" + output_name_ns3
try:
    os.makedirs(folder_name)
except:
    pass

#Add experiment Metadata
f = open(folder_name + "/" + "metadata", "w")
for key,data in vars(args).iteritems():
    f.write(key + " : " + str(data) + "\n")
f.close()

cmd = 'time ./waf --run "fat-tree --OutputFolder={2} --LinkBandwidth={4}Mbps  --Delay=50 --QueueSize=100 --Protocol=TCP --K=4 --Monitor=false --Debug=true --Animation=false --SimulationTime={5} --SizeDistribution={7}  --IntraPodProb=0 --InterPodProb=1 --InterArrivalFlowTime={6} --ErrorRate={0} --ErrorLink=r_0_a0->r_c0 --EcmpMode={1} --SimulationName={1}_{0} --RunStep={3}" &'



first = True
for test in tests:
    
    for error in errors:
        formated_cmd = cmd.format(error, test, output_name_ns3, args.RunStep, args.Bandwidth,
                                   args.SimulationTime, args.InterArrival, args.Distribution)

        subprocess.call(formated_cmd,shell=True)
        if first:
            first = False
            time.sleep(20)
        else:
            time.sleep(2)

