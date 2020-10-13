#!/bin/bash
echo "Installing dependencies..."
sudo apt update
sudo apt -y install googletest google-mock libgtest-dev libgmock-dev doxygen graphviz
