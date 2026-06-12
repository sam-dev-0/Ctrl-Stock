@echo off
setlocal
cd /d "%~dp0"

gcc -std=c11 -Wall -Wextra -Wpedantic -O2 ^
  -IHeaders ^
  main.c ^
  Sources\produto.c ^
  Sources\estoque.c ^
  Sources\arquivo.c ^
  Sources\arq16_csv.c ^
  Sources\arq16.c ^
  Sources\str16.c ^
  Sources\tui16.c ^
  -o estoque.exe

if errorlevel 1 (
  echo.
  echo Falha na compilacao.
  exit /b 1
)

echo.
echo Compilacao concluida: estoque.exe
