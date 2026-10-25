@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
if not exist "%~dp0..\..\build-research" mkdir "%~dp0..\..\build-research"
cl /nologo /std:c++20 /EHsc /O2 /fp:strict /W4 /I"%~dp0." "%~dp0area_trace_checks.cpp" /Fo"%~dp0..\..\build-research\area_trace_checks.obj" /Fe"%~dp0..\..\build-research\area_trace_checks.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0..\..\build-research\area_trace_checks.exe"
exit /b %errorlevel%
