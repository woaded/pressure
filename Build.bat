@echo off

call C:\msys64\msys2_shell.cmd -mingw64 -where "%CD%" -defterm -no-start -shell bash -c "make clean && make"
echo Build complete