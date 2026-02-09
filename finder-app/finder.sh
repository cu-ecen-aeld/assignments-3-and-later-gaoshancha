#!/bin/sh

if [ $# -ne 2 ]; then
    echo "Error: Two arguments required ($# provided)"
    echo "Usage: $0 <directory-to-search> <search-string>"
    exit 1
fi

filesdir=$1
searchstr=$2

if [ ! -d "$filesdir" ]; then
    echo "Error: $filesdir does not exist and/or is not directory" 
    exit 1
fi

n_files=$(find "$filesdir" -type f | wc -l)
n_lines=$(grep -r $searchstr $filesdir 2>/dev/null | wc -l) 
echo "The number of files are $n_files and the number of matching lines are $n_lines"

#add a comment so that we can re-run the git action runner
