@echo off
setlocal
if not defined GEODE_SDK_ROOT (
  echo Set GEODE_SDK_ROOT to the Geode SDK root before running this script.
  exit /b 2
)
where cl.exe >nul 2>nul
if errorlevel 1 (
  echo Run from an x64 MSVC developer environment.
  exit /b 2
)
set "OUT=%~dp0..\..\build-research\canonical-draw-r78"
set "SDK_INCLUDE=%GEODE_SDK_ROOT%\loader\include"
if not exist "%SDK_INCLUDE%\Geode\cocos" set "SDK_INCLUDE=%GEODE_SDK_ROOT%"
set "SDK_LINK=%SDK_INCLUDE%\link\win64"
if not exist "%SDK_INCLUDE%\Geode\cocos\platform\third_party\win32\OGLES\GL\glew.h" (
  echo GEODE_SDK_ROOT does not contain the expected Geode OpenGL headers.
  exit /b 2
)
if not exist "%OUT%" mkdir "%OUT%"
cl /nologo /std:c++20 /EHsc /O2 /W4 "%~dp0frame_checks.cpp" /Fo"%OUT%\frame_checks.obj" /Fe"%OUT%\frame_checks.exe"
if errorlevel 1 exit /b %errorlevel%
"%OUT%\frame_checks.exe"
if errorlevel 1 exit /b 1
cl /nologo /std:c++20 /EHsc /O2 /W4 "%~dp0dirty_state_checks.cpp" /Fo"%OUT%\dirty_state_checks.obj" /Fe"%OUT%\dirty_state_checks.exe"
if errorlevel 1 exit /b %errorlevel%
"%OUT%\dirty_state_checks.exe"
if errorlevel 1 exit /b 1
python "%~dp0source_contract_checks.py"
if errorlevel 1 exit /b 1
cl /nologo /std:c++latest /EHsc /O2 /fp:strict /W4 /I"%SDK_INCLUDE%\Geode\cocos\platform\third_party\win32\OGLES" "%~dp0checks.cpp" /Fo"%OUT%\checks.obj" /Fe"%OUT%\checks.exe" /link "%SDK_LINK%\glew32.lib" opengl32.lib user32.lib gdi32.lib
if errorlevel 1 exit /b 1
if not defined CHLORIUM_R78_COCOS_DLL (
  echo GL/Cocos fixture compiled; execution skipped because CHLORIUM_R78_COCOS_DLL is unset.
  exit /b 0
)
for %%I in ("%CHLORIUM_R78_COCOS_DLL%") do set "COCOS_DIR=%%~dpI"
set "PATH=%COCOS_DIR%;%PATH%"
"%OUT%\checks.exe" "%CHLORIUM_R78_COCOS_DLL%" "%~dp0fixtures\cocos-native.vert" "%~dp0fixtures\cocos-native.frag" "%~dp0canonical_scatter.comp"
exit /b %errorlevel%
