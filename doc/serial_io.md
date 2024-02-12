# ELIZA on a Teletype Model 33 ASR

I got ELIZA to work on an ASR 33 teletype. I wrote about it [here](https://sites.google.com/view/elizaarchaeology/blog).

### macOS

Build and run without serial I/O

```text
clang++ -std=c++20 -pedantic -o eliza eliza.cpp
./eliza
```
Build and run with serial I/O

```text
clang++ -std=c++20 -pedantic -D SUPPORT_SERIAL_IO -o eliza eliza.cpp macos_serial_io.cpp
./eliza --port /dev/cu.PL2303G-USBtoUART10
```

(The ASR 33 should be in full duplex mode on macOS.)

### Windows

Build amnd run without serial I/O

```text
cl /EHsc /W4 /std:c++20 eliza.cpp
eliza
```

Build and run with serial I/O

```text
cl /EHsc /W4 /std:c++20 /D SUPPORT_SERIAL_IO eliza.cpp win_serial_io.cpp
eliza /port COM3
```

(The ASR 33 should be in Simplex mode on Windows.)
