# coding = utf8
# usage: python walkdir.py "squirrel\\src" "*.cc;*.h;*.cpp" list.txt

from __future__ import print_function,division 
from future_builtins import *

import os,sys,fnmatch,pprint

# "*.cc;*.h;*.hpp;*.cpp;*.c"
def main():
    if len(sys.argv)!=4:
        print("usage:walkdir.py list_dir patterns list_file")
        return 1
    [root,patterns,list_file] = sys.argv[1:]
    files_list = []
    patterns = patterns.split(';')
    for path, subdirs, files in os.walk(root):
        for name in files:
            for pattern in patterns:
                if fnmatch.fnmatch(name, pattern):
                    files_list.append((os.path.join(path,name)).replace(root+os.sep,"",1).replace("\\","/"))
                    break
    files_list.sort()
    with open(list_file,"w") as fo:
        pprint.pprint(files_list,fo)
    print("done")
    return 0

if __name__ == '__main__':
    sys.exit(main())
