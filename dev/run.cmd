@echo off
rem Build the browser emulator and serve it -- the one command for an iteration, from cmd or
rem PowerShell, with no execution-policy question to answer. The build itself is dev/run.sh;
rem this finds the POSIX shell Git for Windows installs and hands over to it.
rem   dev\run.cmd              build, then http://localhost:8790/
rem   dev\run.cmd 9000         .. on another port
rem   dev\run.cmd --no-build   serve what is already in build\emu
setlocal
cd /d "%~dp0.."

set "SH="
for %%P in ("%ProgramFiles%\Git\bin\sh.exe" "%ProgramFiles%\Git\usr\bin\sh.exe" "%ProgramFiles(x86)%\Git\bin\sh.exe" "%LocalAppData%\Programs\Git\bin\sh.exe") do if not defined SH if exist "%%~P" set "SH=%%~P"
if not defined SH for /f "delims=" %%P in ('where sh.exe 2^>nul') do if not defined SH set "SH=%%P"
if not defined SH (
    echo dev\run.cmd: no sh.exe found. Install Git for Windows, or run dev/run.sh from Git Bash.
    exit /b 1
)

"%SH%" dev/run.sh %*
