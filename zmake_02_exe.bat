call tdm492.bat
gcc -O2 -DNDEBUG -s -o MinesweeperPLus.exe ./src/main.c -m32 -mwindows -lwinmm -lcomctl32 -Wall -std=c99 -finput-charset=utf-8 -fexec-charset=gbk res.o
pause