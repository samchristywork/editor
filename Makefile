all: build/editor

build/editor: *.c
	mkdir -p build
	gcc -g -Wall -Wextra *.c -o build/editor

clean:
	rm -rf build
