import random

# Open the input and output files
with open("output.txt", "r") as infile, open("output.lp", "w") as outfile:
    # Read the input file into a list of lines
    lines = infile.readlines()

    # Shuffle and write the lines 10 times
    for i in range(1000):
        random.shuffle(lines)
        for line in lines:
            outfile.write(line)
