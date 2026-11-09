@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
if errorlevel 1 exit /b %errorlevel%
set "TASK_PROGRAM_OUT=%~dp0..\..\build-research\group-program-r64"
set "TASK_PROGRAM_SDK=C:\Users\stoti\Projects\gd-timing-lab\deps\geode-sdk\loader\include"
if not exist "%TASK_PROGRAM_OUT%" mkdir "%TASK_PROGRAM_OUT%"
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 "%~dp0checks.cpp" /Fo"%TASK_PROGRAM_OUT%\\" /Fe"%TASK_PROGRAM_OUT%\checks.exe"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 /I"%TASK_PROGRAM_SDK%\Geode\cocos\platform\third_party\win32\OGLES" "%~dp0gpu_checks.cpp" /Fo"%TASK_PROGRAM_OUT%\\" /Fe"%TASK_PROGRAM_OUT%\gpu_checks.exe" /link "%TASK_PROGRAM_SDK%\link\win64\glew32.lib" opengl32.lib user32.lib gdi32.lib
if errorlevel 1 exit /b %errorlevel%
copy /y "C:\Users\stoti\Chlorium\test-game\glew32.dll" "%TASK_PROGRAM_OUT%\glew32.dll" >nul
exit /b %errorlevel%
