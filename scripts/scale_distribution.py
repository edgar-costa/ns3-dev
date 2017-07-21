import sys 

with open(sys.argv[1], "r") as f:

    lines = f.readlines()


f = open(sys.argv[1], "w")

for line in lines:
    splitted_line = line.strip().split(" ")
    splitted_line[0] = int(float(splitted_line[0])) / int(sys.argv[2])
    if splitted_line[0] < 10:
        splitted_line[0] = 10
        
    f.write(str(splitted_line[0]) + " " + str(splitted_line[1]) + "\n")

f.close()
