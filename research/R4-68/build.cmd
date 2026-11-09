@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
if errorlevel 1 exit /b %errorlevel%
set "TASK_MIRROR_OUT=%~dp0..\..\build-research\packed-mirror-r68"
set "TASK_MIRROR_SDK=C:\Users\stoti\Projects\gd-timing-lab\deps\geode-sdk\loader\include"
if not exist "%TASK_MIRROR_OUT%" mkdir "%TASK_MIRROR_OUT%"
cl /nologo /std:c++20 /EHsc /O2 /fp:strict /W4 "%~dp0pure_checks.cpp" /Fo"%TASK_MIRROR_OUT%\\" /Fe"%TASK_MIRROR_OUT%\pure_checks.exe"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 /I"%TASK_MIRROR_SDK%\Geode\cocos\platform\third_party\win32\OGLES" "%~dp0checks.cpp" /Fo"%TASK_MIRROR_OUT%\\" /Fe"%TASK_MIRROR_OUT%\checks.exe" /link "%TASK_MIRROR_SDK%\link\win64\glew32.lib" opengl32.lib user32.lib gdi32.lib
if errorlevel 1 exit /b %errorlevel%
copy /y "C:\Users\stoti\Chlorium\test-game\glew32.dll" "%TASK_MIRROR_OUT%\glew32.dll" >nul
exit /b %errorlevel%
