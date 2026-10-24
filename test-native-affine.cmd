@echo off
setlocal
call "C:\Users\stoti\Projects\gd-timing-lab\toolchain-env.cmd"
cl /nologo /std:c++20 /O2 /fp:strict /EHsc /I src tests\native_affine_checks.cpp /Fe:build\native_affine_checks.exe /Fo:build\native_affine_checks.obj
if errorlevel 1 exit /b %errorlevel%
build\native_affine_checks.exe "C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash\libcocos2d.dll"
exit /b %errorlevel%
