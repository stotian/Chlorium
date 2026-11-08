@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
if errorlevel 1 exit /b %errorlevel%
set "TASK_FAST_OUT=%~dp0..\..\build-research\resident-fast-r58-checks"
set "TASK_FAST_SDK=C:\Users\stoti\Projects\gd-timing-lab\deps\geode-sdk\loader\include"
if not exist "%TASK_FAST_OUT%" mkdir "%TASK_FAST_OUT%"
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 "%~dp0checks.cpp" /Fo"%TASK_FAST_OUT%\\" /Fe"%TASK_FAST_OUT%\checks.exe"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 /I"%TASK_FAST_SDK%\Geode\cocos\platform\third_party\win32\OGLES" "%~dp0gpu_checks.cpp" /Fo"%TASK_FAST_OUT%\\" /Fe"%TASK_FAST_OUT%\gpu_checks.exe" /link "%TASK_FAST_SDK%\link\win64\glew32.lib" opengl32.lib user32.lib gdi32.lib
if errorlevel 1 exit /b %errorlevel%
copy /y "C:\Users\stoti\Chlorium\test-game\glew32.dll" "%TASK_FAST_OUT%\glew32.dll" >nul
exit /b %errorlevel%
