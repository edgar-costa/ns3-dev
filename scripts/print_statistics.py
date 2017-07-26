import numpy as np
import sys
from plot_scripts.parser import Parser
from tabulate import tabulate

if __name__ == "__main__":
    
    tests = ["ECMP_RANDOM_FLOWLET", "ECMP_PER_FLOW", "ECMP_DRILL"]
    errors = [0, 0.001,0.01, 0.05, 0.1]

    test_name = sys.argv[1]
    if test_name:
        test_name += "/"
        
    test_seed = sys.argv[2]

    root_path = "{0}".format(test_name)
    root_name = "fat-tree-{0}_{1}_{2}.fct"

    i =1

    table_list = []
    # print error_header

    test_header = []

    min_list = ["min"]
    max_list = ["max"]
    mean_list = ["mean"]
    median_list = ["median"]
    p25_list = ["25th"]
    p75_list = ["75th"]
    p90_list = ["90th"]
    p95_list = ["95th"]
    p99_list = ["99th"]

    tmp_errors = errors[:]

    #ecmp, drill, etc
    for test in tests:
        #error percentage

        for error in errors:
            try:
                fct_reader = Parser(root_path+root_name.format(test, error, test_seed),)
            except:
                #print "File {0} does not exist".format(root_path+root_name.format(test, error, test_seed))
                try:
                    tmp_errors.remove(error)
                except:
                    pass
                continue
            fct = fct_reader.get_attribute("fct")

            #fct_sorted = sorted(fct)

            fct_np = np.array([round(float(x),5) for x in fct])

            #collect statistics
            minimum = np.min(fct_np)
            maximum = np.max(fct_np)
            mean = round(np.mean(fct_np),5)
            median = round(np.median(fct_np),5)

            #you can also use np.percentile(array, [25,75,..]) it returns an array with the percentiles
            percentile_25 = round(np.percentile(fct_np, 25),5)
            percentile_75 = round(np.percentile(fct_np, 75),5)
            percentile_90 = round(np.percentile(fct_np, 90),5)
            percentile_95 = round(np.percentile(fct_np, 95),5)
            percentile_99 = round(np.percentile(fct_np, 99),5)

            #append to respective lists

            min_list.append(minimum)
            max_list.append(maximum)
            mean_list.append(mean)
            median_list.append(median)
            p25_list.append(percentile_25)
            p75_list.append(percentile_75)
            p90_list.append(percentile_90)
            p95_list.append(percentile_95)
            p99_list.append(percentile_99)


        # min_list.append("")
        # max_list.append("")
        # mean_list.append("")
        # median_list.append("")
        # p25_list.append("")
        # p75_list.append("")
        # p90_list.append("")
        # p95_list.append("")
        # p99_list.append("")

        min_list.append("min")
        max_list.append("max")
        mean_list.append("mean")
        median_list.append("median")
        p25_list.append("25th")
        p75_list.append("75th")
        p90_list.append("90th")
        p95_list.append("95th")
        p99_list.append("99th")



    #table_list.append(error_header)
    table_list.append(test_header)
    table_list.append(min_list)
    table_list.append(max_list)
    table_list.append(mean_list)
    table_list.append(median_list)
    table_list.append(p25_list)
    table_list.append(p75_list)
    table_list.append(p90_list)
    table_list.append(p95_list)
    table_list.append(p99_list)

    #build final table
    for test in tests:
        test_header += ([test] + [""]*len(tmp_errors))
    error_header = ([""] + tmp_errors) * len(tests)
    print tabulate(table_list, error_header, "grid")
