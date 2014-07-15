# by hege

import os
import sys
import zipfile

# package
import ctypes
import logging
import sqlite3

# lib path
lib_path = os.path.dirname(os.__file__)

# create small zipfile with just a few libraries
def make_smallzip():
	zip_path = "libsmall.zip"
	zip_path = os.path.abspath(os.path.join(os.path.dirname(__file__),zip_path))
	if os.path.exists(zip_path):
		os.unlink(zip_path)
	zip_obj = zipfile.PyZipFile(zip_path,"w",compression=zipfile.ZIP_DEFLATED)
	zip_obj.writepy(os.path.dirname(ctypes.__file__))
	zip_obj.writepy(os.path.dirname(logging.__file__))
	zip_obj.writepy(os.path.dirname(sqlite3.__file__))
	zip_obj.close()

# create medium zipfile with everything in top-level stdlib
def make_mediumzip():
	zip_path = "libmedium.zip"
	if os.path.exists(zip_path):
		os.unlink(zip_path)
	zip_obj = zipfile.PyZipFile(zip_path,"w",compression=zipfile.ZIP_DEFLATED)
	zip_obj.writepy(lib_path)
	zip_obj.close()

# create large zipfile with everything we can find
def make_bigzip():
	zip_path = "libbig.zip"
	if os.path.exists(zip_path):
		os.unlink(zip_path)
	#zip_obj = zipfile.PyZipFile(zip_path,"w",compression=zipfile.ZIP_DEFLATED,optimize=-1)
	zip_obj = zipfile.PyZipFile(zip_path,"w",compression=zipfile.ZIP_DEFLATED)
	zip_obj.writepy(lib_path)
	for (root,dirs,files) in os.walk(lib_path):
		if os.path.basename(root) in ("idlelib","test","tkinter","turtledemo",):
			del dirs[:]
			continue
		if "__init__.py" in files:
			del dirs[:]
			try:
				#print(root)
				zip_obj.writepy(root)
			except (EnvironmentError,SyntaxError,):
				pass
	zip_obj.close()

if __name__ == "__main__":
	make_smallzip()
	make_mediumzip()
	make_bigzip()