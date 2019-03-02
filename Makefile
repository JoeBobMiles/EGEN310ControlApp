
build_dir = ./build
src_dir = ./src

binary = $(build_dir)/controlapp.exe
libraries = user32

default: $(build_dir)
	clang $(src_dir)/main.c -l $(libraries) -o $(binary)

$(build_dir):
	mkdir -p $(build_dir)

clean:
	rm -rf build/
