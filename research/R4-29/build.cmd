@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_SAMPLER_OUT=%~dp0..\..\build-research\process-sampler"
if not exist "%TASK_SAMPLER_OUT%" mkdir "%TASK_SAMPLER_OUT%"
cl /nologo /std:c++20 /EHsc /O2 /W4 "%~dp0process_sampler.cpp" /Fo"%TASK_SAMPLER_OUT%\\" /Fe"%TASK_SAMPLER_OUT%\process_sampler.exe" /link psapi.lib bcrypt.lib
exit /b %errorlevel%
