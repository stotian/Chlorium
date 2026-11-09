@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
if errorlevel 1 exit /b %errorlevel%
set "TASK_TRACE_OUT=%~dp0..\..\build-research\group-trace-r63-checks"
if not exist "%TASK_TRACE_OUT%" mkdir "%TASK_TRACE_OUT%"
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 "%~dp0checks.cpp" /Fo"%TASK_TRACE_OUT%\\" /Fe"%TASK_TRACE_OUT%\checks.exe"
exit /b %errorlevel%
