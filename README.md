# v8worker2

[![Build Status](https://travis-ci.org/ry/v8worker2.svg?branch=master)](https://travis-ci.org/ry/v8worker2)

This is a minimal binding between Go (golang) and V8 JavaScript. Basic concept
is to only expose two methods to JavaScript: send and receive.

V8 Version: 6.8.275.3 (May 2018)

[A rather dated presentation on this project](https://docs.google.com/presentation/d/1RgGVgLuP93mPZ0lqHhm7TOpxZBI3TEdAJQZzFqeleAE/edit?usp=sharing)


## Installing

Due to the complexity of building V8, this package is not buildable with `go
get`.

To install:
```
go get github.com/ry/v8worker2
cd `go env GOPATH`/src/github.com/ry/v8worker2
./tools/build.py # Will take ~30 minutes to compile.
go test
```
If you have ccache installed, the build will take advantage of it.


## JavaScript API

The JavaScript interface is exposed thru a single global namespace `V8Worker2`.
The interface has just three methods `V8worker2.print()`, `V8Worker2.send()`,
and `V8Worker2.recv()`.
See
[v8worker2.d.ts](https://github.com/ry/v8worker2/blob/master/v8worker2.d.ts)
for the details.


## Golang API

Documentation is at https://godoc.org/github.com/ry/v8worker2 and
example usage is at
[worker_test.go](https://github.com/ry/v8worker2/blob/master/worker_test.go)


## Difference from the original v8worker

 * The original v8worker passed strings between Go and V8. v8worker2 instead
   communicates using ArrayBuffer, which is more efficient.

 * The original included `recvSync` and `sendSync` methods. These were
   deemed unnecessary. Now `send()` can operate both sychronously by
   returning another ArrayBuffer. Simply return a `[]byte` from the golang
   recv callback.

 * This version is compatible with modern V8, has a better build
   setup, and uses Travis for CI.

 * The original prefixed the methods with dollar signs, this version uses a
   global name space object and provides a typescript declaration file.


## License

MIT License. Contributions welcome.

		Copyright 2015-2018 Ryan Dahl <ry@tinyclouds.org>. All rights reserved.

		Permission is hereby granted, free of charge, to any person obtaining a copy
		of this software and associated documentation files (the "Software"), to
		deal in the Software without restriction, including without limitation the
		rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
		sell copies of the Software, and to permit persons to whom the Software is
		furnished to do so, subject to the following conditions:

		The above copyright notice and this permission notice shall be included in
		all copies or substantial portions of the Software.

		THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
		IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
		FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
		AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
		LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
		FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
		IN THE SOFTWARE.
