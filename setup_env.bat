@echo OFF

set PATH=%~dp0tools\python27;%PATH%
set PYTHONPATH=%PYTHONPATH%;%~dp0tools\grit;%~dp0chrome\tools\build
set GYP_GENERATORS=msvs
set GYP_MSVS_VERSION=2010
set GYP_DEFINES=branding=Chromium buildtype=Dev component=shared_library
rem set GYP_DEFINES=branding=Chromium buildtype=Official component=static_library

