# v8worker2

[![Build Status](https://travis-ci.org/ry/v8worker2.svg?branch=master)](https://travis-ci.org/ry/v8worker2)

This is a minimal binding between Go (golang) and V8 JavaScript. Basic concept
is to only expose two methods to JavaScript: send and receive.

V8 Version: 6.6.164 (Feb 2018)


To build:
```
git clone --recurse-submodules git://github.com/ry/v8worker2.git
cd v8worker2
./tools/build.py  # this will take ~30 minutes
go test
```

The JavaScript interface is exposed thru a single global object:
```typescript
V8Worker2.print(str: string): void;

V8Worker2.send(ab: ArrayBuffer): null | ArrayBuffer;

V8Worker2.recv(callback: (ab: ArrayBuffer) => null | ArrayBuffer): void;
```


