@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_TAPE_OUT=%~dp0..\..\build-research\gpu-group-tape"
set "TASK_TAPE_SDK=C:\Users\stoti\Projects\gd-timing-lab\deps\geode-sdk\loader\include"
if not exist "%TASK_TAPE_OUT%" mkdir "%TASK_TAPE_OUT%"
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 "%~dp0census_checks.cpp" /Fo"%TASK_TAPE_OUT%\\" /Fe"%TASK_TAPE_OUT%\census_checks.exe"
if errorlevel 1 exit /b %errorlevel%
"%TASK_TAPE_OUT%\census_checks.exe"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 /I"%TASK_TAPE_SDK%\Geode\cocos\platform\third_party\win32\OGLES" "%~dp0movement_checks.cpp" /Fo"%TASK_TAPE_OUT%\\" /Fe"%TASK_TAPE_OUT%\movement_checks.exe" /link "%TASK_TAPE_SDK%\link\win64\glew32.lib" opengl32.lib user32.lib gdi32.lib
exit /b %errorlevel%
