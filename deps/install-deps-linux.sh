#!/bin/bash
echo "Installing dependencies..."
sudo apt update
sudo apt -y install googletest libgtest-dev doxygen graphviz
