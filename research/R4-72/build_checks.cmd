@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
if errorlevel 1 exit /b %errorlevel%
set "TASK_TRACE_OUT=%~dp0..\..\build-research\rotation-trace-r72"
set "TASK_TRACE_SDK=C:\Users\stoti\Projects\gd-timing-lab\deps\geode-sdk\loader\include"
if not exist "%TASK_TRACE_OUT%" mkdir "%TASK_TRACE_OUT%"
ml64 /nologo /c /Fo"%TASK_TRACE_OUT%\PointGateway.obj" "%~dp0PointGateway.asm"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 /I"%TASK_TRACE_SDK%\Geode\cocos\platform\third_party\win32\OGLES" "%~dp0gateway_checks.cpp" "%TASK_TRACE_OUT%\PointGateway.obj" "%~dp0..\..\build-research\mixed-program-r71\rotation_oracle.obj" /Fo"%TASK_TRACE_OUT%\gateway_checks.obj" /Fe"%TASK_TRACE_OUT%\gateway_checks.exe" /link "%TASK_TRACE_SDK%\link\win64\glew32.lib" opengl32.lib user32.lib gdi32.lib
if errorlevel 1 exit /b %errorlevel%
copy /y "C:\Users\stoti\Chlorium\test-game\glew32.dll" "%TASK_TRACE_OUT%\glew32.dll" >nul
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 /I"%TASK_TRACE_SDK%\Geode\cocos\platform\third_party\win32\OGLES" "%~dp0replay_checks.cpp" "%~dp0..\..\build-research\mixed-program-r71\rotation_oracle.obj" /Fo"%TASK_TRACE_OUT%\replay_checks.obj" /Fe"%TASK_TRACE_OUT%\replay_checks.exe" /link "%TASK_TRACE_SDK%\link\win64\glew32.lib" opengl32.lib user32.lib gdi32.lib
exit /b %errorlevel%
