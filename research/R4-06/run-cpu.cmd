@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_ALPHA_DEFINE="
set "TASK_ALPHA_SUFFIX="
if /I "%~1"=="--mutant" set "TASK_ALPHA_DEFINE=/DCHLORIUM_PRIVATE_DROP_ALPHA_DEPENDENCY=1"
if /I "%~1"=="--mutant" set "TASK_ALPHA_SUFFIX=_mutant"
if not exist "%~dp0..\..\build-research" mkdir "%~dp0..\..\build-research"
cl /nologo /std:c++20 /EHsc /O2 /fp:strict /W4 %TASK_ALPHA_DEFINE% /I"%~dp0." "%~dp0alpha_checks.cpp" /Fo"%~dp0..\..\build-research\alpha_checks%TASK_ALPHA_SUFFIX%.obj" /Fe"%~dp0..\..\build-research\alpha_checks%TASK_ALPHA_SUFFIX%.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0..\..\build-research\alpha_checks%TASK_ALPHA_SUFFIX%.exe" "%~dp0timings.csv"
exit /b %errorlevel%
