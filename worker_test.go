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
	worker := New(func(msg string) {
		println("recv cb", msg)
		if msg != "hello" {
			t.Fatal("bad msg", msg)
		}
		recvCount++
	})

	code := ` V8Worker2.print("ready"); `
	err := worker.Load("code.js", code)
	if err != nil {
		t.Fatal(err)
	}

	codeWithSyntaxError := ` V8Worker2.print(hello world"); `
	err = worker.Load("codeWithSyntaxError.js", codeWithSyntaxError)
	if err == nil {
		t.Fatal("Expected error")
	}
	//println(err.Error())

	codeWithRecv := `
		V8Worker2.recv(function(msg) {
			V8Worker2.print("recv msg", msg);
		});
		V8Worker2.print("ready");
	`
	err = worker.Load("codeWithRecv.js", codeWithRecv)
	if err != nil {
		t.Fatal(err)
	}
	worker.Send("hi")

	codeWithSend := `
		V8Worker2.send("hello");
		V8Worker2.send("hello");
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
	worker := New(func(msg string) {})
	codeWithArrayBufferAllocator := ` var uint8 = new Uint8Array(256); V8Worker2.print(uint8); `
	err := worker.Load("buffer.js", codeWithArrayBufferAllocator)
	if err != nil {
		t.Fatal(err)
	}
}

func TestMultipleWorkers(t *testing.T) {
	recvCount := 0
	worker1 := New(func(msg string) {
		println("w1", msg)
		recvCount++
	})
	worker2 := New(func(msg string) {
		println("w2", msg)
		recvCount++
	})

	err := worker1.Load("1.js", `V8Worker2.send("hello1")`)
	if err != nil {
		t.Fatal(err)
	}

	err = worker2.Load("2.js", `V8Worker2.send("hello2")`)
	if err != nil {
		t.Fatal(err)
	}

	if recvCount != 2 {
		t.Fatal("bad recvCount", recvCount)
	}
}

func TestRequestFromJS(t *testing.T) {
	var caught string
	worker := New(func(msg string) {
		println("recv cb", msg)
		caught = msg
	})
	code := ` V8Worker2.send("ping"); `
	err := worker.Load("code.js", code)
	if err != nil {
		t.Fatal(err)
	}
	if caught != "ping" {
		t.Fail()
	}
}

/*
//I have profiled this repeatedly with massive values to ensure
//memory does indeed get reclaimed and that the finalizer
// gets called and the C-side of this does clean up memory correctly.
func TestWorkerDeletion(t *testing.T) {
	recvCount := 0
	for i := 1; i <= 100; i++ {
		worker := New(func(msg string) {
			println("worker", msg)
			recvCount++
		})
		err := worker.Load("1.js", `V8Worker2.send("hello1")`)
		if err != nil {
			t.Fatal(err)
		}
		runtime.GC()
	}

	if recvCount != 100 {
		t.Fatal("bad recvCount", recvCount)
	}
}
*/

// Test breaking script execution
func TestWorkerBreaking(t *testing.T) {
	worker := New(func(msg string) {
		println("recv cb", msg)
	})

	go func(w *Worker) {
		time.Sleep(time.Second)
		w.TerminateExecution()
	}(worker)

	worker.Load("forever.js", ` while (true) { ; } `)
}

/*
func TestTightCreateLoop(t *testing.T) {
	println("create 3000 workers in tight loop to see if we get OOM")
	for i := 0; i < 3000; i++ {
		runSimpleWorker(t)
	}
	println("success")
}

func runSimpleWorker(t *testing.T) {
	w := New(nil, nil)
	defer w.Dispose()

	err := w.Load("mytest.js", `
	               // Do something
	               var something = "Simple JavaScript";
	       `)

	if err != nil {
		t.Fatal(err)
	}
}
*/
