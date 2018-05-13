# v8worker2

This is a minimal binding between Go (golang) and V8 JavaScript. Basic concept
is to only expose two methods to JavaScript: send and receive.

V8 Version: 6.6.164 (Feb 2018)


```
git clone --recurse-submodules git://github.com/ry/v8worker2.git
cd v8worker2
./build.py  # this will take ~30 minutes
go test
```
