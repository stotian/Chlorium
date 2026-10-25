@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_AREA_OUT=%~dp0..\..\build-research\area-batch"
set "TASK_AREA_DEFINE="
set "TASK_AREA_SUFFIX="
if /I "%~1"=="--mutant" set "TASK_AREA_DEFINE=/DCHLORIUM_PRIVATE_AREA_BATCH_MUTANT=1"
if /I "%~1"=="--mutant" set "TASK_AREA_SUFFIX=-mutant"
if /I "%~1"=="--hazard-mutant" set "TASK_AREA_DEFINE=/DCHLORIUM_PRIVATE_AREA_HAZARD_MUTANT=1"
if /I "%~1"=="--hazard-mutant" set "TASK_AREA_SUFFIX=-hazard-mutant"
if not exist "%TASK_AREA_OUT%" mkdir "%TASK_AREA_OUT%"
cl /nologo /std:c++20 /EHsc /O2 /fp:strict /W4 %TASK_AREA_DEFINE% /I"%~dp0..\..\src" "%~dp0area_batch_checks.cpp" /Fo"%TASK_AREA_OUT%\area_batch%TASK_AREA_SUFFIX%.obj" /Fe"%TASK_AREA_OUT%\area_batch%TASK_AREA_SUFFIX%.exe"
if errorlevel 1 exit /b %errorlevel%
"%TASK_AREA_OUT%\area_batch%TASK_AREA_SUFFIX%.exe" "C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash\GeometryDash.exe" "%TASK_AREA_OUT%\timings%TASK_AREA_SUFFIX%.csv"
exit /b %errorlevel%
