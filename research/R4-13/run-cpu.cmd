@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_SETTLE_OUT=%~dp0..\..\build-research\causal-settlement"
if not exist "%TASK_SETTLE_OUT%" mkdir "%TASK_SETTLE_OUT%"
cl /nologo /std:c++20 /EHsc /O2 /fp:strict /W4 "%~dp0settlement_bench.cpp" /Fo"%TASK_SETTLE_OUT%\bench.obj" /Fe"%TASK_SETTLE_OUT%\bench.exe"
if errorlevel 1 exit /b %errorlevel%
"%TASK_SETTLE_OUT%\bench.exe" "%TASK_SETTLE_OUT%\timings.csv"
exit /b %errorlevel%
