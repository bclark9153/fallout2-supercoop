@echo off
rem Fallout 2 SuperCoop installer: builds a co-op world from your Fallout 2.
rem Your original game folder is only read, never changed.
title Fallout 2 SuperCoop installer
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" %*
echo.
pause
