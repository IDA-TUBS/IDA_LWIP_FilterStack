Steps to compile lwip library for unix:
```
make OS=unix CPU=none LWIPOPTSDIR=examples/include/ all
cd examples
make all
```

If lwip debug should be enabled, this can either be done in a lwipopts.h or by passing it to the CFLAGS
```
CFLAGS="-D LWIP_DEBUG" make OS=unix CPU=none LWIPOPTSDIR=examples/include/ all
```

Setup virtual tap device for unix:
```
source setp-tapif
```

Test UDP Echo Server
```
echo "test" | nc -u 192.168.0.200 7
```
