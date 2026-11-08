@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
if errorlevel 1 exit /b %errorlevel%
set "TASK_DEFER_OUT=%~dp0..\..\build-research\native-upload-deferral-r59-checks"
set "TASK_DEFER_SDK=C:\Users\stoti\Projects\gd-timing-lab\deps\geode-sdk\loader\include"
if not exist "%TASK_DEFER_OUT%" mkdir "%TASK_DEFER_OUT%"
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 /I"%TASK_DEFER_SDK%\Geode\cocos\platform\third_party\win32\OGLES" "%~dp0gpu_checks.cpp" /Fo"%TASK_DEFER_OUT%\\" /Fe"%TASK_DEFER_OUT%\gpu_checks.exe" /link "%TASK_DEFER_SDK%\link\win64\glew32.lib" opengl32.lib user32.lib gdi32.lib
if errorlevel 1 exit /b %errorlevel%
copy /y "C:\Users\stoti\Chlorium\test-game\glew32.dll" "%TASK_DEFER_OUT%\glew32.dll" >nul
exit /b %errorlevel%
