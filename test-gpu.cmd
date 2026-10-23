@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
set "GEODE_SDK=C:\Users\stoti\Projects\gd-timing-lab\deps\geode-sdk"
set "TASK_CMAKE=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
"%TASK_CMAKE%" --build "%~dp0build" --target chlorium_gpu_checks --parallel 3
if errorlevel 1 exit /b %errorlevel%
set "PATH=C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash;%PATH%"
"%~dp0build\chlorium_gpu_checks.exe" "%~dp0."
exit /b %errorlevel%
