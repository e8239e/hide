CFLAGS = -std=c99 -pedantic -Wall -Werror -O3 -flto
build:
	${CC} ${CFLAGS} hide.c -o hide

build-static:
	${CC} ${CFLAGS} -static hide.c -o hide

build-win:
	zig cc -target x86_64-windows-gnu ${CFLAGS} -static hide.c -o hide.exe

clean:
	rm -f *.o hide hide.exe hide.pdb
