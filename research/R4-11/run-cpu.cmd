@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_POSE_OUT=%~dp0..\..\build-research\pose-program"
set "TASK_POSE_DEFINE="
set "TASK_POSE_SUFFIX="
if /I "%~1"=="--mutant" set "TASK_POSE_DEFINE=/DCHLORIUM_PRIVATE_POSE_MUTANT=1"
if /I "%~1"=="--mutant" set "TASK_POSE_SUFFIX=-mutant"
if not exist "%TASK_POSE_OUT%" mkdir "%TASK_POSE_OUT%"
ml64 /nologo /c /Fo"%TASK_POSE_OUT%\oracle.obj" "%~dp0PoseOracle.asm"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /EHsc /O2 /fp:strict /W4 %TASK_POSE_DEFINE% "%~dp0pose_program_checks.cpp" "%TASK_POSE_OUT%\oracle.obj" /Fo"%TASK_POSE_OUT%\checks%TASK_POSE_SUFFIX%.obj" /Fe"%TASK_POSE_OUT%\checks%TASK_POSE_SUFFIX%.exe"
if errorlevel 1 exit /b %errorlevel%
"%TASK_POSE_OUT%\checks%TASK_POSE_SUFFIX%.exe" "C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash\GeometryDash.exe" "C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash\libcocos2d.dll" "%TASK_POSE_OUT%\timings%TASK_POSE_SUFFIX%.csv"
exit /b %errorlevel%
