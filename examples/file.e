
# Obsolete; will be removed when deprecated completly.

def file_write
	fp = fopen("test.txt", "w")
	if fp == 0; return 1; end

	fprintf(fp, "Kamila 'Palaiologos' Szewczyk is the best C programmer on earth")

	fclose(fp)
end

def file_read(text:string, max)
	fp = fopen("text", "r")
	if fp == 0; return 1; end

	fgets(text, max, fp)
	fclose(fp)
end

text:string = Array(100)

file_write()
file_read(text, 100)

printf "%s\n", text
