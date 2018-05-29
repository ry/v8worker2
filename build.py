#!/usr/bin/env python
import os
import shutil
import stat
import subprocess
import sys
import distutils.spawn
import argparse
import platform

parser = argparse.ArgumentParser(description="v8worker2 build.py")
parser.add_argument('--rebuild', dest='rebuild', action='store_true')
parser.add_argument('--use_ccache', dest='use_ccache', action='store_true')
parser.set_defaults(rebuild=False, use_ccache=False)
args = parser.parse_args()

root_path = os.path.dirname(os.path.realpath(__file__))
prebuilt_path = os.path.join(root_path, "prebuilt")
v8_path = os.path.join(root_path, "v8")
out_path = os.path.join(root_path, "out/")
v8build_path = os.path.join(out_path, "v8build")
depot_tools = os.path.join(root_path, "depot_tools")

# To get a list of args
# (cd v8 && ../depot_tools/gn args ../out/v8build/ --list | vim -)
GN_ARGS = """
  is_component_build=false
  is_debug=false
  libcpp_is_static=false
  symbol_level=1
  use_custom_libcxx=false
  use_sysroot=false
  v8_deprecation_warnings=false
  v8_embedder_string="-v8worker2"
  v8_enable_gdbjit=false
  v8_enable_i18n_support=false
  v8_enable_test_features=false
  v8_experimental_extra_library_files=[]
  v8_extra_library_files=[]
  v8_imminent_deprecation_warnings=false
  v8_monolithic=true
  v8_static_library=false
  v8_target_cpu="x64"
  v8_untrusted_code_mitigations=false
  v8_use_external_startup_data=false
  v8_use_snapshot=true
"""

GCLIENT_SOLUTION = [
  { "name"        : "v8",
    "url"         : "https://chromium.googlesource.com/v8/v8.git",
    "deps_file"   : "DEPS",
    "managed"     : False,
    "custom_deps" : {
      # These deps are unnecessary for building.
      "v8/test/benchmarks/data"               : None,
      "v8/testing/gmock"                      : None,
      "v8/test/mozilla/data"                  : None,
      "v8/test/test262/data"                  : None,
      "v8/test/test262/harness"               : None,
      "v8/test/wasm-js"                       : None,
      "v8/third_party/android_tools"          : None,
      "v8/third_party/catapult"               : None,
      "v8/third_party/colorama/src"           : None,
      "v8/third_party/instrumented_libraries" : None,
      "v8/tools/gyp"                          : None,
      "v8/tools/luci-go"                      : None,
      "v8/tools/swarming_client"              : None,
    },
    "custom_vars": {
      "build_for_node" : True,
    },
  },
]

def main():
  lib_fn = os.path.join(prebuilt_path, platform_name(), "libv8_monolith.a")
  if args.rebuild or not os.path.exists(lib_fn):
    print "Rebuilding V8"
    lib_fn = Rebuild()
  else:
    print "Using prebuilt V8", lib_fn
  WriteProgramConifgFile(lib_fn)

def platform_name():
  u = platform.uname()
  return (u[0] + "-" + u[4]).lower()

def Rebuild():
  env = os.environ.copy()

  EnsureDeps(v8_path)

  gn_path = os.path.join(depot_tools, "gn")
  assert os.path.exists(gn_path)
  ninja_path = os.path.join(depot_tools, "ninja")
  assert os.path.exists(ninja_path)

  gn_args = GN_ARGS.replace('\n', ' ')

  if args.use_ccache:
    ccache_fn = distutils.spawn.find_executable("ccache")
    if ccache_fn:
      gn_args += " cc_wrapper=\"%s\"" % ccache_fn

  print "Running gn"
  subprocess.check_call([gn_path, "gen", v8build_path, "--args=" + gn_args],
                        cwd=v8_path,
                        env=env)
  print "Running ninja"
  subprocess.check_call([ninja_path, "-v", "-C", v8build_path, "v8_monolith"],
                        cwd=v8_path,
                        env=env)
  lib_fn = os.path.join(v8build_path, "obj/libv8_monolith.a")
  return lib_fn

def WriteProgramConifgFile(lib_fn):
  assert os.path.exists(lib_fn)
  if not os.path.isdir(out_path):
    os.makedirs(out_path)

  pc_fn = os.path.join(root_path, "v8.pc")
  include_dir = os.path.join(v8_path, "include")
  with open(pc_fn, 'w+') as f:
    f.write("Name: v8\n")
    f.write("Description: v8\n")
    f.write("Version: xxx\n")
    f.write("Cflags: -I%s\n" % include_dir)
    f.write("Libs: %s\n" % lib_fn)
  print "Wrote " + pc_fn

def EnsureDeps(v8_path):
  # Now call gclient sync.
  spec = "solutions = %s" % GCLIENT_SOLUTION
  print "Fetching dependencies."
  env = os.environ.copy()
  # gclient needs to have depot_tools in the PATH.
  env["PATH"] = depot_tools + os.pathsep + env["PATH"]
  subprocess.check_call(["gclient", "sync", "--spec", spec],
                        cwd=root_path,
                        env=env)

if __name__ == "__main__":
  main()
