@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
if errorlevel 1 exit /b %errorlevel%
set "TASK_CELL_OUT=%~dp0..\..\build-research\same-cell-r70"
if not exist "%TASK_CELL_OUT%" mkdir "%TASK_CELL_OUT%"
cl /nologo /std:c++20 /EHsc /O2 /fp:strict /W4 "%~dp0checks.cpp" /Fo"%TASK_CELL_OUT%\checks.obj" /Fe"%TASK_CELL_OUT%\checks.exe" /link bcrypt.lib
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /EHsc /O2 /fp:strict /W4 "%~dp0packet_checks.cpp" /Fo"%TASK_CELL_OUT%\packet_checks.obj" /Fe"%TASK_CELL_OUT%\packet_checks.exe" /link bcrypt.lib
exit /b %errorlevel%
