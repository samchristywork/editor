all: build/editor

build/editor: *.c
	mkdir -p build
	gcc -g -Wall -Wextra *.c -o build/editor

run: all
	./build/editor

cppcheck:
	mkdir -p log
	cppcheck --enable=all --inconclusive --quiet . 2> log/cppcheck.log

clang-tidy:
	mkdir -p log
	clang-tidy *.c -- -I. > log/clang-tidy.log 2>&1

valgrind: all
	mkdir -p log
	valgrind --log-file=log/valgrind.log --leak-check=full ./build/editor LICENSE
	cat log/valgrind.log | head -n 20

asan: clean
	mkdir -p build log
	gcc -fsanitize=address -g *.c -o build/editor
	ASAN_OPTIONS=log_path=log/asan.log ./build/editor LICENSE || true

ubsan: clean
	mkdir -p build log
	gcc -fsanitize=undefined -g *.c -o build/editor
	UBSAN_OPTIONS=log_path=log/ubsan.log ./build/editor LICENSE || true

tsan: clean
	mkdir -p build log
	gcc -fsanitize=thread -g *.c -o build/editor
	TSAN_OPTIONS=log_path=log/tsan.log ./build/editor LICENSE || true

clean:
	rm -rf build
