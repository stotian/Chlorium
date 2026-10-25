@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_GL_SDK=C:\Users\stoti\Projects\gd-timing-lab\deps\geode-sdk\loader\include"
set "TASK_GL_OUT=%~dp0..\..\build-research\readback-gpu"
set "TASK_GL_SOURCE=%~dp0..\..\tests\visual_gpu_checks.cpp"
if /I "%~1"=="--mutant" python "%~dp0prepare_readback_mutant.py"
if errorlevel 1 exit /b %errorlevel%
if /I "%~1"=="--mutant" set "TASK_GL_OUT=%~dp0..\..\build-research\readback-gpu-mutant"
if /I "%~1"=="--mutant" set "TASK_GL_SOURCE=%TASK_GL_OUT%\visual_gpu_checks.cpp"
if not exist "%TASK_GL_OUT%" mkdir "%TASK_GL_OUT%"
copy /Y "C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash\glew32.dll" "%TASK_GL_OUT%\glew32.dll" >nul
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /EHsc /O2 /MD /W3 /utf-8 /DFMT_HEADER_ONLY /I"%~dp0..\..\src" /I"%TASK_GL_SDK%\Geode\cocos\platform\third_party\win32\OGLES" /I"C:\Users\stoti\Projects\gd-timing-lab\deps\cpm-cache\fmt\71b2\include" "%TASK_GL_SOURCE%" /Fo"%TASK_GL_OUT%\visual_checks.obj" /Fe"%TASK_GL_OUT%\visual_checks.exe" /link opengl32.lib user32.lib gdi32.lib "%TASK_GL_SDK%\link\win64\glew32.lib"
if errorlevel 1 exit /b %errorlevel%
"%TASK_GL_OUT%\visual_checks.exe" "%TASK_GL_OUT%\artifacts"
exit /b %errorlevel%
