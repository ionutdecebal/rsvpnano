#!/bin/sh
cd "$(dirname "$0")" || exit 1
python3 convert_books.py
