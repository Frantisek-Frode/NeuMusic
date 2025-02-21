# What is this
A basic music player written in C using `libav` and `libao`.


# Compilation
## Linux
Make sure [libao](https://xiph.org/ao/), [libav](https://github.com/libav/libav) and `dbus-1` are in include path. Then run `make`.

## Other platforms
The part interfacing with dbus (`mrpis.c`) will need to be removed/replaced by another way to listen for media keys.
`input.c` might also need to be reworked, at least on Windows.

<sup>If you manage to get it working, please submit a PR.</sup>
