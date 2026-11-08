@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
if errorlevel 1 exit /b %errorlevel%
set "TASK_RESIDENT_OUT=%~dp0..\..\build-research\retained-scene-r47"
set "TASK_RESIDENT_SDK=C:\Users\stoti\Projects\gd-timing-lab\deps\geode-sdk\loader\include"
if not exist "%TASK_RESIDENT_OUT%" mkdir "%TASK_RESIDENT_OUT%"
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 "%~dp0scene_checks.cpp" /Fo"%TASK_RESIDENT_OUT%\\" /Fe"%TASK_RESIDENT_OUT%\scene_checks.exe"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 /I"%TASK_RESIDENT_SDK%\Geode\cocos\platform\third_party\win32\OGLES" "%~dp0gpu_scene_checks.cpp" /Fo"%TASK_RESIDENT_OUT%\\" /Fe"%TASK_RESIDENT_OUT%\gpu_scene_checks.exe" /link "%TASK_RESIDENT_SDK%\link\win64\glew32.lib" opengl32.lib user32.lib gdi32.lib
exit /b %errorlevel%
