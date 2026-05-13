@echo off
setlocal

rem =======================================================================
rem  run_win.bat - Configure and build PHD2 on Windows
rem
rem  Usage:
rem    run_win.bat                Incremental build (default)
rem    run_win.bat -rebuild       Wipe tmp\ first, then full configure + build
rem    run_win.bat -config        Configure only, no build
rem    run_win.bat -launch        Build then start phd2.exe
rem    run_win.bat -rebuild -launch    Combine flags
rem    run_win.bat -help          Show this help
rem =======================================================================

set CLEAN=0
set BUILD=1
set LAUNCH=0

:parse_args
if "%~1"=="" goto args_done
if /i "%~1"=="-rebuild"     ( set CLEAN=1   & shift & goto parse_args )
if /i "%~1"=="--rebuild"    ( set CLEAN=1   & shift & goto parse_args )
if /i "%~1"=="-config"      ( set BUILD=0   & shift & goto parse_args )
if /i "%~1"=="--config"     ( set BUILD=0   & shift & goto parse_args )
if /i "%~1"=="-config-only" ( set BUILD=0   & shift & goto parse_args )
if /i "%~1"=="-launch"      ( set LAUNCH=1  & shift & goto parse_args )
if /i "%~1"=="--launch"     ( set LAUNCH=1  & shift & goto parse_args )
if /i "%~1"=="-help"        goto show_help
if /i "%~1"=="--help"       goto show_help
if /i "%~1"=="-?"           goto show_help
if /i "%~1"=="/?"           goto show_help
echo Unknown option: %~1
echo.
goto show_help

:show_help
echo.
echo Usage: run_win.bat [-rebuild] [-config] [-launch] [-help]
echo.
echo   -rebuild  Wipe tmp\ and run a full configure + build from scratch.
echo             Without this, the script does an incremental build on
echo             top of the existing tmp\ (much faster after first run).
echo   -config   Only run cmake configure, do not build.
echo   -launch   After a successful build, start tmp\Release\phd2.exe.
echo   -help     Show this message.
echo.
exit /b 0

:args_done

rem -----------------------------------------------------------------------
rem  Treat "tmp\ does not exist" as an implicit -rebuild.
rem -----------------------------------------------------------------------
if not exist tmp set CLEAN=1

if %CLEAN%==1 goto do_clean
goto skip_clean

:do_clean
rem -----------------------------------------------------------------------
rem  Kill known build-process stragglers that hold file handles in tmp\.
rem  Returns nonzero when the process isn't running -- that's fine.
rem -----------------------------------------------------------------------
taskkill /f /im vctip.exe >nul 2>&1
taskkill /f /im vcpkg.exe >nul 2>&1
taskkill /f /im cl.exe >nul 2>&1
taskkill /f /im MSBuild.exe >nul 2>&1

rem -----------------------------------------------------------------------
rem  Robust delete: try rmdir, retry once, then fall back to renaming the
rem  directory out of the way (works even when files inside are locked).
rem -----------------------------------------------------------------------
if exist tmp (
    rmdir /s /q tmp 2>nul
    if exist tmp (
        echo Locks detected on tmp\. Waiting 3s then retrying...
        timeout /t 3 /nobreak >nul
        rmdir /s /q tmp 2>nul
    )
)

if exist tmp (
    set "STAMP=%RANDOM%%RANDOM%"
    move tmp tmp_locked_%STAMP% >nul 2>&1
    if errorlevel 1 (
        echo.
        echo ERROR: Could not delete or rename tmp\.
        echo A process likely has tmp\ as its current working directory.
        echo Common culprits: a shell sitting in tmp\, Visual Studio with
        echo                  the project loaded, Windows Search indexing.
        echo.
        echo Open Resource Monitor ^(resmon^), search "tmp" under Associated
        echo Handles on the CPU tab, then close the offending process.
        exit /b 1
    )
    echo Locked tmp\ has been renamed to tmp_locked_%STAMP%\
    echo ^(Delete it manually later when nothing holds it.^)
)

mkdir tmp

rem -----------------------------------------------------------------------
rem  Pre-clone vcpkg with visible progress. CMake's FetchContent normally
rem  does this clone silently inside an MSBuild custom-build step.
rem -----------------------------------------------------------------------
where git >nul 2>&1
if errorlevel 1 (
    echo ERROR: git is required but not on PATH.
    exit /b 1
)

mkdir tmp\_deps
echo.
echo Pre-cloning vcpkg ^(~500MB; one-time per clean build^)...
git clone --progress https://github.com/microsoft/vcpkg.git tmp\_deps\vcpkg-src
if errorlevel 1 (
    echo.
    echo ERROR: vcpkg clone failed. Check network/proxy settings.
    exit /b 1
)

:skip_clean

cd tmp

set VCPKG_MAX_CONCURRENCY=%NUMBER_OF_PROCESSORS%
set CMAKE_BUILD_PARALLEL_LEVEL=%NUMBER_OF_PROCESSORS%
set "CL=%CL% /MP"

rem -----------------------------------------------------------------------
rem  Configure. For incremental, cmake --build below would also pick up
rem  CMakeLists changes, but running cmake explicitly surfaces config
rem  errors earlier and lets -config work on its own.
rem -----------------------------------------------------------------------
cmake -Wno-dev -A Win32 ..
if errorlevel 1 (
    echo.
    echo CMake configure failed.
    cd ..
    exit /b 1
)

if %BUILD%==0 (
    cd ..
    echo.
    echo Configure complete. Skipping build ^(-config flag^).
    exit /b 0
)

echo.
echo ============================================================
echo Building with %NUMBER_OF_PROCESSORS% cores ^(Release^)...
echo ============================================================
echo.

cmake --build . --config Release --parallel %NUMBER_OF_PROCESSORS%
if errorlevel 1 (
    echo.
    echo Build failed. See errors above.
    cd ..
    exit /b 1
)

cd ..

echo.
echo ============================================================
echo Build complete.
echo.
echo   Binary:  tmp\Release\phd2.exe
echo   Launch:  start tmp\Release\phd2.exe
echo.
echo ============================================================

if %LAUNCH%==1 (
    echo Launching phd2.exe...
    start "" "tmp\Release\phd2.exe"
)

exit /b 0
