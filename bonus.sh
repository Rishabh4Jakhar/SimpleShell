#!/bin/bash

echo "This code will read the file and is just made for to show bonus (&)"
echo
echo Current working directory: $(pwd)
echo
echo "Enter file name with relative path"
read FILE

file="$FILE"
while read line; do
  echo "$line"
done < "$file"
