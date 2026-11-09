@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
if errorlevel 1 exit /b %errorlevel%
set "TASK_DIRECT_OUT=%~dp0..\..\build-research\direct-vertices-r65"
set "TASK_DIRECT_SDK=C:\Users\stoti\Projects\gd-timing-lab\deps\geode-sdk\loader\include"
if not exist "%TASK_DIRECT_OUT%" mkdir "%TASK_DIRECT_OUT%"
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 /I"%TASK_DIRECT_SDK%\Geode\cocos\platform\third_party\win32\OGLES" "%~dp0gpu_checks.cpp" /Fo"%TASK_DIRECT_OUT%\\" /Fe"%TASK_DIRECT_OUT%\gpu_checks.exe" /link "%TASK_DIRECT_SDK%\link\win64\glew32.lib" opengl32.lib user32.lib gdi32.lib
if errorlevel 1 exit /b %errorlevel%
copy /y "C:\Users\stoti\Chlorium\test-game\glew32.dll" "%TASK_DIRECT_OUT%\glew32.dll" >nul
exit /b %errorlevel%
