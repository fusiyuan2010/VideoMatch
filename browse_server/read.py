import cPickle
import sys

with open(sys.argv[1], 'rb') as f:
    jpg_list = cPickle.load(f)
for jpg_item in jpg_list:
    f2 = open(sys.argv[2] + '/' + jpg_item[0], 'wb')
    f2.write(jpg_item[1])
