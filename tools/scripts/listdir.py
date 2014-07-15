# coding = utf8

from __future__ import print_function, unicode_literals, division 
from future_builtins import *

import os
import sys
import pprint

def main():
    if len(sys.argv)!=3:
        print("usage:listdir.py list_dir list_file")
        return 1
    [list_dir,list_file] = sys.argv[1:]
    with open(list_file,"w") as fo:
        content = os.listdir(list_dir)
        pprint.pprint(content,fo)
    print("done")
    return 0

if __name__ == "__main__":
    sys.exit(main())
