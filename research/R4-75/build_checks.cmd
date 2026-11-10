@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
if errorlevel 1 exit /b %errorlevel%
set "TASK_REAL_OUT=%~dp0..\..\build-research\real-root-geometry-r75"
set "TASK_REAL_SDK=C:\Users\stoti\Projects\gd-timing-lab\deps\geode-sdk\loader\include"
if not exist "%TASK_REAL_OUT%" mkdir "%TASK_REAL_OUT%"
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 /DCHLORIUM_REAL_GEOMETRY_R75=1 /I"%TASK_REAL_SDK%\Geode\cocos\platform\third_party\win32\OGLES" "%~dp0..\R4-74\joint_replay.cpp" "%~dp0..\..\build-research\mixed-program-r71\rotation_oracle.obj" /Fo"%TASK_REAL_OUT%\geometry_checks.obj" /Fe"%TASK_REAL_OUT%\geometry_checks.exe" /link "%TASK_REAL_SDK%\link\win64\glew32.lib" opengl32.lib user32.lib gdi32.lib
if errorlevel 1 exit /b %errorlevel%
copy /y "C:\Users\stoti\Chlorium\test-game\glew32.dll" "%TASK_REAL_OUT%\glew32.dll" >nul
exit /b %errorlevel%
