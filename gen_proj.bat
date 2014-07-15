@echo OFF

call setup_env.bat
tools\gyp\gyp.bat --depth . --no-circular-check -Ibuild/common.gypi build/all.gyp