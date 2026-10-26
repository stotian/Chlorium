@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_LOOP_OUT=%~dp0..\..\build-research\transform-loop"
set "TASK_LOOP_FLAGS="
set "TASK_LOOP_ARG="
if not "%~1"=="" if not "%~1"=="--benchmark" if not "%~1"=="--zero-delta-mutant" if not "%~1"=="--drop-flags-mutant" if not "%~1"=="--drop-callback-mutant" exit /b 2
if "%~1"=="--zero-delta-mutant" set "TASK_LOOP_OUT=%TASK_LOOP_OUT%-zero-mutant"
if "%~1"=="--zero-delta-mutant" set "TASK_LOOP_FLAGS=/DCHLORIUM_TRANSFORM_ZERO_DELTA_MUTANT=1"
if "%~1"=="--drop-flags-mutant" set "TASK_LOOP_OUT=%TASK_LOOP_OUT%-flags-mutant"
if "%~1"=="--drop-flags-mutant" set "TASK_LOOP_FLAGS=/DCHLORIUM_TRANSFORM_DROP_FLAGS_MUTANT=1"
if "%~1"=="--drop-callback-mutant" set "TASK_LOOP_OUT=%TASK_LOOP_OUT%-callback-mutant"
if "%~1"=="--drop-callback-mutant" set "TASK_LOOP_FLAGS=/DCHLORIUM_TRANSFORM_DROP_CALLBACK_MUTANT=1"
if "%~1"=="--benchmark" set "TASK_LOOP_ARG=%TASK_LOOP_OUT%\timings.csv"
if not exist "%TASK_LOOP_OUT%" mkdir "%TASK_LOOP_OUT%"
cl /nologo /std:c++20 /EHsc /O2 /fp:strict /W4 %TASK_LOOP_FLAGS% "%~dp0loop_checks.cpp" /Fo"%TASK_LOOP_OUT%\\" /Fe"%TASK_LOOP_OUT%\loop_checks.exe"
if errorlevel 1 exit /b %errorlevel%
"%TASK_LOOP_OUT%\loop_checks.exe" %TASK_LOOP_ARG%
exit /b %errorlevel%
