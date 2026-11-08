@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_WATCH_OUT=%~dp0..\..\build-research\quad-read-watch"
if not exist "%TASK_WATCH_OUT%" mkdir "%TASK_WATCH_OUT%"
cl /nologo /std:c++20 /EHsc /O2 /W4 "%~dp0watch_checks.cpp" /Fo"%TASK_WATCH_OUT%\\" /Fe"%TASK_WATCH_OUT%\watch_checks.exe"
if errorlevel 1 exit /b %errorlevel%
"%TASK_WATCH_OUT%\watch_checks.exe"
exit /b %errorlevel%
