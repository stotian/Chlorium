@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_IDENTITY_OUT=%~dp0..\..\build-research\identity-node"
set "TASK_IDENTITY_SOURCE=%~dp0node_checks.cpp"
set "TASK_IDENTITY_FLAGS="
if "%~1"=="--skip-linear-guard-mutant" (
    set "TASK_IDENTITY_OUT=%TASK_IDENTITY_OUT%-mutant"
    set "TASK_IDENTITY_FLAGS=/DCHLORIUM_IDENTITY_NODE_SKIP_LINEAR_GUARD_MUTANT=1"
    shift
)
if not exist "%TASK_IDENTITY_OUT%" mkdir "%TASK_IDENTITY_OUT%"
cl /nologo /std:c++20 /EHsc /O2 /fp:strict /W4 %TASK_IDENTITY_FLAGS% "%TASK_IDENTITY_SOURCE%" /Fo"%TASK_IDENTITY_OUT%\\" /Fe"%TASK_IDENTITY_OUT%\node_checks.exe"
if errorlevel 1 exit /b %errorlevel%
"%TASK_IDENTITY_OUT%\node_checks.exe" %1 %2
exit /b %errorlevel%
