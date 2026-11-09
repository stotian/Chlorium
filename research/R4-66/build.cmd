@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
if errorlevel 1 exit /b %errorlevel%
set "TASK_ADAPTIVE_OUT=%~dp0..\..\build-research\adaptive-vertices-r66"
set "TASK_ADAPTIVE_SDK=C:\Users\stoti\Projects\gd-timing-lab\deps\geode-sdk\loader\include"
if not exist "%TASK_ADAPTIVE_OUT%" mkdir "%TASK_ADAPTIVE_OUT%"
cl /nologo /std:c++20 /EHsc /O2 /W4 "%~dp0checks.cpp" /Fo"%TASK_ADAPTIVE_OUT%\\" /Fe"%TASK_ADAPTIVE_OUT%\checks.exe"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 /DCHLORIUM_ADAPTIVE_VERTEX_PAGES_R66=1 /I"%TASK_ADAPTIVE_SDK%\Geode\cocos\platform\third_party\win32\OGLES" "%~dp0..\R4-65\gpu_checks.cpp" /Fo"%TASK_ADAPTIVE_OUT%\\" /Fe"%TASK_ADAPTIVE_OUT%\gpu_checks.exe" /link "%TASK_ADAPTIVE_SDK%\link\win64\glew32.lib" opengl32.lib user32.lib gdi32.lib
if errorlevel 1 exit /b %errorlevel%
copy /y "C:\Users\stoti\Chlorium\test-game\glew32.dll" "%TASK_ADAPTIVE_OUT%\glew32.dll" >nul
exit /b %errorlevel%
