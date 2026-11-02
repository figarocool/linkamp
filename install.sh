#!/usr/bin/env bash

set -euo pipefail
IFS=$'\n\t'

echo "This script re-installs Linkamp executable files in Linkamp OS. This should only be run inside Linkamp OS"

# Assumes the build files are on the same folder as this script
rm -rf ~/linkamp/*
cp -r build ~/linkamp/
cp shutdown.sh ~/linkamp/
cp start.sh ~/linkamp/
cp -r venv ~/linkamp/
cp -r python ~/linkamp/

echo "Done!"
