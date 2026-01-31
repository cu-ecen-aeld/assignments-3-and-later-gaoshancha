#!/bin/bash

if [ $# -ne 2 ]; then
    echo "Error: Two arguments required ($# provided)"
    echo "Usage: $0 <file-to-write> <write-string>"
    exit 1
fi

write_file=$1
write_string=$2
write_dir=$(dirname "$write_file")
filename=$(basename "$write_file")

mkdir -p $write_dir

if [ $? -ne 0 ]; then
    echo "Error: Could not create directory for file to write: $write_dir"
    exit 1
fi

if [ ! -d "$write_dir" ]; then
    echo "Error: $filesdir does not exist and/or is not directory"
    exit 1
fi


echo "$write_string" > "$write_file"

if [ $? -ne 0 ]; then
    echo "Error: Could not create file $write_file"
    exit 1
fi
