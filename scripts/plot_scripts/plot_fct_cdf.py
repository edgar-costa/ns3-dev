import numpy as np
import matplotlib.pyplot as plt
import sys
from parser import Parser

if __name__ == "__main__":
    
    tests = ["ECMP_RANDOM_FLOWLET", "ECMP_PER_FLOW", "ECMP_RANDOM"]
    errors = [0, 0.001, 0.01, 0.05, 0.1]

    test_name = sys.argv[1]
    if test_name:
        test_name += "/"
        
    test_seed = sys.argv[2]

    root_path = "/home/edgar/ns-3-dev-git/outputs/{0}".format(test_name)
    root_name = "fat-tree-{0}_{1}_{2}.fct"

    plt.figure(1)
    color = ["r--", "g--", "b--", "y--", "k--"]
    i =1
    for test in tests:
        
        ax = plt.subplot(310+i)
        ax.set_title(test)

        color_i = 0
        for error in errors:
            fct_reader = Parser(root_path+root_name.format(test, error, test_seed))
            fct = fct_reader.get_attribute("fct")
            
            fct_sorted = sorted(fct)
    
            #log scale

            #y axis step
            y = 1. * np.arange(len(fct))
            y = [x/(len(fct)-1) for x in y]

            ##y = comulative_prob[:]
            
            plt.plot(fct_sorted, y, color[color_i])
            plt.xscale('log')
            color_i +=1

        i +=1

        
    plt.savefig("fct_"+test_name.replace("/", ""))
    plt.show()
    
    
    
