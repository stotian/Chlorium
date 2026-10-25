@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_EPOCH_SDK=C:\Users\stoti\Projects\gd-timing-lab\deps\geode-sdk\loader\include"
set "TASK_EPOCH_OUT=%~dp0..\..\build-research\epoch-gpu"
if not exist "%TASK_EPOCH_OUT%" mkdir "%TASK_EPOCH_OUT%"
copy /Y "C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash\glew32.dll" "%TASK_EPOCH_OUT%\glew32.dll" >nul
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /EHsc /O2 /MD /W4 /utf-8 /I"%TASK_EPOCH_SDK%" /I"%~dp0..\..\tests\fakes" /I"%TASK_EPOCH_SDK%\Geode\cocos\platform\third_party\win32\OGLES" "%~dp0epoch_gpu_checks.cpp" /Fo"%TASK_EPOCH_OUT%\epoch_checks.obj" /Fe"%TASK_EPOCH_OUT%\epoch_checks.exe" /link opengl32.lib user32.lib gdi32.lib "%TASK_EPOCH_SDK%\link\win64\glew32.lib"
if errorlevel 1 exit /b %errorlevel%
"%TASK_EPOCH_OUT%\epoch_checks.exe"
exit /b %errorlevel%
