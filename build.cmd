@echo off
setlocal
cd /d "%~dp0"
for /f "usebackq delims=" %%P in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%P"
if not defined VSROOT exit /b 1
call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build mkdir build
rc /nologo /fo build\resources.res resources.rc
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /MT /W4 /WX /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /DWIN32_LEAN_AND_MEAN /D_WIN32_WINNT=0x0A00 /Fo:build\ /Fe:build\WindowGuard.exe main.cpp guard.cpp settings.cpp settings_ui.cpp build\resources.res user32.lib shell32.lib advapi32.lib dwmapi.lib gdi32.lib comctl32.lib comdlg32.lib uxtheme.lib ole32.lib /link /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTINPUT:app.manifest
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /MT /W4 /WX /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /DWIN32_LEAN_AND_MEAN /D_WIN32_WINNT=0x0A00 /Fo:build\ /Fe:build\GuardTests.exe tests.cpp guard.cpp settings.cpp user32.lib dwmapi.lib shell32.lib ole32.lib /link /SUBSYSTEM:CONSOLE /MANIFEST:EMBED /MANIFESTINPUT:app.manifest
exit /b %errorlevel%
