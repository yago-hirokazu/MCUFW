@echo off

set TDMGCC=..\..\..\..\target\windows\TDM-GCC
set PATH=%TDMGCC%\tdm\bin;%TDMGCC%\make-3.81-bin\bin;%TDMGCC%\make-3.81-dep\bin;%PATH%

make clean
make

for /l %%k in (1,1,10) do (
    test_exe.exe test%%k.sh
    fc .\result_test%%k_sh.txt .\result_test\result_test%%k_sh.txt > NUL

    if not %ERRORLEVEL%==0 (
       echo error @ test%%k
       goto error
    )
)
goto end


:error
echo error detected!!!

:end
pause
