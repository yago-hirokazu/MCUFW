@echo off
set PATH=.\tdm\bin;.\make-3.81-bin\bin;.\make-3.81-dep\bin;%PATH%
cd TDM-GCC
make
cd ..
cmd.exe /k "TDM-GCC\main.exe"
pause
