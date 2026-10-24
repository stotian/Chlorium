@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "TASK_CMAKE=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
"%TASK_CMAKE%" --build "%~dp0build" --target chlorium_visual_checks --parallel 2
if errorlevel 1 exit /b %errorlevel%
set "PATH=C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash;%PATH%"
"%~dp0build\chlorium_visual_checks.exe" "%~dp0build-tests\visual-readback-gpu"
exit /b %errorlevel%
