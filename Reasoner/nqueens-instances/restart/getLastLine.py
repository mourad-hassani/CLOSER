import os

def get_last_non_empty_line(file_path):
    with open(file_path, 'r') as f:
        lines = f.readlines()

    for line in reversed(lines):
        if line.strip():  # Check if the line is not empty
            return line.strip()

    return None  # If no non-empty line is found

# Get all files in the current directory that start with 'nqueens-30-' and end with '.lp'
files = [f for f in os.listdir('.') if f.startswith('nqueens-30-') and f.endswith('.lp')]

# Open a text file for writing the output lines
with open('output.txt', 'w') as outfile:
    # Loop through the files and extract the last non-empty line from each file
    for file in files:
        last_line = get_last_non_empty_line(file)
        outfile.write(f"{last_line}\n")

