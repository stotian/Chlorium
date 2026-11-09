@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
if errorlevel 1 exit /b %errorlevel%
"C:\Program Files\Python314\python.exe" "%~dp0prepare_oracle.py"
if errorlevel 1 exit /b %errorlevel%
set "TASK_MIXED_OUT=%~dp0..\..\build-research\mixed-program-r71"
set "TASK_MIXED_SDK=C:\Users\stoti\Projects\gd-timing-lab\deps\geode-sdk\loader\include"
ml64 /nologo /c /I"%TASK_MIXED_OUT%" /Fo"%TASK_MIXED_OUT%\rotation_oracle.obj" "%~dp0rotation_oracle.asm"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 /I"%TASK_MIXED_SDK%\Geode\cocos\platform\third_party\win32\OGLES" "%~dp0checks.cpp" "%TASK_MIXED_OUT%\rotation_oracle.obj" /Fo"%TASK_MIXED_OUT%\checks.obj" /Fe"%TASK_MIXED_OUT%\checks.exe" /link "%TASK_MIXED_SDK%\link\win64\glew32.lib" opengl32.lib user32.lib gdi32.lib
if errorlevel 1 exit /b %errorlevel%
copy /y "C:\Users\stoti\Chlorium\test-game\glew32.dll" "%TASK_MIXED_OUT%\glew32.dll" >nul
exit /b %errorlevel%
