@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_PHASE_OUT=%~dp0..\..\build-research\exact-phase"
set "TASK_PHASE_FLAGS="
if not "%~1"=="" if not "%~1"=="--unsafe-mutant" if not "%~1"=="--checks-only" exit /b 2
if "%~1"=="--unsafe-mutant" set "TASK_PHASE_OUT=%~dp0..\..\build-research\exact-phase-mutant"
if "%~1"=="--unsafe-mutant" set "TASK_PHASE_FLAGS=/DCHLORIUM_PRIVATE_PHASE_UNSAFE_MUTANT=1"
if not exist "%TASK_PHASE_OUT%" mkdir "%TASK_PHASE_OUT%"
cl /nologo /std:c++20 /EHsc /O2 /fp:strict /W4 %TASK_PHASE_FLAGS% "%~dp0phase_translation_checks.cpp" /Fo"%TASK_PHASE_OUT%\\" /Fe"%TASK_PHASE_OUT%\phase_checks.exe"
if errorlevel 1 exit /b %errorlevel%
if "%~1"=="--checks-only" goto checks
"%TASK_PHASE_OUT%\phase_checks.exe" "%TASK_PHASE_OUT%\timings.csv"
exit /b %errorlevel%
:checks
"%TASK_PHASE_OUT%\phase_checks.exe"
exit /b %errorlevel%
