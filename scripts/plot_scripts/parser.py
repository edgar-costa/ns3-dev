import csv

file_format = {"id":0, "size": 1, "fct": 2, "startTime":3, "endTime":4}

class Parser(object):

    def __init__(self,file_name ="", format = file_format, delimiter = " "):
        self._file = file_name
        self._file_format = format
        self._delimiter = delimiter

        #load data
        try:
            self.raw = self.load_file(self._file)
        except:
            print "Error reading file"

    def load_file(self,file_name):
        file = open(file_name, 'rb')
        data = csv.reader(file, delimiter=self._delimiter)
        return [x for x in data]


    def create_id_to_data(self):
        pass


    def get_attribute(self,name):

        index = self._file_format[name]

        return [x[index] for x in self.raw]

        
    
