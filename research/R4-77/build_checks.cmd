@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
if errorlevel 1 exit /b %errorlevel%
set "TASK_CLASS_OUT=%~dp0..\..\build-research\retained-geometry-r77"
set "TASK_CLASS_SDK=C:\Users\stoti\Projects\gd-timing-lab\deps\geode-sdk\loader\include"
if not exist "%TASK_CLASS_OUT%" mkdir "%TASK_CLASS_OUT%"
cl /nologo /std:c++20 /EHsc /O2 /W4 "%~dp0checks.cpp" /Fo"%TASK_CLASS_OUT%\checks.obj" /Fe"%TASK_CLASS_OUT%\checks.exe"
if errorlevel 1 exit /b %errorlevel%
"%TASK_CLASS_OUT%\checks.exe"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 /DCHLORIUM_REAL_GEOMETRY_R75=1 /DCHLORIUM_CLASS_GEOMETRY_R77=1 /I"%TASK_CLASS_SDK%\Geode\cocos\platform\third_party\win32\OGLES" "%~dp0..\R4-74\joint_replay.cpp" "%~dp0..\..\build-research\mixed-program-r71\rotation_oracle.obj" /Fo"%TASK_CLASS_OUT%\class_geometry.obj" /Fe"%TASK_CLASS_OUT%\class_geometry.exe" /link "%TASK_CLASS_SDK%\link\win64\glew32.lib" opengl32.lib user32.lib gdi32.lib
if errorlevel 1 exit /b %errorlevel%
copy /y "C:\Users\stoti\Chlorium\test-game\glew32.dll" "%TASK_CLASS_OUT%\glew32.dll" >nul
exit /b %errorlevel%
