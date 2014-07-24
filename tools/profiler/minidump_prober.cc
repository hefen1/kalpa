#include "tools/profiler/minidump_prober.h"

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <assert.h>

#include "tools/profiler/minidump_reader.h"
#include "tools/profiler/strconv.h"

//////////////////////////////////////////////////////////////////////////
namespace{

// Character set independent string type
typedef std::basic_string<TCHAR> tstring;

// We want to use secure version of _stprintf function when possible
int __STPRINTF_S(TCHAR* buffer, size_t sizeOfBuffer, const TCHAR* format, ... )
{
    va_list args; 
    va_start(args, format);

#if _MSC_VER<1400
    UNREFERENCED_PARAMETER(sizeOfBuffer);
    return _vstprintf(buffer, format, args);
#else
    return _vstprintf_s(buffer, sizeOfBuffer, format, args);
#endif
}

// We want to use secure version of _tfopen when possible
#if _MSC_VER<1400
#define _TFOPEN_S(_File, _Filename, _Mode) _File = _tfopen(_Filename, _Mode);
#else
#define _TFOPEN_S(_File, _Filename, _Mode) _tfopen_s(&(_File), _Filename, _Mode);
#endif

#if _MSC_VER<1400
#define WCSNCPY_S(strDest, sizeInBytes, strSource, count) wcsncpy(strDest, strSource, count)
#define WCSCPY_S(strDestination, numberOfElements, strSource) wcscpy(strDestination, strSource)
#define STRCPY_S(strDestination, numberOfElements, strSource) strcpy(strDestination, strSource)
#define _TCSCPY_S(strDestination, numberOfElements, strSource) _tcscpy(strDestination, strSource)
#define _LTOT_S(value, str, sizeOfStr, radix) _ltot(value, str, radix)
#define _ULTOT_S(value, str, sizeOfStr, radix) _ultot(value, str, radix)
#define _TCSCAT_S(strDestination, size, strSource) _tcscat(strDestination, strSource)
#else
#define WCSNCPY_S(strDest, sizeInBytes, strSource, count) wcsncpy_s(strDest, sizeInBytes, strSource, count)
#define WCSCPY_S(strDestination, numberOfElements, strSource) wcscpy_s(strDestination, numberOfElements, strSource)
#define STRCPY_S(strDestination, numberOfElements, strSource) strcpy_s(strDestination, numberOfElements, strSource)
#define _TCSCPY_S(strDestination, numberOfElements, strSource) _tcscpy_s(strDestination, numberOfElements, strSource)
#define _LTOT_S(value, str, sizeOfStr, radix) _ltot_s(value, str, sizeOfStr, radix)
#define _ULTOT_S(value, str, sizeOfStr, radix) _ultot_s(value, str, sizeOfStr, radix)
#define _TCSCAT_S(strDestination, size, strSource) _tcscat_s(strDestination, size, strSource)
#endif

int _STPRINTF_S(TCHAR* buffer, size_t sizeOfBuffer, const TCHAR* format, ... )
{
  va_list args; 
  va_start(args, format);

#if _MSC_VER<1400
  UNREFERENCED_PARAMETER(sizeOfBuffer);
  return _vstprintf(buffer, format, args);
#else
  return _vstprintf_s(buffer, sizeOfBuffer, format, args);
#endif
}

// COutputter
// This class is used for generating the content of the resulting file.
// Currently text format is supported.
class COutputter
{
public:

    void Init(FILE* f)
    {
        assert(f!=NULL);
        m_fOut = f;    
    }

    void BeginDocument(LPCTSTR pszTitle)
    {
        _ftprintf(m_fOut, _T("= %s = \n\n"), pszTitle);    
    }

    void EndDocument()
    {    
        _ftprintf(m_fOut, _T("\n== END ==\n"));
    }

    void BeginSection(LPCTSTR pszTitle)
    {
        _ftprintf(m_fOut, _T("== %s ==\n\n"), pszTitle);
    }

    void EndSection()
    {
        _ftprintf(m_fOut, _T("\n\n"));
    }

    void PutRecord(LPCTSTR pszName, LPCTSTR pszValue)
    {
        _ftprintf(m_fOut, _T("%s = %s\n"), pszName, pszValue);
    }

    void PutTableCell(LPCTSTR pszValue, int width, bool bLastInRow)
    {
        TCHAR szFormat[32];
        __STPRINTF_S(szFormat, 32, _T("%%-%ds%s"), width, bLastInRow?_T("\n"):_T(" "));
        _ftprintf(m_fOut, szFormat, pszValue);
    }

private:

    FILE* m_fOut;
};


/* Table names passed to crpGetProperty() function. */
#define CRP_TBL_MDMP_MISC    _T("MdmpMisc")    //!< Table: Miscellaneous info contained in crash minidump file.  
#define CRP_TBL_MDMP_MODULES _T("MdmpModules") //!< Table: The list of loaded modules.
#define CRP_TBL_MDMP_THREADS _T("MdmpThreads") //!< Table: The list of threads.
#define CRP_TBL_MDMP_LOAD_LOG _T("MdmpLoadLog") //!< Table: Minidump loading log.

/* Meta information */
#define CRP_META_ROW_COUNT _T("RowCount") //!< Row count in the table.  

/* Column names passed to crpGetProperty() function. */

// Column IDs of the CRP_MDMP_MISC table
#define CRP_COL_CPU_ARCHITECTURE _T("CPUArchitecture") //!< Column: Processor architecture.
#define CRP_COL_CPU_COUNT        _T("CPUCount")        //!< Column: Number of processors.
#define CRP_COL_PRODUCT_TYPE     _T("ProductType")     //!< Column: Type of system (server or workstation).
#define CRP_COL_OS_VER_MAJOR     _T("OSVerMajor")      //!< Column: OS major version.
#define CRP_COL_OS_VER_MINOR     _T("OSVerMinor")      //!< Column: OS minor version.
#define CRP_COL_OS_VER_BUILD     _T("OSVerBuild")      //!< Column: OS build number.
#define CRP_COL_OS_VER_CSD       _T("OSVerCSD")        //!< Column: The latest service pack installed.
#define CRP_COL_EXCPTRS_EXCEPTION_CODE _T("ExptrsExceptionCode")  //!< Column: Code of the structured exception.
#define CRP_COL_EXCEPTION_ADDRESS _T("ExceptionAddress")          //!< Column: Exception address.
#define CRP_COL_EXCEPTION_THREAD_ROWID _T("ExceptionThreadROWID") //!< Column: ROWID in \ref CRP_TBL_MDMP_THREADS of the thread in which exception occurred.
#define CRP_COL_EXCEPTION_THREAD_STACK_MD5  _T("ExceptionThreadStackMD5") //!< Column: MD5 hash of the stack trace of the thread where exception occurred.
#define CRP_COL_EXCEPTION_MODULE_ROWID _T("ExceptionModuleROWID") //!< Column: ROWID in \ref CRP_TBL_MDMP_MODULES of the module in which exception occurred.

// Column IDs of the CRP_MDMP_MODULES table
#define CRP_COL_MODULE_NAME      _T("ModuleName")           //!< Column: Module name.
#define CRP_COL_MODULE_IMAGE_NAME _T("ModuleImageName")     //!< Column: Image name containing full path.  
#define CRP_COL_MODULE_BASE_ADDRESS _T("ModuleBaseAddress") //!< Column: Module base load address.
#define CRP_COL_MODULE_SIZE      _T("ModuleSize")           //!< Column: Module size.
#define CRP_COL_MODULE_LOADED_PDB_NAME _T("LoadedPDBName")  //!< Column: The full path and file name of the .pdb file. 
#define CRP_COL_MODULE_LOADED_IMAGE_NAME _T("LoadedImageName")  //!< Column: The full path and file name of executable file.
#define CRP_COL_MODULE_SYM_LOAD_STATUS _T("ModuleSymLoadStatus") //!< Column: Symbol load status for the module.

// Column IDs of the CRP_MDMP_THREADS table
#define CRP_COL_THREAD_ID            _T("ThdeadID")           //!< Column: Thread ID.
#define CRP_COL_THREAD_STACK_TABLEID _T("ThreadStackTABLEID") //!< Column: The table ID of the table containing stack trace for this thread.

// Column IDs of a stack trace table
#define CRP_COL_STACK_MODULE_ROWID     _T("StackModuleROWID")    //!< Column: Stack trace: ROWID of the module in the CRP_TBL_MODULES table.
#define CRP_COL_STACK_SYMBOL_NAME      _T("StackSymbolName")     //!< Column: Stack trace: symbol name.
#define CRP_COL_STACK_OFFSET_IN_SYMBOL _T("StackOffsetInSymbol") //!< Column: Stack trace: offset in symbol, hexadecimal.
#define CRP_COL_STACK_SOURCE_FILE      _T("StackSourceFile")     //!< Column: Stack trace: source file name.
#define CRP_COL_STACK_SOURCE_LINE      _T("StackSourceLine")     //!< Column: Stack trace: source file line number.
#define CRP_COL_STACK_ADDR_PC_OFFSET   _T("StackAddrPCOffset")   //!< Column: Stack trace: AddrPC offset.

// Column IDs of the CRP_MDMP_LOAD_LOG table
#define CRP_COL_LOAD_LOG_ENTRY _T("LoadLogEntry")   //!< Column: A entry of the minidump loading log.

// helper
int ParseDynTableId(CString sTableId, int& index)
{
  if(sTableId.Left(5)=="STACK")
  {
    CString sIndex = sTableId.Mid(5);
    index = _ttoi(sIndex.GetBuffer(0));
    return 0;
  }

  return -1;
}

CString FormatErrorMsg(DWORD dwErrorCode)
{
  LPTSTR msg = 0;
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_ALLOCATE_BUFFER,
    NULL, dwErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    (LPTSTR)&msg, 0, NULL);
  CString str = msg;
  str.Replace(_T("\r\n"), _T(""));
  GlobalFree(msg);
  return str;
}

// Helper function thatr etrieves an error report property
int crpGetPropertyW(CMiniDumpReader* reader,LPCWSTR lpszTableId,LPCWSTR lpszColumnId,INT nRowIndex,
                LPWSTR lpszBuffer,ULONG cchBuffSize,PULONG pcchCount)
{
  // Set default output values
  if(lpszBuffer!=NULL && cchBuffSize>=1)
    lpszBuffer[0] = 0; // Empty buffer
  if(pcchCount!=NULL)
    *pcchCount = 0;  

  LPCWSTR pszPropVal = NULL;
  const int BUFF_SIZE = 4096; 
  TCHAR szBuff[BUFF_SIZE]; // Internal buffer to store property value
  strconv_t strconv; // String convertor object

  // Validate input parameters
  if( lpszTableId==NULL ||
    lpszColumnId==NULL ||
    nRowIndex<0 || // Check we have non-negative row index
    (lpszBuffer==NULL && cchBuffSize!=0) || // Check that we have a valid buffer
    (lpszBuffer!=NULL && cchBuffSize==0)
    )
  {
    return -1;
  }

  CMiniDumpReader* pDmpReader = reader;

  CString sTableId = lpszTableId;
  CString sColumnId = lpszColumnId;
  int nDynTableIndex = -1;
  // nDynTable will be equal to 0 if a stack trace table is queired
  int nDynTable = ParseDynTableId(sTableId, nDynTableIndex);
  if(nDynTable==0){
      pDmpReader->StackWalk(pDmpReader->m_DumpData.m_Threads[nDynTableIndex].m_dwThreadId);
  }   

  if(sTableId.Compare(CRP_TBL_MDMP_MISC)==0)
  {
    if(nRowIndex!=0)
    {
      return -4;    
    }

    if(sColumnId.Compare(CRP_META_ROW_COUNT)==0)
    {
      return 1; // there is 1 row in this table
    }
    else if(sColumnId.Compare(CRP_COL_CPU_ARCHITECTURE)==0)
    {
      _ULTOT_S(pDmpReader->m_DumpData.m_uProcessorArchitecture, szBuff, BUFF_SIZE, 10);
      _TCSCAT_S(szBuff, BUFF_SIZE, _T(" "));

      TCHAR* szDescription = _T("unknown processor type");
      if(pDmpReader->m_DumpData.m_uProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64)
        szDescription = _T("x64 (AMD or Intel)");
      if(pDmpReader->m_DumpData.m_uProcessorArchitecture==PROCESSOR_ARCHITECTURE_IA32_ON_WIN64)
        szDescription = _T("WOW");
      if(pDmpReader->m_DumpData.m_uProcessorArchitecture==PROCESSOR_ARCHITECTURE_IA64)
        szDescription = _T("Intel Itanium Processor Family (IPF)");
      if(pDmpReader->m_DumpData.m_uProcessorArchitecture==PROCESSOR_ARCHITECTURE_INTEL)
        szDescription = _T("x86");

      _TCSCAT_S(szBuff, BUFF_SIZE, szDescription);

      pszPropVal = szBuff;        
    }
    else if(sColumnId.Compare(CRP_COL_CPU_COUNT)==0)
    {
      _ULTOT_S(pDmpReader->m_DumpData.m_uchNumberOfProcessors, szBuff, BUFF_SIZE, 10);
      pszPropVal = szBuff;        
    }
    else if(sColumnId.Compare(CRP_COL_PRODUCT_TYPE)==0)
    {
      _ULTOT_S(pDmpReader->m_DumpData.m_uchProductType, szBuff, BUFF_SIZE, 10);
      _TCSCAT_S(szBuff, BUFF_SIZE, _T(" "));

      TCHAR* szDescription = _T("unknown product type");
      if(pDmpReader->m_DumpData.m_uchProductType==VER_NT_DOMAIN_CONTROLLER)
        szDescription = _T("domain controller");
      if(pDmpReader->m_DumpData.m_uchProductType==VER_NT_SERVER)
        szDescription = _T("server");
      if(pDmpReader->m_DumpData.m_uchProductType==VER_NT_WORKSTATION)
        szDescription = _T("workstation");

      _TCSCAT_S(szBuff, BUFF_SIZE, szDescription);

      pszPropVal = szBuff;        
    }
    else if(sColumnId.Compare(CRP_COL_OS_VER_MAJOR)==0)
    {      
      _ULTOT_S(pDmpReader->m_DumpData.m_ulVerMajor, szBuff, BUFF_SIZE, 10);
      pszPropVal = szBuff;        
    }
    else if(sColumnId.Compare(CRP_COL_OS_VER_MINOR)==0)
    {
      _ULTOT_S(pDmpReader->m_DumpData.m_ulVerMinor, szBuff, BUFF_SIZE, 10);
      pszPropVal = szBuff;        
    }
    else if(sColumnId.Compare(CRP_COL_OS_VER_BUILD)==0)
    {
      _ULTOT_S(pDmpReader->m_DumpData.m_ulVerBuild, szBuff, BUFF_SIZE, 10);
      pszPropVal = szBuff;        
    }
    else if(sColumnId.Compare(CRP_COL_OS_VER_CSD)==0)
    {      
      pszPropVal = strconv.t2w(pDmpReader->m_DumpData.m_sCSDVer);    
    }
    else if(sColumnId.Compare(CRP_COL_EXCPTRS_EXCEPTION_CODE)==0)
    { 
      if(!pDmpReader->m_bReadExceptionStream)
      {
        return -3;
      }
      _STPRINTF_S(szBuff, BUFF_SIZE, _T("0x%x"), pDmpReader->m_DumpData.m_uExceptionCode); 
      _TCSCAT_S(szBuff, BUFF_SIZE, _T(" "));
      CString msg = FormatErrorMsg(pDmpReader->m_DumpData.m_uExceptionCode);
      _TCSCAT_S(szBuff, BUFF_SIZE, msg);
      pszPropVal = szBuff;
    }
    else if(sColumnId.Compare(CRP_COL_EXCEPTION_ADDRESS)==0)
    {  
      if(!pDmpReader->m_bReadExceptionStream)
      {
        return -3;
      }
      _STPRINTF_S(szBuff, BUFF_SIZE, _T("0x%I64x"), pDmpReader->m_DumpData.m_uExceptionAddress); 
      pszPropVal = szBuff;
    }
    else if(sColumnId.Compare(CRP_COL_EXCEPTION_THREAD_ROWID)==0)
    {  
      if(!pDmpReader->m_bReadExceptionStream)
      {
        return -3;
      }
      _STPRINTF_S(szBuff, BUFF_SIZE, _T("%d"), pDmpReader->GetThreadRowIdByThreadId(pDmpReader->m_DumpData.m_uExceptionThreadId)); 
      pszPropVal = szBuff;
    }
    else if(sColumnId.Compare(CRP_COL_EXCEPTION_MODULE_ROWID)==0)
    {  
      if(!pDmpReader->m_bReadExceptionStream)
      {
        return -3;
      }
      _STPRINTF_S(szBuff, BUFF_SIZE, _T("%d"), pDmpReader->GetModuleRowIdByAddress(pDmpReader->m_DumpData.m_uExceptionAddress)); 
      pszPropVal = szBuff;
    }
    else if(sColumnId.Compare(CRP_COL_EXCEPTION_THREAD_STACK_MD5)==0)
    {  
      if(!pDmpReader->m_bReadExceptionStream)
      {
        return -3;
      }
      int nThreadROWID = pDmpReader->GetThreadRowIdByThreadId(pDmpReader->m_DumpData.m_uExceptionThreadId);
      if(nThreadROWID>=0)
      {
        pDmpReader->StackWalk(pDmpReader->m_DumpData.m_Threads[nThreadROWID].m_dwThreadId);
        CString sMD5 = pDmpReader->m_DumpData.m_Threads[nThreadROWID].m_sStackTraceMD5;        
        _STPRINTF_S(szBuff, BUFF_SIZE, _T("%s"), sMD5.GetBuffer(0));         
      }
      pszPropVal = szBuff;      
    }
    else
    {
      return -2;
    }
  }
  else if(sTableId.Compare(CRP_TBL_MDMP_MODULES)==0)
  {  
    if(nRowIndex>=(int)pDmpReader->m_DumpData.m_Modules.size())
    {
      return -4;    
    }

    if(sColumnId.Compare(CRP_META_ROW_COUNT)==0)
    {
      return (int)pDmpReader->m_DumpData.m_Modules.size();
    }
    else if(sColumnId.Compare(CRP_COL_MODULE_NAME)==0)
    {      
      pszPropVal = strconv.t2w(pDmpReader->m_DumpData.m_Modules[nRowIndex].m_sModuleName);    
    }
    else if(sColumnId.Compare(CRP_COL_MODULE_IMAGE_NAME)==0)
    {      
      pszPropVal = strconv.t2w(pDmpReader->m_DumpData.m_Modules[nRowIndex].m_sImageName);    
    }
    else if(sColumnId.Compare(CRP_COL_MODULE_BASE_ADDRESS)==0)
    {      
      _STPRINTF_S(szBuff, BUFF_SIZE, _T("0x%I64x"), pDmpReader->m_DumpData.m_Modules[nRowIndex].m_uBaseAddr); 
      pszPropVal = szBuff;
    }
    else if(sColumnId.Compare(CRP_COL_MODULE_SIZE)==0)
    {
      _STPRINTF_S(szBuff, BUFF_SIZE, _T("%I64u"), pDmpReader->m_DumpData.m_Modules[nRowIndex].m_uImageSize); 
      pszPropVal = szBuff;
    }
    else if(sColumnId.Compare(CRP_COL_MODULE_LOADED_PDB_NAME)==0)
    {
      pszPropVal = strconv.t2w(pDmpReader->m_DumpData.m_Modules[nRowIndex].m_sLoadedPdbName);          
    }
    else if(sColumnId.Compare(CRP_COL_MODULE_LOADED_IMAGE_NAME)==0)
    {
      pszPropVal = strconv.t2w(pDmpReader->m_DumpData.m_Modules[nRowIndex].m_sLoadedImageName);          
    }
    else if(sColumnId.Compare(CRP_COL_MODULE_SYM_LOAD_STATUS)==0)
    {
      CString sSymLoadStatus;
      MdmpModule m = pDmpReader->m_DumpData.m_Modules[nRowIndex];
      if(m.m_bImageUnmatched)
        sSymLoadStatus = _T("No matching binary found.");          
      else if(m.m_bPdbUnmatched)
        sSymLoadStatus = _T("No matching PDB file found.");          
      else
      {
        if(m.m_bNoSymbolInfo)            
          sSymLoadStatus = _T("No symbols loaded.");          
        else
          sSymLoadStatus = _T("Symbols loaded.");          
      }

#if _MSC_VER < 1400
      _tcscpy(szBuff, sSymLoadStatus.GetBuffer(0));
#else
      _tcscpy_s(szBuff, BUFF_SIZE, sSymLoadStatus.GetBuffer(0));
#endif

      pszPropVal = strconv.t2w(szBuff);          
    }    
    else
    {
      return -2;
    }
  }
  else if(sTableId.Compare(CRP_TBL_MDMP_THREADS)==0)
  {  
    if(nRowIndex>=(int)pDmpReader->m_DumpData.m_Threads.size())
    {
      return -4;    
    }

    if(sColumnId.Compare(CRP_META_ROW_COUNT)==0)
    {
      return (int)pDmpReader->m_DumpData.m_Threads.size();
    }
    else if(sColumnId.Compare(CRP_COL_THREAD_ID)==0)
    {
      _STPRINTF_S(szBuff, BUFF_SIZE, _T("0x%x"), pDmpReader->m_DumpData.m_Threads[nRowIndex].m_dwThreadId); 
      pszPropVal = szBuff;
    }
    else if(sColumnId.Compare(CRP_COL_THREAD_STACK_TABLEID)==0)
    {
      _STPRINTF_S(szBuff, BUFF_SIZE, _T("STACK%d"), nRowIndex); 
      pszPropVal = szBuff;
    }    
    else
    {
      return -2;
    }

  }  
  else if(sTableId.Compare(CRP_TBL_MDMP_LOAD_LOG)==0)
  {  
    if(nRowIndex>=(int)pDmpReader->m_DumpData.m_LoadLog.size())
    {
      return -4;    
    }

    if(sColumnId.Compare(CRP_META_ROW_COUNT)==0)
    {
      return (int)pDmpReader->m_DumpData.m_LoadLog.size();
    }
    else if(sColumnId.Compare(CRP_COL_LOAD_LOG_ENTRY)==0)
    {
      _TCSCPY_S(szBuff, BUFF_SIZE, pDmpReader->m_DumpData.m_LoadLog[nRowIndex]); 
      pszPropVal = szBuff;
    }
    else
    {
      return -2;
    }

  }  
  else if(nDynTable==0)
  {
    int nEntryIndex = nDynTableIndex;

    // Ensure we walked the stack for this thread
    assert(pDmpReader->m_DumpData.m_Threads[nEntryIndex].m_bStackWalk);

    if(nRowIndex>=(int)pDmpReader->m_DumpData.m_Threads[nEntryIndex].m_StackTrace.size())
    {
      return -4;    
    }

    if(sColumnId.Compare(CRP_META_ROW_COUNT)==0)
    {
      return (int)pDmpReader->m_DumpData.m_Threads[nEntryIndex].m_StackTrace.size();
    }
    else if(sColumnId.Compare(CRP_COL_STACK_OFFSET_IN_SYMBOL)==0)
    {      
      _STPRINTF_S(szBuff, BUFF_SIZE, _T("0x%I64x"), pDmpReader->m_DumpData.m_Threads[nEntryIndex].m_StackTrace[nRowIndex].m_dw64OffsInSymbol);      
      pszPropVal = szBuff;                  
    }
    else if(sColumnId.Compare(CRP_COL_STACK_ADDR_PC_OFFSET)==0)
    {       
      _STPRINTF_S(szBuff, BUFF_SIZE, _T("0x%I64x"), pDmpReader->m_DumpData.m_Threads[nEntryIndex].m_StackTrace[nRowIndex].m_dwAddrPCOffset);
      pszPropVal = szBuff;                
    }
    else if(sColumnId.Compare(CRP_COL_STACK_SOURCE_LINE)==0)
    {       
      _ULTOT_S(pDmpReader->m_DumpData.m_Threads[nEntryIndex].m_StackTrace[nRowIndex].m_nSrcLineNumber, szBuff, BUFF_SIZE, 10);
      pszPropVal = szBuff;                
    }  
    else if(sColumnId.Compare(CRP_COL_STACK_MODULE_ROWID)==0)
    {  
      _LTOT_S(pDmpReader->m_DumpData.m_Threads[nEntryIndex].m_StackTrace[nRowIndex].m_nModuleRowID, szBuff, BUFF_SIZE, 10);
      pszPropVal = szBuff;       
    }
    else if(sColumnId.Compare(CRP_COL_STACK_SYMBOL_NAME)==0)
    {      
      pszPropVal = strconv.t2w(pDmpReader->m_DumpData.m_Threads[nEntryIndex].m_StackTrace[nRowIndex].m_sSymbolName);    
    }
    else if(sColumnId.Compare(CRP_COL_STACK_SOURCE_FILE)==0)
    {      
      pszPropVal = strconv.t2w(pDmpReader->m_DumpData.m_Threads[nEntryIndex].m_StackTrace[nRowIndex].m_sSrcFileName);    
    }
    else
    {
      return -2;
    }
  }
  else
  {
    return -3;
  }


  // Check the provided buffer size
  if(lpszBuffer==NULL || cchBuffSize==0)
  {
    // User wants us to get the required length of the buffer
    if(pcchCount!=NULL)
    {
      *pcchCount = pszPropVal!=NULL?(ULONG)wcslen(pszPropVal):0;
    }
  }
  else
  {
    // User wants us to return the property value 
    ULONG uRequiredLen = pszPropVal!=NULL?(ULONG)wcslen(pszPropVal):0;
    if(uRequiredLen>(cchBuffSize))
    {
      return uRequiredLen;
    }

    // Copy the property to the buffer
    if(pszPropVal!=NULL)
      WCSCPY_S(lpszBuffer, cchBuffSize, pszPropVal);

    if(pcchCount!=NULL)
    {
      *pcchCount = uRequiredLen;
    }
  }

  // Done.
  return 0;
}

int get_prop(CMiniDumpReader* reader, LPCTSTR table_id, LPCTSTR column_id, tstring& str, int row_id=0);
int get_prop(CMiniDumpReader* reader, LPCTSTR table_id, LPCTSTR column_id, tstring& str, int row_id)
{
  const int BUFF_SIZE = 1024;
  TCHAR buffer[BUFF_SIZE];  
  int result = crpGetPropertyW(reader, table_id, column_id, row_id, buffer, BUFF_SIZE, NULL);
  if(result==0)
    str = buffer;
  return result;
}

int get_table_row_count(CMiniDumpReader* reader, LPCTSTR table_id)
{
  return crpGetPropertyW(reader, table_id, CRP_META_ROW_COUNT, 0, NULL, 0, NULL);
}

}

//////////////////////////////////////////////////////////////////////////
CMiniDumpProber::CMiniDumpProber()
:reader_(NULL){

}

CMiniDumpProber::~CMiniDumpProber(){
  if(reader_){
    delete reader_;
  }
}

bool CMiniDumpProber::Open(const std::wstring& dump_path,const std::wstring& sym_path){
  if(reader_){
    return false;
  }
  reader_ = new CMiniDumpReader();
  if(!reader_ ->CheckDbgHelpApiVersion()){
    return false;
  }
  if(reader_->Open(dump_path.c_str(),sym_path.c_str())){
    return false;
  }

  return true;
}

bool CMiniDumpProber::Export(const std::wstring& log_path){
  if(!reader_){
    return false;
  }

  // Open resulting file
  FILE* f = NULL;      
  _TFOPEN_S(f, log_path.c_str(), _T("wt"));
  if(f==NULL){
    return false;
  }

  doExport(f);

  if(f){
    fclose(f);
  }
  return true;
}

void CMiniDumpProber::doExport(FILE* f){
  COutputter doc;
  int result = 0;

  doc.Init(f);
  doc.BeginDocument(_T("Error Report"));

  doc.BeginSection(_T("Summary"));

  tstring sOsVerMajor;
  result = get_prop(reader_, CRP_TBL_MDMP_MISC, CRP_COL_OS_VER_MAJOR, sOsVerMajor);
  tstring sOsVerMinor;
  result = get_prop(reader_, CRP_TBL_MDMP_MISC, CRP_COL_OS_VER_MINOR, sOsVerMinor);
  tstring sOsVerBuild;
  result = get_prop(reader_, CRP_TBL_MDMP_MISC, CRP_COL_OS_VER_BUILD, sOsVerBuild);
  tstring sOsVerCSD;
  result = get_prop(reader_, CRP_TBL_MDMP_MISC, CRP_COL_OS_VER_CSD, sOsVerCSD);

  tstring sOsVer;
  sOsVer += sOsVerMajor;
  sOsVer += _T(".");
  sOsVer += sOsVerMinor;
  sOsVer += _T(".");
  sOsVer += sOsVerBuild;
  sOsVer += _T(" ");
  sOsVer += sOsVerCSD; 

  doc.PutRecord(_T("OS version (from minidump)"), sOsVer.c_str());

  // Print SystemType
  tstring sSystemType;
  result = get_prop(reader_, CRP_TBL_MDMP_MISC, CRP_COL_PRODUCT_TYPE, sSystemType);
  if(result==0)
    doc.PutRecord(_T("Product type"), sSystemType.c_str());

  // Print ProcessorArchitecture
  tstring sProcessorArchitecture;
  result = get_prop(reader_, CRP_TBL_MDMP_MISC, CRP_COL_CPU_ARCHITECTURE, sProcessorArchitecture);
  if(result==0)
    doc.PutRecord(_T("CPU architecture"), sProcessorArchitecture.c_str());

  // Print NumberOfProcessors
  tstring sCPUCount;
  result = get_prop(reader_, CRP_TBL_MDMP_MISC, CRP_COL_CPU_COUNT, sCPUCount);
  if(result==0)
    doc.PutRecord(_T("CPU count"), sCPUCount.c_str());

  tstring sExceptionAddress;
  result = get_prop(reader_, CRP_TBL_MDMP_MISC, CRP_COL_EXCEPTION_ADDRESS, sExceptionAddress);
  if(result==0)
    doc.PutRecord(_T("Exception address"), sExceptionAddress.c_str());

  tstring sExceptionCode;
  result = get_prop(reader_, CRP_TBL_MDMP_MISC, CRP_COL_EXCPTRS_EXCEPTION_CODE, sExceptionCode);
  if(result==0)
    doc.PutRecord(_T("SEH exception code (from minidump)"), sExceptionCode.c_str());

  tstring sExceptionThreadRowID;  
  result = get_prop(reader_, CRP_TBL_MDMP_MISC, CRP_COL_EXCEPTION_THREAD_ROWID, sExceptionThreadRowID);
  int uExceptionThreadRowID = _ttoi(sExceptionThreadRowID.c_str());
  if(result==0)
  {
    tstring sExceptionThreadId;
    result = get_prop(reader_, CRP_TBL_MDMP_THREADS, CRP_COL_THREAD_ID, sExceptionThreadId, uExceptionThreadRowID);
    if(result==0)
      doc.PutRecord(_T("Exception thread ID"), sExceptionThreadId.c_str());
  }

  tstring sExceptionModuleRowID;  
  result = get_prop(reader_, CRP_TBL_MDMP_MISC, CRP_COL_EXCEPTION_MODULE_ROWID, sExceptionModuleRowID);
  int uExceptionModuleRowID = _ttoi(sExceptionModuleRowID.c_str());
  if(result==0)
  {
    tstring sExceptionModuleName;
    result = get_prop(reader_, CRP_TBL_MDMP_MODULES, CRP_COL_MODULE_NAME, sExceptionModuleName, uExceptionModuleRowID);
    if(result==0)
      doc.PutRecord(_T("Exception module name"), sExceptionModuleName.c_str());
  }

  doc.EndSection();

  int nThreadCount = get_table_row_count(reader_, CRP_TBL_MDMP_THREADS);
  for(int i=0; i<nThreadCount; i++)
  { 
    tstring sThreadId;
    result = get_prop(reader_, CRP_TBL_MDMP_THREADS, CRP_COL_THREAD_ID, sThreadId, i);
    if(result==0)
    {
      tstring str = _T("Stack trace for thread ");
      str += sThreadId;
      doc.BeginSection(str.c_str());

      doc.PutTableCell(_T("Frame"), 32, true);

      tstring sStackTableId;
      get_prop(reader_, CRP_TBL_MDMP_THREADS, CRP_COL_THREAD_STACK_TABLEID, sStackTableId, i);

      BOOL bMissingFrames=FALSE;
      int nFrameCount = get_table_row_count(reader_, sStackTableId.c_str());
      int j;
      for(j=0; j<nFrameCount; j++)
      {        
        tstring sModuleName;
        tstring sAddrPCOffset;
        tstring sSymbolName;            
        tstring sOffsInSymbol;
        tstring sSourceFile;
        tstring sSourceLine;

        tstring sModuleRowId;
        result = get_prop(reader_, sStackTableId.c_str(), CRP_COL_STACK_MODULE_ROWID, sModuleRowId, j);
        if(result==0)
        {
          int nModuleRowId = _ttoi(sModuleRowId.c_str());              
          if(nModuleRowId==-1)
          {            
            if(!bMissingFrames)
              doc.PutTableCell(_T("[Frames below may be incorrect and/or missing]"), 32, true);                                
            bMissingFrames = TRUE;
          }
          get_prop(reader_, CRP_TBL_MDMP_MODULES, CRP_COL_MODULE_NAME, sModuleName, nModuleRowId);              
        }      

        get_prop(reader_, sStackTableId.c_str(), CRP_COL_STACK_ADDR_PC_OFFSET, sAddrPCOffset, j);        
        get_prop(reader_, sStackTableId.c_str(), CRP_COL_STACK_SYMBOL_NAME, sSymbolName, j);        
        get_prop(reader_, sStackTableId.c_str(), CRP_COL_STACK_OFFSET_IN_SYMBOL, sOffsInSymbol, j);              
        get_prop(reader_, sStackTableId.c_str(), CRP_COL_STACK_SOURCE_FILE, sSourceFile, j);              
        get_prop(reader_, sStackTableId.c_str(), CRP_COL_STACK_SOURCE_LINE, sSourceLine, j);              

        tstring str;
        str = sModuleName;
        if(!str.empty())
          str += _T("!");

        if(sSymbolName.empty())
          str += sAddrPCOffset;  
        else
        {
          str += sSymbolName;
          str += _T("+");
          str += sOffsInSymbol;
        }

        if(!sSourceFile.empty())
        {
          size_t pos = sSourceFile.rfind('\\');
          if(pos>=0)
            sSourceFile = sSourceFile.substr(pos+1);
          str += _T(" [ ");
          str += sSourceFile;
          str += _T(": ");
          str += sSourceLine;
          str += _T(" ] ");
        } 

        doc.PutTableCell(str.c_str(), 32, true);                    
      }       

      doc.EndSection();
    }
  }

  // Print module list
  doc.BeginSection(_T("Module List"));

  doc.PutTableCell(_T("#"), 2, false);
  doc.PutTableCell(_T("Name"), 32, false);
  doc.PutTableCell(_T("SymLoadStatus"), 32, false);
  doc.PutTableCell(_T("LoadedPDBName"), 48, false);
  doc.PutTableCell(_T("LoadedImageName"), 48, true);

  // Get module count
  int nItemCount = get_table_row_count(reader_, CRP_TBL_MDMP_MODULES);
  for(int i=0; i<nItemCount; i++)
  {
    TCHAR szBuffer[10];
    __STPRINTF_S(szBuffer, 10, _T("%d"), i+1);
    doc.PutTableCell(szBuffer, 2, false);

    tstring sModuleName;
    result = get_prop(reader_, CRP_TBL_MDMP_MODULES, CRP_COL_MODULE_NAME, sModuleName, i);  
    doc.PutTableCell(sModuleName.c_str(), 32, false);      

    tstring sSymLoadStatus;
    result = get_prop(reader_, CRP_TBL_MDMP_MODULES, CRP_COL_MODULE_SYM_LOAD_STATUS, sSymLoadStatus, i);  
    doc.PutTableCell(sSymLoadStatus.c_str(), 32, false);      

    tstring sLoadedPDBName;
    result = get_prop(reader_, CRP_TBL_MDMP_MODULES, CRP_COL_MODULE_LOADED_PDB_NAME, sLoadedPDBName, i);  
    doc.PutTableCell(sLoadedPDBName.c_str(), 48, false);      

    tstring sLoadedImageName;
    result = get_prop(reader_, CRP_TBL_MDMP_MODULES, CRP_COL_MODULE_LOADED_IMAGE_NAME, sLoadedImageName, i);  
    doc.PutTableCell(sLoadedImageName.c_str(), 48, true);      
  }  
  doc.EndSection();

  doc.BeginSection(_T("Minidump Load Log"));
  nItemCount = get_table_row_count(reader_, CRP_TBL_MDMP_LOAD_LOG);
  for(int i=0; i<nItemCount; i++)
  {
    TCHAR szBuffer[10];
    __STPRINTF_S(szBuffer, 10, _T("%d"), i+1);
    doc.PutTableCell(szBuffer, 2, false);

    tstring sEntry;
    result = get_prop(reader_, CRP_TBL_MDMP_LOAD_LOG, CRP_COL_LOAD_LOG_ENTRY, sEntry, i);  
    doc.PutTableCell(sEntry.c_str(), 64, true);          
  }
  doc.EndSection();

  doc.EndDocument();
}
