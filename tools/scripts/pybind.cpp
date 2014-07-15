// mypy.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <python.h>
#include <windows.h>
#include <string>

// GIL lock
class PyGILStateLock  
{  
public:  
	PyGILStateLock(void)  
	{  
		state = PyGILState_Ensure( );  
	}  
	~PyGILStateLock(void)  
	{  
		PyGILState_Release( state );  
	}  
private:  
	PyGILState_STATE state;  
}; 

// thread unlock
class PyThreadStateUnlock  
{  
public:  
	PyThreadStateUnlock(void)  
	{  
		state = PyEval_SaveThread( );  //Py_BEGIN_ALLOW_THREADS
	}  
	~PyThreadStateUnlock(void)  
	{  
		PyEval_RestoreThread( state );  //Py_END_ALLOW_THREADS
	}  
private:  
	PyThreadState* state;  
}; 

// 1. Py_SetProgramName should be first,then setpath,plz look source of setpath
// 2. ignore Py_SetPythonHome if py_setpath invoked
// 3. free gil at end
void PyVmInit(wchar_t *program_path,wchar_t *lib_path)
{
	Py_SetProgramName(program_path);
	Py_SetPath(lib_path);
	Py_Initialize();
	PyEval_InitThreads();
	PyEval_ReleaseThread(PyThreadState_Get());
}

// 1. ensure keep GIL in mainthread when fini
void PyVmFini()
{
	PyGILState_Ensure();
	Py_Finalize();
}

void  PyLogException(std::string& err_msg)
{
	if (!Py_IsInitialized())
	{
		err_msg = "vm is not init";
		return;
	}

	if (PyErr_Occurred() != NULL)
	{
		PyObject *type_obj, *value_obj, *traceback_obj;
		PyErr_Fetch(&type_obj, &value_obj, &traceback_obj);
		if (value_obj == NULL)
			return;

		err_msg.clear();
		PyErr_NormalizeException(&type_obj, &value_obj, 0);
		if (PyUnicode_Check(PyObject_Str(value_obj)))
		{
			err_msg = PyUnicode_AsUTF8(PyObject_Str(value_obj));
		}

		if (traceback_obj != NULL)
		{
			err_msg += "Traceback:";

			PyObject * pModuleName = PyUnicode_FromString("traceback");
			PyObject * pTraceModule = PyImport_Import(pModuleName);
			Py_XDECREF(pModuleName);
			if (pTraceModule != NULL)
			{
				PyObject * pModuleDict = PyModule_GetDict(pTraceModule);
				if (pModuleDict != NULL)
				{
					PyObject * pFunc = PyDict_GetItemString(pModuleDict,"format_exception");
					if (pFunc != NULL)
					{
						PyObject * errList = PyObject_CallFunctionObjArgs(pFunc,type_obj,value_obj,
							traceback_obj,NULL);
						if (errList != NULL)
						{
							int listSize = PyList_Size(errList);
							for (int i=0;i < listSize;++i)
							{
								err_msg += PyUnicode_AsUTF8(PyList_GetItem(errList,i));
							}
						}
					}
				}
				Py_XDECREF(pTraceModule);
			}
		}
		Py_XDECREF(type_obj);
		Py_XDECREF(value_obj);
		Py_XDECREF(traceback_obj);

		//clear error
		PyErr_Clear();
	}
}

BOOL run = true;
int rand_delay = 0;
DWORD  WINAPI thrdFun(void * param)
{
    for (;run;)
    {
        rand_delay++;

		PyGILStateLock gil_lock;
        printf("\n++here:%d\n", GetCurrentThreadId());

		PyRun_SimpleString(
            "import time\n"
			"print('\\nToday is %s\\n'%(time.ctime(time.time())))\n"
            );

		{
			PyThreadStateUnlock thread_lock;
	        Sleep(rand_delay%7);
		}

        printf("\n--here:%d\n", GetCurrentThreadId());
    }

    return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
	//
	// make lib_path,remove debug|release\xyz.exe
	//
	wchar_t lib_path[MAX_PATH]={0};

	wchar_t file_path[MAX_PATH]={0};
	wcscpy_s(file_path,argv[0]);
	wcsrchr(file_path,L'\\')[0]=0;
	wcsrchr(file_path,L'\\')[0]=0;

	wcscpy_s(lib_path,file_path);wcscat_s(lib_path,L";");
	wcscat_s(lib_path,file_path);wcscat_s(lib_path,L"\\libbig.zip");

	//PyVmInit(argv[0],L"C:\\projects\\mypy;C:\\projects\\mypy\\libbig.zip");
	PyVmInit(argv[0],lib_path);

	//
	// create 4 threads
	//
#if 1
    DWORD trd;
    int i;
    for(i=0; i<4; i++)
    {
       CreateThread(NULL, 0, thrdFun, NULL, 0, &trd);
    }    
    Sleep(5000);
    
    run = FALSE;
    Sleep(7);
#endif
	//
	// some test
	//
	{
		PyGILStateLock gil_lock;

		PyRun_SimpleString("x=100");
		PyRun_SimpleString("print(x)");
	
		// import error
		std::string err_msg;

		PyRun_SimpleString("import ssss");
		PyLogException(err_msg);

		PyObject* mod_obj = PyImport_Import(PyUnicode_FromString("hello"));
		Py_XDECREF(mod_obj);
		PyLogException(err_msg);

		// anyfile
		PyRun_AnyFile(stdin,NULL);
	}

	PyVmFini();
	return 0;
}

