
#!/bin/bash

# Input file
input_file="output.txt"

# Create a temporary directory for output files
temp_dir=$(mktemp -d)

# Initialize counters
file_counter=0
line_counter=0
current_file=""

# Function to process each generated file
process_file() {
  local file=$1
  grep "core" "$file" | cut -d':' -f2 | paste -sd "," -
}

# Read the input file line by line
while IFS= read -r line
do
  # Check if the line contains the word "case"
  if [[ $line == *"case"* ]]; then
    # If it's not the first "case", process the previous file
    if [[ $line_counter -ne 0 ]]; then
      process_file "$current_file"
    fi

    # Increment the file counter and create a new file in the temp directory
    file_counter=$((file_counter + 1))
    file_name=$(echo "$line" | tr -d '[:space:]')
    current_file="${temp_dir}/${file_name}_${file_counter}.txt"

    # Reset the line counter for the new file
    line_counter=0
  fi

  # Write the line to the current file
  echo "$line" >> "$current_file"
  line_counter=$((line_counter + 1))

done < "$input_file"

# Process the last file
process_file "$current_file"

# Optional: Print the path of the temporary directory for user reference
echo "Output files are located in: $temp_dir"
