@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
if errorlevel 1 exit /b %errorlevel%
set "TASK_ATLAS_OUT=%~dp0..\..\build-research\atlas-write-r48"
if not exist "%TASK_ATLAS_OUT%" mkdir "%TASK_ATLAS_OUT%"
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 "%~dp0atlas_write_checks.cpp" /Fo"%TASK_ATLAS_OUT%\\" /Fe"%TASK_ATLAS_OUT%\atlas_write_checks.exe"
exit /b %errorlevel%
