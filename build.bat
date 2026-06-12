@echo off
setlocal

gcc -std=c11 -Wall -Wextra -Wpedantic -O2 ^
  main.c produto.c estoque.c arquivo.c arq16_csv.c arq16.c str16.c tui16.c ^
  -o estoque.exe

if errorlevel 1 (
  echo.
  echo Falha na compilacao.
  exit /b 1
)

echo.
echo Compilacao concluida: estoque.exe
