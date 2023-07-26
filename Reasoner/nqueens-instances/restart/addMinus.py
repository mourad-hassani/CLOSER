# Open the input and output files
inlines=[]
with open("input.txt", "r") as infile, open("output.txt", "w") as outfile:
	lines=infile.readlines()
	for inline in lines:
		inwords = inline.strip().split()
		inlines.append(inwords)
with open("output.txt", "w") as outfile:
	outfile.write(" ".join(inlines[0])+"\n")
	for i in range(len(inlines)-1):
		missing_words = set(inlines[i]) - set(inlines[i+1])
		missing_words_minus = [str(missing_word)+"-" for missing_word in missing_words]
		outfile.write(" ".join(inlines[i+1])+" "+" ".join(missing_words_minus)+"\n")
