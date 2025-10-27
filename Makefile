all: build/editor

build/editor: src/*.c src/*.h
	mkdir -p build
	gcc -g -Wpedantic -Wall -Wextra src/*.c -o build/editor

run: all
	./build/editor

cppcheck:
	mkdir -p log
	cppcheck --enable=all --inconclusive --quiet . 2> log/cppcheck.log

clang-tidy:
	mkdir -p log
	clang-tidy src/* -- -I. > log/clang-tidy.log 2>&1

valgrind: all
	mkdir -p log
	valgrind --log-file=log/valgrind.log --leak-check=full ./build/editor LICENSE
	cat log/valgrind.log | head -n 20

asan: clean
	mkdir -p build log
	gcc -fsanitize=address -g src/*.c -o build/editor
	ASAN_OPTIONS=log_path=log/asan.log ./build/editor LICENSE || true

ubsan: clean
	mkdir -p build log
	gcc -fsanitize=undefined -g src/*.c -o build/editor
	UBSAN_OPTIONS=log_path=log/ubsan.log ./build/editor LICENSE || true

tsan: clean
	mkdir -p build log
	gcc -fsanitize=thread -g src/*.c -o build/editor
	TSAN_OPTIONS=log_path=log/tsan.log ./build/editor LICENSE || true

gprof: clean
	mkdir -p build log
	gcc -pg -g -Wall -Wextra src/*.c -o build/editor
	./build/editor
	gprof build/editor gmon.out > log/gprof.txt

gcov: clean
	mkdir -p build log
	gcc --coverage -g -Wall -Wextra src/*.c -o build/editor
	./build/editor
	gcov src/*.c > log/gcov.txt

analyze: clean all
	make cppcheck
	make clang-tidy
	make valgrind
	make asan
	make ubsan
	make tsan
	make gprof
	make gcov

clean:
	rm -rf build
