# fxgasm.h and fxinline.h are generated as part of the build.
mkdir -p bin
cd bin
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .
