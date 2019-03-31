
build_dir = ./build
src_dir = ./src

binary = $(build_dir)/controlapp.exe
libraries = user32 gdi32 bthprops ws2_32

default: $(build_dir)
	clang -g -gcodeview $(src_dir)/main.c $(addprefix -l,$(libraries)) -o $(binary)

$(build_dir):
	mkdir -p $(build_dir)

clean:
	rm -rf build/
