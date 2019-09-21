import pickle
import base64
import csv

infile = open('./instruction_file/non_ordered.pickl','rb')
obj = pickle.load(infile)
infile.close()

with open('a.csv', 'w', encoding='utf8') as csv_file:
    wr = csv.writer(csv_file, delimiter='|')
    pickle_bytes = pickle.dumps(obj)            # unsafe to write
    b64_bytes = base64.b64encode(pickle_bytes)  # safe to write but still bytes
    b64_str = b64_bytes.decode('utf8')          # safe and in utf8
    wr.writerow(['col1', 'col2', b64_str])


with open('a.csv', 'r') as csv_file:
    for line in csv_file:
        line = line.strip('\n')
        b64_str = line.split('|')[2]                    # take the pickled obj
        obj = pickle.loads(base64.b64decode(b64_str))   # retrieve
