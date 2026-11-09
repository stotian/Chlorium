@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
if errorlevel 1 exit /b %errorlevel%
set "TASK_WATCH_OUT=%~dp0..\..\build-research\native-field-watch-r69"
if not exist "%TASK_WATCH_OUT%" mkdir "%TASK_WATCH_OUT%"
cl /nologo /std:c++20 /EHsc /O2 /fp:strict /W4 "%~dp0watch_checks.cpp" /Fo"%TASK_WATCH_OUT%\\" /Fe"%TASK_WATCH_OUT%\watch_checks.exe"
exit /b %errorlevel%
