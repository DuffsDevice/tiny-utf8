#!/bin/bash
echo "Installing dependencies..."

mkdir -p googletest
cd googletest

git init
git remote add origin https://github.com/google/googletest
git fetch origin
git checkout -b master --track origin/master

mkdir -p build
cd build

cmake -DCMAKE_CXX_STANDARD=17 ..

make
make install

cd ../..
