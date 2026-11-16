@echo off
rem Build the C tools (tools\c) from any working directory.
rem A convenience wrapper only: the contract is the CMake presets in tools\c\CMakePresets.json. Runs, in tools\c:
rem   cmake --preset tools                                            (configure)
rem   cmake --build --preset tools --parallel N -- /nodeReuse:false   (build, Release)
rem   ctest --preset tools -j N                                       (--test: the unit tests)
rem Executables land in build\tools\, tests in build\tools\tests\.
rem cmake is taken from the SHSM_CMAKE environment variable (full path to cmake.exe) when set, else from PATH.
setlocal EnableExtensions
set "HERE=%~dp0"
set "MSBUILDDISABLENODEREUSE=1"
set "DOTEST=0"
set "JOBS=%NUMBER_OF_PROCESSORS%"
if "%JOBS%"=="" set "JOBS=2"

:args
if "%~1"=="" goto argsdone
if /i "%~1"=="--test" (set "DOTEST=1" & shift & goto args)
if /i "%~1"=="--help" goto usage
if /i "%~1"=="-h" goto usage
if /i "%~1"=="/?" goto usage
if /i "%~1"=="--parallel" goto jobs
if /i "%~1"=="-j" goto jobs
echo build_c: unknown option %~1 1>&2
exit /b 2
:jobs
if "%~2"=="" (echo build_c: %~1 needs a number 1>&2 & exit /b 2)
set "JOBS=%~2"
shift
shift
goto args
:argsdone

set "CMAKE="
if defined SHSM_CMAKE set "CMAKE=%SHSM_CMAKE%"
if not defined CMAKE for %%C in (cmake.exe) do set "CMAKE=%%~$PATH:C"
if not defined CMAKE (
  echo build_c: cmake.exe not found: put CMake 4.x on PATH or set SHSM_CMAKE to the full path of cmake.exe 1>&2
  exit /b 2
)
if not exist "%CMAKE%" (
  echo build_c: cmake not found at the path in SHSM_CMAKE: %CMAKE% 1>&2
  exit /b 2
)
for %%D in ("%CMAKE%") do set "CMAKEDIR=%%~dpD"

pushd "%HERE%c" || exit /b 2
"%CMAKE%" --preset tools
if errorlevel 1 goto fail
"%CMAKE%" --build --preset tools --parallel %JOBS% -- /nodeReuse:false
if errorlevel 1 goto fail
if not "%DOTEST%"=="1" goto done
set "CTEST=%CMAKEDIR%ctest.exe"
if not exist "%CTEST%" set "CTEST=ctest.exe"
"%CTEST%" --preset tools -j %JOBS%
if errorlevel 1 goto fail
:done
popd
exit /b 0

:fail
set "RC=%ERRORLEVEL%"
popd
exit /b %RC%

:usage
echo Usage: build_c.cmd [--test] [--parallel N ^| -j N] [--help]
echo   Builds the C tools (tools\c) with the CMake preset "tools" (Release); executables land in build\tools.
echo   --test          also run the unit tests (ctest --preset tools)
echo   --parallel N    build jobs and CTest jobs (default: number of processors)
exit /b 0
