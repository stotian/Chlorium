@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
if errorlevel 1 exit /b %errorlevel%
set "TASK_PREDICATE_OUT=%~dp0..\..\build-research\predicate-error-r59"
set "TASK_PREDICATE_SDK=C:\Users\stoti\Projects\gd-timing-lab\deps\geode-sdk\loader\include"
if not exist "%TASK_PREDICATE_OUT%" mkdir "%TASK_PREDICATE_OUT%"
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 /I"%TASK_PREDICATE_SDK%\Geode\cocos\platform\third_party\win32\OGLES" "%~dp0predicate_error_check.cpp" /Fo"%TASK_PREDICATE_OUT%\\" /Fe"%TASK_PREDICATE_OUT%\check.exe" /link "%TASK_PREDICATE_SDK%\link\win64\glew32.lib" opengl32.lib user32.lib gdi32.lib
if errorlevel 1 exit /b %errorlevel%
copy /y "C:\Users\stoti\Chlorium\test-game\glew32.dll" "%TASK_PREDICATE_OUT%\glew32.dll" >nul
exit /b %errorlevel%
