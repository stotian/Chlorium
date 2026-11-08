@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_SPLICE_OUT=%~dp0..\..\build-research\masked-corners"
if not exist "%TASK_SPLICE_OUT%" mkdir "%TASK_SPLICE_OUT%"
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 "%~dp0splice_checks.cpp" /Fo"%TASK_SPLICE_OUT%\\" /Fe"%TASK_SPLICE_OUT%\splice_checks.exe"
exit /b %errorlevel%
