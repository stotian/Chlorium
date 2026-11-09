@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
if errorlevel 1 exit /b %errorlevel%
set "TASK_COPY_OUT=%~dp0..\..\build-research\canonical-copy-r73"
set "TASK_COPY_SDK=C:\Users\stoti\Projects\gd-timing-lab\deps\geode-sdk\loader\include"
if not exist "%TASK_COPY_OUT%" mkdir "%TASK_COPY_OUT%"
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 /DCHLORIUM_CANONICAL_GPU_COPY_R73=1 /I"%TASK_COPY_SDK%\Geode\cocos\platform\third_party\win32\OGLES" "%~dp0..\R4-65\gpu_checks.cpp" /Fo"%TASK_COPY_OUT%\gpu_checks.obj" /Fe"%TASK_COPY_OUT%\gpu_checks.exe" /link "%TASK_COPY_SDK%\link\win64\glew32.lib" opengl32.lib user32.lib gdi32.lib
if errorlevel 1 exit /b %errorlevel%
copy /y "C:\Users\stoti\Chlorium\test-game\glew32.dll" "%TASK_COPY_OUT%\glew32.dll" >nul
exit /b %errorlevel%
