#!/bin/bash
set -e

echo " Preparing build directory and building project"

# Salvo la directory corrente (dove si lancia lo script)
root_dir=$(pwd)

# La cartella build è dentro root_dir
build_dir="$root_dir/build"

if [ ! -d "$build_dir" ]; then
  echo " Build directory not found, creating and configuring..."
  mkdir "$build_dir"
  cmake -S "$root_dir" -B "$build_dir"
else
  echo " Build directory exists, skipping cmake configuration"
fi

cmake --build "$build_dir"

echo " Build finished."
