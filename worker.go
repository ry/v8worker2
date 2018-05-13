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

/*
#cgo CXXFLAGS: -std=c++11
#cgo pkg-config: out/v8.pc
#include <stdlib.h>
#include "binding.h"
*/
import "C"
import "errors"

import "unsafe"
import "sync"
import "runtime"

type workerTableIndex int

var workerTableLock sync.Mutex

// This table will store all pointers to all active workers. Because we can't safely
// pass pointers to Go objects to C, we instead pass a key to this table.
var workerTable = make(map[workerTableIndex]*worker)

// Keeps track of the last used table index. Incremeneted when a worker is created.
var workerTableNextAvailable workerTableIndex = 0

// To receive messages from javascript...
type ReceiveMessageCallback func(msg []byte) []byte

// Don't init V8 more than once.
var initV8Once sync.Once

// Internal worker struct which is stored in the workerTable.
// Weak-ref pattern https://groups.google.com/forum/#!topic/golang-nuts/1ItNOOj8yW8/discussion
type worker struct {
	cWorker    *C.worker
	cb         ReceiveMessageCallback
	tableIndex workerTableIndex
}

// This is a golang wrapper around a single V8 Isolate.
type Worker struct {
	*worker
	disposed bool
}

// Return the V8 version E.G. "4.3.59"
func Version() string {
	return C.GoString(C.worker_version())
}

func workerTableLookup(index workerTableIndex) *worker {
	workerTableLock.Lock()
	defer workerTableLock.Unlock()
	return workerTable[index]
}

//export recvCb
func recvCb(buf unsafe.Pointer, buflen C.int, index workerTableIndex) {
	gbuf := C.GoBytes(buf, buflen)
	w := workerTableLookup(index)
	w.cb(gbuf)
	// TODO use the return value of cb()
}

// Creates a new worker, which corresponds to a V8 isolate. A single threaded
// standalone execution context.
func New(cb ReceiveMessageCallback) *Worker {
	workerTableLock.Lock()
	w := &worker{
		cb:         cb,
		tableIndex: workerTableNextAvailable,
	}

	workerTableNextAvailable++
	workerTable[w.tableIndex] = w
	workerTableLock.Unlock()

	initV8Once.Do(func() {
		C.v8_init()
	})

	w.cWorker = C.worker_new(C.int(w.tableIndex))

	externalWorker := &Worker{
		worker:   w,
		disposed: false,
	}

	runtime.SetFinalizer(externalWorker, func(final_worker *Worker) {
		final_worker.Dispose()
	})
	return externalWorker
}

// Forcefully frees up memory associated with worker.
// GC will also free up worker memory so calling this isn't strictly necessary.
func (w *Worker) Dispose() {
	if w.disposed {
		panic("worker already disposed")
	}
	w.disposed = true
	workerTableLock.Lock()
	internalWorker := w.worker
	delete(workerTable, internalWorker.tableIndex)
	workerTableLock.Unlock()
	C.worker_dispose(internalWorker.cWorker)
}

// Load and executes a javascript file with the filename specified by
// scriptName and the contents of the file specified by the param code.
func (w *Worker) Load(scriptName string, code string) error {
	scriptName_s := C.CString(scriptName)
	code_s := C.CString(code)
	defer C.free(unsafe.Pointer(scriptName_s))
	defer C.free(unsafe.Pointer(code_s))

	r := C.worker_load(w.worker.cWorker, scriptName_s, code_s)
	if r != 0 {
		errStr := C.GoString(C.worker_last_exception(w.worker.cWorker))
		return errors.New(errStr)
	}
	return nil
}

// Same as Send but for []byte. $recv callback will get an ArrayBuffer.
func (w *Worker) SendBytes(msg []byte) error {
	msg_p := C.CBytes(msg)
	defer C.free(msg_p)

	r := C.worker_send_bytes(w.worker.cWorker, msg_p, C.size_t(len(msg)))
	if r != 0 {
		errStr := C.GoString(C.worker_last_exception(w.worker.cWorker))
		return errors.New(errStr)
	}

	return nil
}

// Terminates execution of javascript
func (w *Worker) TerminateExecution() {
	C.worker_terminate_execution(w.worker.cWorker)
}
