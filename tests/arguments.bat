@echo off
echo argc-style dump
echo 1=%~1
echo 2=%~2
echo 3=%~3
echo *=%*
if "%~1"=="" exit /b 10
exit /b 0
