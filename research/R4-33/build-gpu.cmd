@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_CORNER_OUT=%~dp0..\..\build-research\sprite-corner-oracle"
set "TASK_CORNER_SDK=C:\Users\stoti\Projects\gd-timing-lab\deps\geode-sdk\loader\include"
if not exist "%TASK_CORNER_OUT%" mkdir "%TASK_CORNER_OUT%"
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 /I"%TASK_CORNER_SDK%\Geode\cocos\platform\third_party\win32\OGLES" "%~dp0gpu_corner_checks.cpp" /Fo"%TASK_CORNER_OUT%\\" /Fe"%TASK_CORNER_OUT%\gpu_corner_checks.exe" /link "%TASK_CORNER_SDK%\link\win64\glew32.lib" opengl32.lib user32.lib gdi32.lib
exit /b %errorlevel%
