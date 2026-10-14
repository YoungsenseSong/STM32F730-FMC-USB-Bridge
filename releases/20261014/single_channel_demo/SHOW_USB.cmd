@echo off
title Real MEMS USB Demo
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0SHOW_USB.ps1" %*
pause
