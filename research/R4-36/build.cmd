@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_HASH_OUT=%~dp0..\..\build-research\native-opacity-hash"
if not exist "%TASK_HASH_OUT%" mkdir "%TASK_HASH_OUT%"
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 "%~dp0hash_checks.cpp" /Fo"%TASK_HASH_OUT%\\" /Fe"%TASK_HASH_OUT%\hash_checks.exe"
exit /b %errorlevel%
