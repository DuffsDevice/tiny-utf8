@echo off

if not exist "vcpkg" mkdir "vcpkg"
cd vcpkg

git init
git remote add origin https://github.com/microsoft/vcpkg.git
git fetch origin
git checkout -b master --track origin/master

bootstrap-vcpkg.bat
vcpkg integrate install

echo "Installing dependencies..."
vcpkg install gtest

cd ..
