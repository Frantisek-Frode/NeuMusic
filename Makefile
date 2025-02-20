CC := gcc
# TODO: separate
CFLAGS := -ggdb $(shell pkg-config --cflags dbus-1)
CLIBS := -lm -lao -lavcodec -lavformat -lavutil -lswresample $(shell pkg-config --libs dbus-1)

OUTPUT := $(shell ls src/*.c | sed -e 's|src|build/tmp|')
OBJS := $(OUTPUT:.c=.o)


# link
build/music: $(OBJS) $(OBJS:.o:.d)
	gcc $(OBJS) -o build/music $(CLIBS)

# get dependencies
# TODO: get clean working without includes
# ifneq ($(strip), $(filter-out clean, $(MAKECMDGOALS)))
-include $(OBJS:.o=.d)
# endif

# compile and generate dependency info
build/tmp/%.o build/tmp/%.d: src/%.c
	@mkdir -p build/tmp
	gcc -c -MMD $(CFLAGS) src/$*.c -o build/tmp/$*.o
	@mv -f build/tmp/$*.d build/tmp/$*.d.tmp
	@sed -e 's|.*:|build/tmp/$*.o:|' < build/tmp/$*.d.tmp > build/tmp/$*.d
	@sed -e 's|.*:|build/tmp/$*.d:|' < build/tmp/$*.d.tmp >> build/tmp/$*.d
	@sed -e 's/.*://' -e 's/\\$$//' < build/tmp/$*.d.tmp | fmt -1 | \
	  sed -e 's/^ *//' -e 's/$$/:/' >> build/tmp/$*.d
	@rm -f build/tmp/$*.d.tmp

.PHONY: clean
clean:
	rm -f build/tmp/* build/music

