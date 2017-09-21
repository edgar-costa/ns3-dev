import csv
import numpy as np
import matplotlib.pyplot as plt
import random

#Here we will merge, parse, and filter netflow data.


file_format = {"packets":0, "duration": 1, "bytes": 2}
BytesThreshold = 80;

class MergeAndParse(object):

    def __init__(self):
        pass

    def load_file(self, file_name):

        lines = []

        with open(file_name, 'rb') as f:
            for line in f:
                flow = line.split()
                #accept flow.
                if int(flow[2])/int(flow[0]) > BytesThreshold:
                    lines.append(flow)

        return lines


    def merge_files(self, fileA, fileB):

        #load files
        parsedA = self.load_file(fileA)
        parsedB = self.load_file(fileB)

        # shuffle data sets
        random.shuffle(parsedA)
        random.shuffle(parsedB)

        #get minimum size so we only take MIN ELEMENTS from both sets
        min_size = min(len(parsedA), len(parsedB))

        data_set = parsedA[:min_size] + parsedB[:min_size]

        #shuffle again
        random.shuffle(data_set)

        return data_set


    def run(self, fileA, fileB, fileOut):

        data_set = self.merge_files(fileA, fileB)

        with open(fileOut, "w") as f:

            for data_point in data_set:
                f.write(" ".join(data_point) + "\n")



a = MergeAndParse()
a.run("../../swift_datasets/netflowA.flows", "../../swift_datasets/netflowB.flows", "../../swift_datasets/netflow.flows")