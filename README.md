# xemu_wait_trace_plugin

Basic QEMU TCG plugin to trace invocations of `KeWaitForSingleObject` and `KeWaitForMultipleObjects`.

Note that the addresses are likely kernel-specific and may need to be adjusted for your BIOS.

## Use

1. Make sure that xemu is built with `--enable-plugins`
1. Run xemu with `-plugin <path_to_the_built_library> -d plugin`

## Building

Configure by setting the `XEMU_SOURCE_DIR` variable to the path to your xemu source tree.

`cmake -S . -B build -DXEMU_SOURCE_DIR=<path_to_xemu_source> && cmake --build build --verbose`
