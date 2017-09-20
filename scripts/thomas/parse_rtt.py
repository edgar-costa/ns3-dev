import csv
import numpy as np
import matplotlib.pyplot as plt

file_format = {"src":0, "dst": 1, "src_port": 2, "dst_port":3, "rtt":6}

class ParseRTT(object):

    def __init__(self,file_name, file_format = file_format, delimiter = " "):

        self._delimiter = delimiter
        self._file_format = file_format
        self.load_file(file_name)

    def load_file(self, file_name):
        file = open(file_name, 'rb')
        self._raw = csv.reader(file, delimiter=self._delimiter)


    def run(self):
        rtts = []
        for x in self._raw:
            try:
                rtts.append(float(x[self._file_format["rtt"]]))
            except:
                continue

        return rtts

    def get_cdf(self, data):

        sorted_data =  sorted(data)
        y = 1. * np.arange(len(data))
        y = [x / (len(data) - 1) for x in y]

        ##y = comulative_prob[:]
        color = ["r--", "g--", "b--", "y--", "k--"]

        plt.plot(sorted_data, y, color[0])
        plt.xscale('log')

        #plt.savefig("fct_" + test_name.replace("/", ""))
        plt.show()



A = ParseRTT("rtt.txt", delimiter="\t")
A.get_cdf(A.run())