@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_INPUT_OUT=%~dp0build-tests\system-input-guard"
if not exist "%TASK_INPUT_OUT%" mkdir "%TASK_INPUT_OUT%"
if not exist "%TASK_INPUT_OUT%\foreign" mkdir "%TASK_INPUT_OUT%\foreign"
cl /nologo /std:c++20 /EHsc /O2 /W4 /DCHLORIUM_TEST_FOREIGN_XINPUT=1 /LD "%~dp0tests\system_input_guard_checks.cpp" /Fo"%TASK_INPUT_OUT%\foreign\\" /Fe"%TASK_INPUT_OUT%\foreign\XINPUT1_4.dll" /link /IMPLIB:"%TASK_INPUT_OUT%\foreign\fixture.lib"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /EHsc /O2 /W4 "%~dp0tests\system_input_guard_checks.cpp" /Fo"%TASK_INPUT_OUT%\\" /Fe"%TASK_INPUT_OUT%\input_guard_checks.exe"
if errorlevel 1 exit /b %errorlevel%
"%TASK_INPUT_OUT%\input_guard_checks.exe" --reject-foreign "%TASK_INPUT_OUT%\foreign\XINPUT1_4.dll"
if errorlevel 1 exit /b %errorlevel%
"%TASK_INPUT_OUT%\input_guard_checks.exe" --load-cocos "C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash\libcocos2d.dll"
exit /b %errorlevel%
