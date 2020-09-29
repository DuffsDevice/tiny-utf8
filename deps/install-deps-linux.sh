#!/bin/bash
echo "Installing dependencies..."
apt update
apt -y install googletest libgtest-dev doxygen graphviz
