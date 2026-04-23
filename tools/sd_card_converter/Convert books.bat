@echo off
cd /d "%~dp0"
where py >nul 2>nul
if errorlevel 1 (
  python convert_books.py
) else (
  py -3 convert_books.py
)
if errorlevel 1 (
  echo.
  echo If Python was not found, install Python 3 from python.org and try again.
  pause
)
