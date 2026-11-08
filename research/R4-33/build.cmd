@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_CORNER_OUT=%~dp0..\..\build-research\sprite-corner-oracle"
if not exist "%TASK_CORNER_OUT%" mkdir "%TASK_CORNER_OUT%"
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 "%~dp0native_corner_checks.cpp" /Fo"%TASK_CORNER_OUT%\\" /Fe"%TASK_CORNER_OUT%\native_corner_checks.exe"
exit /b %errorlevel%
