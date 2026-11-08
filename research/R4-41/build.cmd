@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_OCCLUSION_OUT=%~dp0..\..\build-research\opaque-occlusion"
set "TASK_OCCLUSION_SDK=C:\Users\stoti\Projects\gd-timing-lab\deps\geode-sdk\loader\include"
if not exist "%TASK_OCCLUSION_OUT%" mkdir "%TASK_OCCLUSION_OUT%"
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 "%~dp0mask_checks.cpp" /Fo"%TASK_OCCLUSION_OUT%\\" /Fe"%TASK_OCCLUSION_OUT%\mask_checks.exe"
if errorlevel 1 exit /b %errorlevel%
"%TASK_OCCLUSION_OUT%\mask_checks.exe"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 /I"%TASK_OCCLUSION_SDK%\Geode\cocos\platform\third_party\win32\OGLES" "%~dp0occlusion_checks.cpp" /Fo"%TASK_OCCLUSION_OUT%\\" /Fe"%TASK_OCCLUSION_OUT%\occlusion_checks.exe" /link "%TASK_OCCLUSION_SDK%\link\win64\glew32.lib" opengl32.lib user32.lib gdi32.lib
exit /b %errorlevel%
