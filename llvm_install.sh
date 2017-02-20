#!/bin/bash

echo "Enter the number corresponding to the llvm/clang version to install:"

echo "[1] FreeBSD10 AMD64"
echo "[2] FreeBSD10 i386"
echo "[3] AArch64 Linux"
echo "[4] armv7a Linux"
echo "[5] x86_64 Debian 8"
echo "[6] x86_64 Ubuntu 16.04"
echo "[7] MIPS"
echo "[8] MIPSel"
echo ""
echo ""
echo "Input selection: "

read VERSION

# Unwieldy and long conditional but it's bash so :/
if [ $VERSION == "1" ] 
then
    wget http://releases.llvm.org/3.9.1/clang+llvm-3.9.1-amd64-unknown-freebsd10.tar.xz
elif [ $VERSION == "2" ] 
then
    wget http://releases.llvm.org/3.9.1/clang+llvm-3.9.1-i386-unknown-freebsd10.tar.xz
elif [ $VERSION == "3" ] 
then
    wget http://releases.llvm.org/3.9.1/clang+llvm-3.9.1-aarch64-linux-gnu.tar.xz
elif [ $VERSION == "4" ] 
then
    wget http://releases.llvm.org/3.9.1/clang+llvm-3.9.1-armv7a-linux-gnueabihf.tar.xz
elif [ $VERSION == "5" ] 
then
    wget http://releases.llvm.org/3.9.1/clang+llvm-3.9.1-x86_64-linux-gnu-debian8.tar.xz
elif [ $VERSION == "6" ] 
then
    wget http://releases.llvm.org/3.9.1/clang+llvm-3.9.1-x86_64-linux-gnu-ubuntu-16.04.tar.xz
elif [ $VERSION == "7" ] 
then
    wget http://releases.llvm.org/3.9.1/clang+llvm-3.9.1-mips-linux-gnu.tar.xz
elif [ $VERSION == "8" ] 
then
    wget http://releases.llvm.org/3.9.1/clang+llvm-3.9.1-mipsel-linux-gnu.tar.xz
else 
    echo "Invalid selection $VERSION"
    exit 1
fi

echo "Extracting from tar"
tar xf clang*

cd clang*

echo "Copying to /usr/local/"
sudo cp -R * /usr/local/
