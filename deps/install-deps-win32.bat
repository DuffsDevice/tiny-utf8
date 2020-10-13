@echo off
echo "Installing dependencies..."

if not exist "vcpkg" mkdir "vcpkg"
cd vcpkg

git init
git remote add origin https://github.com/microsoft/vcpkg.git
git fetch origin
git checkout -b master --track origin/master

cmd /C bootstrap-vcpkg.bat

vcpkg.exe install gtest:x86-windows gtest:x64-windows

cd ..
