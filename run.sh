#!/bin/bash

# =============================
# IF YOU WANT DEBUG SET TO TRUE

DEBUG=false
# =============================

# Compile the code
rm -rf build/
mkdir build
cd build

# if debug is true
if [ "$DEBUG" = true ]; then
    ../cmake/bin/cmake -DENABLE_DEBUG=ON ..
else
    ../cmake/bin/cmake -DENABLE_DEBUG=OFF ..
fi

make
# List all .c files in the tests directory
for file in ../tests/*.c; do
    # Get the filename without the extension
    filename=$(basename -- "$file")
    filename="${filename%.*}"
    # Run the test
    ./absint "../tests/${filename}.c"
done
