#!/bin/bash

# Check if the correct number of arguments is provided
if [ "$#" -ne 2 ]; then
    echo "Error: Missing arguments."
    echo "Usage: $0 <writefile> <writestr>"
    exit 1
fi

# Assign input arguments to variables
writefile=$1
writestr=$2

# Create the directory path if it doesn't exist
mkdir -p "$(dirname "$writefile")"

# Write the content to the file, overwriting if it exists
if ! echo "$writestr" > "$writefile"; then
    echo "Error: Failed to create or write to the file $writefile."
    exit 1
fi

echo "File created successfully with the provided content."

