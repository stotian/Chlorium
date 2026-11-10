@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
if errorlevel 1 exit /b %errorlevel%
set "TASK_FOLLOW_OUT=%~dp0..\..\build-research\follow-program-r76"
set "TASK_FOLLOW_SDK=C:\Users\stoti\Projects\gd-timing-lab\deps\geode-sdk\loader\include"
"C:\Program Files\Python314\python.exe" "%~dp0prepare_oracle.py"
if errorlevel 1 exit /b %errorlevel%
ml64 /nologo /c /I"%TASK_FOLLOW_OUT%" /Fo"%TASK_FOLLOW_OUT%\follow_oracle.obj" "%~dp0follow_oracle.asm"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 /I"%TASK_FOLLOW_SDK%\Geode\cocos\platform\third_party\win32\OGLES" "%~dp0checks.cpp" "%TASK_FOLLOW_OUT%\follow_oracle.obj" "%~dp0..\..\build-research\mixed-program-r71\rotation_oracle.obj" /Fo"%TASK_FOLLOW_OUT%\checks.obj" /Fe"%TASK_FOLLOW_OUT%\checks.exe" /link "%TASK_FOLLOW_SDK%\link\win64\glew32.lib" opengl32.lib user32.lib gdi32.lib
if errorlevel 1 exit /b %errorlevel%
ml64 /nologo /c /Fo"%TASK_FOLLOW_OUT%\FollowGateway.obj" "%~dp0FollowGateway.asm"
if errorlevel 1 exit /b %errorlevel%
ml64 /nologo /c /Fo"%TASK_FOLLOW_OUT%\gateway_driver.obj" "%~dp0gateway_driver.asm"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 /I"%TASK_FOLLOW_SDK%\Geode\cocos\platform\third_party\win32\OGLES" "%~dp0gateway_checks.cpp" "%TASK_FOLLOW_OUT%\FollowGateway.obj" "%TASK_FOLLOW_OUT%\gateway_driver.obj" /Fo"%TASK_FOLLOW_OUT%\gateway_checks.obj" /Fe"%TASK_FOLLOW_OUT%\gateway_checks.exe" /link "%TASK_FOLLOW_SDK%\link\win64\glew32.lib" opengl32.lib user32.lib gdi32.lib
if errorlevel 1 exit /b %errorlevel%
copy /y "C:\Users\stoti\Chlorium\test-game\glew32.dll" "%TASK_FOLLOW_OUT%\glew32.dll" >nul
exit /b %errorlevel%
