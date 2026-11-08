@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_PACKED_OUT=%~dp0..\..\build-research\packed-corners"
if not exist "%TASK_PACKED_OUT%" mkdir "%TASK_PACKED_OUT%"
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 "%~dp0packed_checks.cpp" /Fo"%TASK_PACKED_OUT%\\" /Fe"%TASK_PACKED_OUT%\packed_checks.exe"
exit /b %errorlevel%
