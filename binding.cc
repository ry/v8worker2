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
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <map>

#include "binding.h"
#include "libplatform/libplatform.h"
#include "v8.h"
#include "_cgo_export.h"

using namespace v8;

struct worker_s {
  int table_index;
  Isolate* isolate;
  std::string last_exception;
  Persistent<Function> recv;
  Persistent<Context> context;
  std::map<std::string, Eternal<Module>> modules;
};

// Extracts a C string from a V8 Utf8Value.
const char* ToCString(const String::Utf8Value& value) {
  return *value ? *value : "<string conversion failed>";
}

/*
bool AbortOnUncaughtExceptionCallback(Isolate* isolate) {
  return true;
}

void MessageCallback2(Local<Message> message, Local<Value> data) {
  printf("MessageCallback2\n\n");
}

void FatalErrorCallback2(const char* location, const char* message) {
  printf("FatalErrorCallback2\n");
}
*/

void ExitOnPromiseRejectCallback(PromiseRejectMessage promise_reject_message) {
  auto isolate = Isolate::GetCurrent();
  worker* w = (worker*)isolate->GetData(0);
  assert(w->isolate == isolate);
  HandleScope handle_scope(w->isolate);
  auto context = w->context.Get(w->isolate);

  auto exception = promise_reject_message.GetValue();

  auto message = Exception::CreateMessage(isolate, exception);
  auto onerrorStr = String::NewFromUtf8(w->isolate, "onerror");
  auto onerror = context->Global()->Get(onerrorStr);

  if (onerror->IsFunction()) {
    Local<Function> func = Local<Function>::Cast(onerror);
    Local<Value> args[5];
    auto origin = message->GetScriptOrigin();
    args[0] = exception->ToString();
    args[1] = message->GetScriptResourceName();
    args[2] = origin.ResourceLineOffset();
    args[3] = origin.ResourceColumnOffset();
    args[4] = exception;
    func->Call(context->Global(), 5, args);
    /* message, source, lineno, colno, error */
  } else {
    printf("Unhandled Promise\n");
    message->PrintCurrentStackTrace(isolate, stdout);
  }

  exit(1);
}

MaybeLocal<Module> ResolveCallback(Local<Context> context,
                                   Local<String> specifier,
                                   Local<Module> referrer) {
  auto isolate = Isolate::GetCurrent();
  worker* w = (worker*)isolate->GetData(0);

  HandleScope handle_scope(isolate);

  String::Utf8Value str(specifier);
  const char* moduleName = *str;

  if (w->modules.count(moduleName) == 0) {
    std::string out;
    out.append("Module (");
    out.append(moduleName);
    out.append(") has not been loaded");
    out.append("\n");
    w->last_exception = out;
    return MaybeLocal<Module>();
  }

  return w->modules[moduleName].Get(isolate);
}

// Exception details will be appended to the first argument.
std::string ExceptionString(worker* w, TryCatch* try_catch) {
  std::string out;
  size_t scratchSize = 20;
  char scratch[scratchSize];  // just some scratch space for sprintf

  HandleScope handle_scope(w->isolate);
  Local<Context> context = w->context.Get(w->isolate);
  String::Utf8Value exception(try_catch->Exception());
  const char* exception_string = ToCString(exception);

  Handle<Message> message = try_catch->Message();

  if (message.IsEmpty()) {
    // V8 didn't provide any extra information about this error; just
    // print the exception.
    out.append(exception_string);
    out.append("\n");
  } else {
    // Print (filename):(line number)
    String::Utf8Value filename(message->GetScriptOrigin().ResourceName());
    const char* filename_string = ToCString(filename);
    int linenum = message->GetLineNumber();

    snprintf(scratch, scratchSize, "%i", linenum);
    out.append(filename_string);
    out.append(":");
    out.append(scratch);
    out.append("\n");

    // Print line of source code.
    String::Utf8Value sourceline(message->GetSourceLine());
    const char* sourceline_string = ToCString(sourceline);

    out.append(sourceline_string);
    out.append("\n");

    // Print wavy underline (GetUnderline is deprecated).
    int start = message->GetStartColumn(context).FromJust();
    for (int i = 0; i < start; i++) {
      out.append(" ");
    }
    int end = message->GetEndColumn(context).FromJust();
    for (int i = start; i < end; i++) {
      out.append("^");
    }
    out.append("\n");
    String::Utf8Value stack_trace(try_catch->StackTrace());
    if (stack_trace.length() > 0) {
      const char* stack_trace_string = ToCString(stack_trace);
      out.append(stack_trace_string);
      out.append("\n");
    } else {
      out.append(exception_string);
      out.append("\n");
    }
  }
  return out;
}

extern "C" {

const char* worker_version() { return V8::GetVersion(); }

void worker_set_flags(int* argc, char** argv) {
  V8::SetFlagsFromCommandLine(argc, argv, true);
}

const char* worker_last_exception(worker* w) {
  return w->last_exception.c_str();
}

int worker_load(worker* w, char* name_s, char* source_s) {
  Locker locker(w->isolate);
  Isolate::Scope isolate_scope(w->isolate);
  HandleScope handle_scope(w->isolate);

  Local<Context> context = Local<Context>::New(w->isolate, w->context);
  Context::Scope context_scope(context);

  TryCatch try_catch(w->isolate);

  Local<String> name = String::NewFromUtf8(w->isolate, name_s);
  Local<String> source = String::NewFromUtf8(w->isolate, source_s);

  ScriptOrigin origin(name);

  Local<Script> script = Script::Compile(source, &origin);

  if (script.IsEmpty()) {
    assert(try_catch.HasCaught());
    w->last_exception = ExceptionString(w, &try_catch);
    return 1;
  }

  Handle<Value> result = script->Run();

  if (result.IsEmpty()) {
    assert(try_catch.HasCaught());
    w->last_exception = ExceptionString(w, &try_catch);
    return 2;
  }

  return 0;
}

int worker_load_module(worker* w, char* name_s, char* source_s, int callback_index) {
  Locker locker(w->isolate);
  Isolate::Scope isolate_scope(w->isolate);
  HandleScope handle_scope(w->isolate);

  Local<Context> context = Local<Context>::New(w->isolate, w->context);
  Context::Scope context_scope(context);

  TryCatch try_catch(w->isolate);

  Local<String> name = String::NewFromUtf8(w->isolate, name_s);
  Local<String> source_text = String::NewFromUtf8(w->isolate, source_s);

  Local<Integer> line_offset = Integer::New(w->isolate, 0);
  Local<Integer> column_offset = Integer::New(w->isolate, 0);
  Local<Boolean> is_cross_origin = True(w->isolate);
  Local<Integer> script_id = Local<Integer>();
  Local<Value> source_map_url = Local<Value>();
  Local<Boolean> is_opaque = False(w->isolate);
  Local<Boolean> is_wasm = False(w->isolate);
  Local<Boolean> is_module = True(w->isolate);

  ScriptOrigin origin(name, line_offset, column_offset, is_cross_origin,
                      script_id, source_map_url, is_opaque, is_wasm, is_module);

  ScriptCompiler::Source source(source_text, origin);
  Local<Module> module;

  if (!ScriptCompiler::CompileModule(w->isolate, &source).ToLocal(&module)) {
    assert(try_catch.HasCaught());
    w->last_exception = ExceptionString(w, &try_catch);
    return 1;
  }

  for (int i = 0; i < module->GetModuleRequestsLength(); i++) {
    Local<String> dependency = module->GetModuleRequest(i);
    String::Utf8Value str(dependency);
    char* dependencySpecifier = *str;

    // If we've already loaded the module, skip resolving it.
    // TODO: Is there ever a time when the specifier would be the same
    // but would need to be resolved again?
    if (w->modules.count(dependencySpecifier) != 0) {
      continue;
    }

    int ret = ResolveModule(dependencySpecifier, name_s, callback_index);
    if (ret != 0) {
      // TODO: Use module->GetModuleRequestLocation() to get source locations
      std::string out;
      out.append("Module (");
      out.append(dependencySpecifier);
      out.append(") has not been loaded");
      out.append("\n");
      w->last_exception = out;
      return ret;
    }
  }

  Eternal<Module> persModule(w->isolate, module);
  w->modules[name_s] = persModule;

  Maybe<bool> ok = module->InstantiateModule(context, ResolveCallback);

  if (!ok.FromMaybe(false)) {
    // TODO: I'm not sure if this is needed
    if (try_catch.HasCaught()) {
      assert(try_catch.HasCaught());
      w->last_exception = ExceptionString(w, &try_catch);
    }
    return 2;
  }

  MaybeLocal<Value> result = module->Evaluate(context);

  if (result.IsEmpty()) {
    assert(try_catch.HasCaught());
    w->last_exception = ExceptionString(w, &try_catch);
    return 2;
  }

  return 0;
}

void Print(const FunctionCallbackInfo<Value>& args) {
  bool first = true;
  for (int i = 0; i < args.Length(); i++) {
    HandleScope handle_scope(args.GetIsolate());
    if (first) {
      first = false;
    } else {
      printf(" ");
    }
    String::Utf8Value str(args[i]);
    const char* cstr = ToCString(str);
    printf("%s", cstr);
  }
  printf("\n");
  fflush(stdout);
}

// Sets the recv callback.
void Recv(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  worker* w = (worker*)isolate->GetData(0);
  assert(w->isolate == isolate);

  HandleScope handle_scope(isolate);

  auto context = w->context.Get(w->isolate);

  Local<Value> v = args[0];
  assert(v->IsFunction());
  Local<Function> func = Local<Function>::Cast(v);

  w->recv.Reset(isolate, func);
}

// Called from JavaScript, routes message to golang.
void Send(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  worker* w = static_cast<worker*>(isolate->GetData(0));
  assert(w->isolate == isolate);

  Locker locker(w->isolate);
  EscapableHandleScope handle_scope(isolate);

  auto context = w->context.Get(w->isolate);

  Local<Value> v = args[0];
  assert(v->IsArrayBuffer());

  auto ab = Local<ArrayBuffer>::Cast(v);
  auto contents = ab->GetContents();

  void* buf = contents.Data();
  int buflen = static_cast<int>(contents.ByteLength());

  auto retbuf = recvCb(buf, buflen, w->table_index);
  if (retbuf.data) {
    auto ab = ArrayBuffer::New(w->isolate, retbuf.data, retbuf.len,
                               ArrayBufferCreationMode::kInternalized);
    /*
    // I'm slightly worried the above ArrayBuffer construction leaks memory
    // the following might be a safer way to do it.
    auto ab = ArrayBuffer::New(w->isolate, retbuf.len);
    auto contents = ab->GetContents();
    memcpy(contents.Data(), retbuf.data, retbuf.len);
    free(retbuf.data);
    */
    args.GetReturnValue().Set(handle_scope.Escape(ab));
  }
}

// Called from golang. Must route message to javascript lang.
// non-zero return value indicates error. check worker_last_exception().
int worker_send_bytes(worker* w, void* data, size_t len) {
  Locker locker(w->isolate);
  Isolate::Scope isolate_scope(w->isolate);
  HandleScope handle_scope(w->isolate);

  auto context = w->context.Get(w->isolate);

  TryCatch try_catch(w->isolate);

  Local<Function> recv = Local<Function>::New(w->isolate, w->recv);
  if (recv.IsEmpty()) {
    w->last_exception = "V8Worker2.recv has not been called.";
    return 1;
  }

  Local<Value> args[1];
  args[0] = ArrayBuffer::New(w->isolate, data, len,
                             ArrayBufferCreationMode::kInternalized);
  assert(!args[0].IsEmpty());
  assert(!try_catch.HasCaught());

  recv->Call(context->Global(), 1, args);

  if (try_catch.HasCaught()) {
    w->last_exception = ExceptionString(w, &try_catch);
    return 2;
  }

  return 0;
}

void v8_init() {
  Platform* platform = platform::CreateDefaultPlatform();
  V8::InitializePlatform(platform);
  V8::Initialize();
}

worker* worker_new(int table_index) {
  worker* w = new (worker);

  Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
      ArrayBuffer::Allocator::NewDefaultAllocator();
  Isolate* isolate = Isolate::New(create_params);
  Locker locker(isolate);
  Isolate::Scope isolate_scope(isolate);
  HandleScope handle_scope(isolate);

  w->isolate = isolate;
  // Leaving this code here because it will probably be useful later on, but
  // disabling it now as I haven't got tests for the desired behavior.
  // w->isolate->SetCaptureStackTraceForUncaughtExceptions(true);
  // w->isolate->SetAbortOnUncaughtExceptionCallback(AbortOnUncaughtExceptionCallback);
  // w->isolate->AddMessageListener(MessageCallback2);
  // w->isolate->SetFatalErrorHandler(FatalErrorCallback2);
  w->isolate->SetPromiseRejectCallback(ExitOnPromiseRejectCallback);
  w->isolate->SetData(0, w);
  w->table_index = table_index;

  Local<ObjectTemplate> global = ObjectTemplate::New(w->isolate);
  Local<ObjectTemplate> v8worker2 = ObjectTemplate::New(w->isolate);

  global->Set(String::NewFromUtf8(w->isolate, "V8Worker2"), v8worker2);

  v8worker2->Set(String::NewFromUtf8(w->isolate, "print"),
                 FunctionTemplate::New(w->isolate, Print));

  v8worker2->Set(String::NewFromUtf8(w->isolate, "recv"),
                 FunctionTemplate::New(w->isolate, Recv));

  v8worker2->Set(String::NewFromUtf8(w->isolate, "send"),
                 FunctionTemplate::New(w->isolate, Send));

  Local<Context> context = Context::New(w->isolate, NULL, global);
  w->context.Reset(w->isolate, context);
  context->Enter();

  return w;
}

void worker_dispose(worker* w) {
  w->isolate->Dispose();
  delete (w);
}

void worker_terminate_execution(worker* w) { w->isolate->TerminateExecution(); }
}
