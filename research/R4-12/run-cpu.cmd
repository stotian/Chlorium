@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_IR_OUT=%~dp0..\..\build-research\causal-world"
set "TASK_IR_DEFINE="
set "TASK_IR_SUFFIX="
if /I "%~1"=="--order-mutant" set "TASK_IR_DEFINE=/DCHLORIUM_PRIVATE_CAUSAL_ORDER_MUTANT=1"
if /I "%~1"=="--order-mutant" set "TASK_IR_SUFFIX=-order-mutant"
if /I "%~1"=="--barrier-mutant" set "TASK_IR_DEFINE=/DCHLORIUM_PRIVATE_CAUSAL_BARRIER_MUTANT=1"
if /I "%~1"=="--barrier-mutant" set "TASK_IR_SUFFIX=-barrier-mutant"
if /I "%~1"=="--simd-mutant" set "TASK_IR_DEFINE=/DCHLORIUM_PRIVATE_CAUSAL_SIMD_MUTANT=1"
if /I "%~1"=="--simd-mutant" set "TASK_IR_SUFFIX=-simd-mutant"
if not exist "%TASK_IR_OUT%" mkdir "%TASK_IR_OUT%"
cl /nologo /std:c++20 /EHsc /O2 /fp:strict /W4 %TASK_IR_DEFINE% "%~dp0causal_world_checks.cpp" /Fo"%TASK_IR_OUT%\checks%TASK_IR_SUFFIX%.obj" /Fe"%TASK_IR_OUT%\checks%TASK_IR_SUFFIX%.exe"
if errorlevel 1 exit /b %errorlevel%
"%TASK_IR_OUT%\checks%TASK_IR_SUFFIX%.exe" "%TASK_IR_OUT%\timings%TASK_IR_SUFFIX%.csv"
exit /b %errorlevel%
