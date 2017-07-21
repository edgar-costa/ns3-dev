import matplotlib.pyplot as plt
import sys, math
from parser import Parser

def stdin_to_lines():
    lines = [x for x in sys.stdin]
    return lines

def split_lines(lines, separator=" "):
    splited_lines = []
    for line in lines:
        splited_line = line.strip().split(separator)
        for i, element in enumerate(splited_line):
            try:
                splited_lines[i].append(element)
            except IndexError:
                splited_lines +=[[element]]
    return splited_lines
            

def plot_cwnd(time, cwnd,fig_name):
    plt.plot(time, cwnd, 'ro')
#    plt.axis([0, float(max(time)), 0, float(max(cwnd))])
    plt.savefig(fig_name+'.png')
    plt.show()



if __name__ == "__main__":
    
    parser = Parser(sys.argv[1], {'time':0, 'cwnd':1}, " ")

    time = parser.get_attribute('time')
    cwnd = parser.get_attribute('cwnd')

    plot_cwnd(time,cwnd,sys.argv[2])




