#!/bin/bash

# Check if a filename was provided as an argument
if [ $# -ne 1 ]; then
    echo "Usage: $0 <input_file.s>"
    exit 1
fi

# Extract filename and extension
filename=$(basename -- "$1")
extension="${filename##*.}"
filename_noext="${filename%.*}"

# Check if the file is a .s file
if [ "$extension" != "s" ]; then
    echo "Error: Input file must have a .s extension"
    exit 1
fi

# Find the line number of the last occurrence of the fence instruction
last_fence_line=$(grep -n 'fence' "$1" | cut -d':' -f1 | tail -n1)
last_fence_line=$((last_fence_line+1))

# Find the line number of the .size directive
size_line=$(grep -n '\.size' "$1" | cut -d':' -f1 | tail -n1)
size_line=$((size_line-1))

echo $size_line

# Check if both fence and .size directives were found
if [ -z "$last_fence_line" ] || [ -z "$size_line" ]; then
    echo "Error: Could not find fence instruction or .size directive in the input file"
    exit 1
fi

# Extract the lines between the last fence instruction and the .size directive
output_file="program.s"
awk "NR >= $last_fence_line && NR <= $size_line" "$1" > "../rv32-benchmarks-ibex/$output_file"

echo "Extraction completed. Output saved to $output_file"

rm -f ../rv32-benchmarks-ibex/program.x

(cd ../rv32-benchmarks-ibex && make fix replace assemble)


input_file="../rv32-benchmarks-ibex/program.x"
output_file="pmu_ispm.h"

rm pmu_ispm.h

echo -n "#define ISPM_DATA " >> "$output_file"


# Use awk to add backslash at the end of each line except second-to-last
awk 'NR > 1 { print line } { line = $0 " \\" } END { printf "%s", line }' "$input_file" >> "$output_file"

sed -i '$ s/.$//' "$output_file"

echo "Conversion complete. Output file saved as $output_file"