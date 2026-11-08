@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_CENSUS_OUT=%~dp0..\..\build-research\opacity-invocations"
if not exist "%TASK_CENSUS_OUT%" mkdir "%TASK_CENSUS_OUT%"
cl /nologo /std:c++20 /EHsc /O2 /W4 "%~dp0execution_checks.cpp" /Fo"%TASK_CENSUS_OUT%\\" /Fe"%TASK_CENSUS_OUT%\execution_checks.exe"
if errorlevel 1 exit /b %errorlevel%
"%TASK_CENSUS_OUT%\execution_checks.exe"
exit /b %errorlevel%
