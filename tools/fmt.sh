#!/bin/sh
set -e
cd `dirname "$0"`/..
clang-format -style=Google -i binding.cc binding.h
go fmt
