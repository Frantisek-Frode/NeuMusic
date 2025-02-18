build/music: src/* Makefile
	mkdir -p build
	gcc -ggdb\
		`pkg-config --cflags dbus-1`\
		src/*.c -o $@\
		-lm\
		-lao\
		-lavcodec -lavformat -lavutil -lswresample\
		`pkg-config --libs dbus-1`

