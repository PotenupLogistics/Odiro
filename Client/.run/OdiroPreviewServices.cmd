@echo off
setlocal EnableExtensions
set "REPO_ROOT=%~dp0..\.."
call "%REPO_ROOT%\task-run.bat" -SkipClient
exit /b %ERRORLEVEL%
