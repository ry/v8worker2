/*
Copyright 2018 Ryan Dahl <ry@tinyclouds.org>. All rights reserved.

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
*/
package v8worker2

import (
	"testing"
	"time"
)

func TestVersion(t *testing.T) {
	println(Version())
}

func TestBasic(t *testing.T) {
	recvCount := 0
	worker := New(func(msg []byte) []byte {
		if len(msg) != 5 {
			t.Fatal("bad msg", msg)
		}
		recvCount++
		return nil
	})

	code := `V8Worker2.print("ready");`
	err := worker.Load("code.js", code)
	if err != nil {
		t.Fatal(err)
	}

	codeWithSyntaxError := `V8Worker2.print(hello world");`
	err = worker.Load("codeWithSyntaxError.js", codeWithSyntaxError)
	if err == nil {
		t.Fatal("Expected error")
	}
	//println(err.Error())

	codeWithRecv := `
		V8Worker2.recv(function(msg) {
			var enc = new TextDecoder("ascii");
			var str = enc.decode(msg);
			if (str !== "hii") {
				throw Error("bad message");
			}
			V8Worker2.print("recv msg", str);
		});
		V8Worker2.print("ready");
	`
	err = worker.Load("codeWithRecv.js", codeWithRecv)
	if err != nil {
		t.Fatal(err)
	}
	worker.SendBytes([]byte("hii"))

	codeWithSend := `
		V8Worker2.send(new ArrayBuffer(5));
		V8Worker2.send(new ArrayBuffer(5));
	`
	err = worker.Load("codeWithSend.js", codeWithSend)
	if err != nil {
		t.Fatal(err)
	}

	if recvCount != 2 {
		t.Fatal("bad recvCount", recvCount)
	}
}

func TestPrintUint8Array(t *testing.T) {
	worker := New(func(msg []byte) []byte {
		return nil
	})
	codeWithArrayBufferAllocator := `
		var uint8 = new Uint8Array(16);
		V8Worker2.print(uint8);
	`
	err := worker.Load("buffer.js", codeWithArrayBufferAllocator)
	if err != nil {
		t.Fatal(err)
	}
}

func TestMultipleWorkers(t *testing.T) {
	recvCount := 0
	worker1 := New(func(msg []byte) []byte {
		if len(msg) != 5 {
			t.Fatal("bad message")
		}
		recvCount++
		return nil
	})
	worker2 := New(func(msg []byte) []byte {
		if len(msg) != 3 {
			t.Fatal("bad message")
		}
		recvCount++
		return nil
	})

	err := worker1.Load("1.js", `V8Worker2.send(new ArrayBuffer(5))`)
	if err != nil {
		t.Fatal(err)
	}

	err = worker2.Load("2.js", `V8Worker2.send(new ArrayBuffer(3))`)
	if err != nil {
		t.Fatal(err)
	}

	if recvCount != 2 {
		t.Fatal("bad recvCount", recvCount)
	}
}

func TestRequestFromJS(t *testing.T) {
	var captured []byte
	worker := New(func(msg []byte) []byte {
		captured = msg
		return nil
	})
	code := ` V8Worker2.send(new ArrayBuffer(4)); `
	err := worker.Load("code.js", code)
	if err != nil {
		t.Fatal(err)
	}
	if len(captured) != 4 {
		t.Fail()
	}
}

// Test breaking script execution
func TestWorkerBreaking(t *testing.T) {
	worker := New(func(msg []byte) []byte {
		return nil
	})

	go func(w *Worker) {
		time.Sleep(time.Second)
		w.TerminateExecution()
	}(worker)

	worker.Load("forever.js", ` while (true) { ; } `)
}
