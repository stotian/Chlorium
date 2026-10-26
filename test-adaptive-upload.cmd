@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_UPLOAD_DEFINE="
set "TASK_UPLOAD_SUFFIX="
if /I "%~1"=="--mutant" set "TASK_UPLOAD_DEFINE=/DCHLORIUM_PRIVATE_UPLOAD_MUTANT=1"
if /I "%~1"=="--mutant" set "TASK_UPLOAD_SUFFIX=-mutant"
if not exist "%~dp0build-tests" mkdir "%~dp0build-tests"
cl /nologo /std:c++20 /EHsc /O2 /W4 %TASK_UPLOAD_DEFINE% /I"%~dp0src" "%~dp0tests\adaptive_upload_checks.cpp" /Fo"%~dp0build-tests\adaptive_upload%TASK_UPLOAD_SUFFIX%.obj" /Fe"%~dp0build-tests\adaptive_upload%TASK_UPLOAD_SUFFIX%.exe"
if errorlevel 1 exit /b %errorlevel%
"%~dp0build-tests\adaptive_upload%TASK_UPLOAD_SUFFIX%.exe"
exit /b %errorlevel%
