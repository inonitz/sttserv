#!/bin/bash

export TMPDIR=$PWD/tmp
mkdir -p $TMPDIR

python3 setupvenv.py

if [ -d "venv" ]; then
    source venv/bin/activate
    echo "Virtual environment activated."
fi
