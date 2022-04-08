


references:
https://github.com/germandiagogomez/getting-started-with-meson
https://github.com/kstenerud/meson-examples/tree/master/library
https://chromium.googlesource.com/external/github.com

useful build commands:
meson build  --buildtype=debug --wipe
meson build  --buildtype=release --wipe
ninja -C build

./build/src/avro-person/avro-person 10