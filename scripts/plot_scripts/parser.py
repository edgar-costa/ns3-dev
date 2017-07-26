import csv

file_format = {"id":0, "size": 1, "fct": 2, "startTime":3, "endTime":4}

class Parser(object):

    def __init__(self,file_name ="", formater = file_format, delimiter = " "):
        self._file = file_name
        self._file_format = formater
        self._delimiter = delimiter
        self._iter_counter = 0

        #load data
        try:
            self.raw = self.load_file(self._file)
        except:
            #print "Error reading file"
            raise Exception

    def load_file(self,file_name):
        file = open(file_name, 'rb')
        data = csv.reader(file, delimiter=self._delimiter)
        return [x for x in data]

    def __iter__(self):
        return self

    def next(self):
        if self._iter_counter == len(self.raw):
            self._iter_counter = 0
            raise StopIteration

        else:
            self._iter_counter +=1
            return self.raw[self._iter_counter - 1]


    def create_id_to_data(self):
        pass
   
    def get_index(self, name):
        return self._file_format[name]

    def get_attribute(self,name):

        index = self._file_format[name]

        return [x[index] for x in self.raw]

        
    
