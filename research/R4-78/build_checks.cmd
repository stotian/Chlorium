@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
if errorlevel 1 exit /b %errorlevel%
set "TASK_CANON_OUT=%~dp0..\..\build-research\canonical-draw-r78"
set "TASK_CANON_SDK=C:\Users\stoti\Projects\gd-timing-lab\deps\geode-sdk\loader\include"
if not exist "%TASK_CANON_OUT%" mkdir "%TASK_CANON_OUT%"
cl /nologo /std:c++20 /EHsc /O2 /W4 "%~dp0frame_checks.cpp" /Fo"%TASK_CANON_OUT%\frame_checks.obj" /Fe"%TASK_CANON_OUT%\frame_checks.exe"
if errorlevel 1 exit /b %errorlevel%
"%TASK_CANON_OUT%\frame_checks.exe"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 /I"%TASK_CANON_SDK%\Geode\cocos\platform\third_party\win32\OGLES" "%~dp0checks.cpp" /Fo"%TASK_CANON_OUT%\checks.obj" /Fe"%TASK_CANON_OUT%\checks.exe" /link "%TASK_CANON_SDK%\link\win64\glew32.lib" opengl32.lib user32.lib gdi32.lib
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 /I"%TASK_CANON_SDK%\Geode\cocos\platform\third_party\win32\OGLES" "%~dp0runtime_checks.cpp" "%~dp0..\..\build-research\mixed-program-r71\rotation_oracle.obj" /Fo"%TASK_CANON_OUT%\runtime_checks.obj" /Fe"%TASK_CANON_OUT%\runtime_checks.exe" /link "%TASK_CANON_SDK%\link\win64\glew32.lib" opengl32.lib user32.lib gdi32.lib
if errorlevel 1 exit /b %errorlevel%
copy /y "C:\Users\stoti\Chlorium\test-game\glew32.dll" "%TASK_CANON_OUT%\glew32.dll" >nul
exit /b %errorlevel%
