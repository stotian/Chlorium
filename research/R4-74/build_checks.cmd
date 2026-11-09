@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
if errorlevel 1 exit /b %errorlevel%
set "TASK_JOINT_OUT=%~dp0..\..\build-research\joint-root-r74"
set "TASK_JOINT_SDK=C:\Users\stoti\Projects\gd-timing-lab\deps\geode-sdk\loader\include"
if not exist "%TASK_JOINT_OUT%" mkdir "%TASK_JOINT_OUT%"
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 /I"%TASK_JOINT_SDK%\Geode\cocos\platform\third_party\win32\OGLES" "%~dp0joint_replay.cpp" "%~dp0..\..\build-research\mixed-program-r71\rotation_oracle.obj" /Fo"%TASK_JOINT_OUT%\joint_replay.obj" /Fe"%TASK_JOINT_OUT%\joint_replay.exe" /link "%TASK_JOINT_SDK%\link\win64\glew32.lib" opengl32.lib user32.lib gdi32.lib
if errorlevel 1 exit /b %errorlevel%
copy /y "C:\Users\stoti\Chlorium\test-game\glew32.dll" "%TASK_JOINT_OUT%\glew32.dll" >nul
exit /b %errorlevel%
