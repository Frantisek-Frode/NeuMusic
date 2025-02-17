build/mpris.o: src/mpris.c
	mkdir -p build
	gcc `pkg-config --cflags dbus-1` $< -o $@ `pkg-config --libs dbus-1`
