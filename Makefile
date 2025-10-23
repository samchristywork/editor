all: build/editor

build/editor: *.c
	mkdir -p build
	gcc -g -Wall -Wextra *.c -o build/editor

run: all
	./build/editor

clang-tidy:
	mkdir -p log
	clang-tidy *.c -- -I. > log/clang-tidy.log 2>&1

valgrind: all
	mkdir -p log
	valgrind --log-file=log/valgrind.log --leak-check=full ./build/editor LICENSE
	cat log/valgrind.log | head -n 20

clean:
	rm -rf build
