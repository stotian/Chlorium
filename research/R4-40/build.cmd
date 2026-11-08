@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_MASKED_OUT=%~dp0..\..\build-research\masked-corners"
if not exist "%TASK_MASKED_OUT%" mkdir "%TASK_MASKED_OUT%"
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 "%~dp0masked_checks.cpp" /Fo"%TASK_MASKED_OUT%\\" /Fe"%TASK_MASKED_OUT%\masked_checks.exe"
exit /b %errorlevel%
