#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#define null 0

char *append (char *, long * const, long * const, const char * const);
char *compile (const char * const, const long);
char *itoa (const int);

int main (int argc, char **argv) {
	if (argc < 2 || argc == 3 || (argc == 4 && strcmp(argv[2], "-o"))) {
		printf("Usage: %s filename\n [-o outname]", argv[0]);
		return 1;
	}
	FILE *f = fopen(argv[1], "r");
	if (f == null) {
		printf("Error opening file: %s\n", argv[1]);
		return 1;
	}
	//count number of bytes in file
	fseek(f, 0L, SEEK_END);
	long l = ftell(f);
	char *b = malloc(l);
	//return to beginning of file
	rewind(f);
	if (b != null) {
		fread(b, l, 1, f);
	} else {
		printf("Malloc error: %s may be empty.\n", argv[1]);
		return 1;
	}
	fclose(f);
	f = null;

	char *outfn = "a.s";
	if (argc == 4) {
		outfn = argv[3];	
	}
	FILE *outf = fopen(outfn, "w");
	if (outf == null) {
		printf("Error writing to file.\n");
		return 1;
	}
	fprintf(outf, "%s", compile(b, l));
	fclose(outf);
	free(b);
	return 0;
}

//Compiles the brainfuck program stored in "in". "in" has length "l".
//Characters that aren't part of the brainfuck language are simply ignored.
char *compile (const char * const in, const long l) {
	long len = 1024;
	//output buffer
	char *b = malloc(len);
	//current index in the output buffer
	long p = 0;

	//bnums is a basic stack keeping track of the level of indentation the
	//user is on. This is used to name the labels when brackets are used.
	int blen = 10;
	int *bnums = malloc(blen*sizeof(int));
	//current index in the stack
	int bi = 0;
	//number on current label
	int cb = 0;

	//the last symbol read in the input buffer
	char lastsymbol = '\0';
	//the number of equal symbols read in a row
	int seq = 0;

	
	b = append(b, &p, &len, ".global main\n"
				"main:\n\t"
		      		"mov $30000, %rdi\n\t"
		      		"call malloc\n\t"
				"mov %rax, %rdi\n\t");
	for (int i = 0; i < l; i++) {		
		char currsymbol;
		switch(in[i]) {
		case '+':
		case '-':
		case '>':
		case '<':
		case '.':
		case ',':
		case '[':
		case ']':
			currsymbol = in[i];
			break;
		default:
			continue;
		}

		if (lastsymbol == currsymbol) {
			seq++;
		} else {	
			char *seqs = itoa(seq);
			seq = 1;
			if (lastsymbol == '>') {
				b = append(b, &p, &len, "add $");
				b = append(b, &p, &len, seqs);
				b = append(b, &p, &len, ", %rdi\n\t");
			} else if (lastsymbol == '<') {
				b = append(b, &p, &len, "sub $");
				b = append(b, &p, &len, seqs);
				b = append(b, &p, &len, ", %rdi\n\t");
			} else if (lastsymbol == '+') {
				b = append(b, &p, &len, "addb $");
				b = append(b, &p, &len, seqs);
				b = append(b, &p, &len, ", (%rdi)\n\t");
			} else if (lastsymbol == '-') {
				b = append(b, &p, &len, "subb $");
				b = append(b, &p, &len, seqs);
				b = append(b, &p, &len, ", (%rdi)\n\t");
			}
			free(seqs);
		}
		
		if (currsymbol == '.') {
			b = append(b, &p, &len, "push %rdi\n\t"
						"movb (%rdi), %dil\n\t"
						"call putchar\n\t"
						"pop %rdi\n\t");
		} else if (currsymbol == ',') {
			b = append(b, &p, &len, "push %rdi\n\t"
						"call getchar\n\t"
						"pop %rdi\n\t"
						"movb %rax, (%rdi)\n\t");
		} else if (currsymbol == '[') {
			int bnum = cb;
			if (bi > blen) {
				blen *= 2;
				bnums = realloc(bnums, blen);
			}
			bnums[bi++] = cb++;
			char *bnumstring = itoa(bnum);
			b = append(b, &p, &len, "l");
			b = append(b, &p, &len, bnumstring);
			b = append(b, &p, &len, ":\n\t"
						"cmpb $0, (%rdi)\n\t"
						"je l");
			b = append(b, &p, &len, bnumstring);
			b = append(b, &p, &len, "e\n\t");
			free(bnumstring);
		} else if (currsymbol == ']') {
			if (bi < 0) {
				printf("Error: Unbalanced brackets.\n");
				exit(1);
			}
			int bnum = bnums[--bi];
			char *bnumstring = itoa(bnum);
			b = append(b, &p, &len,	"cmpb $0, (%rdi)\n\t"
						"jne l");
			b = append(b, &p, &len, bnumstring);
			b = append(b, &p, &len, "\n\t");
			b = append(b, &p, &len, "l");
			b = append(b, &p, &len, bnumstring);
			b = append(b, &p, &len, "e:\n\t");
			free(bnumstring);
		}
		lastsymbol = currsymbol;
	}

	b = append(b, &p, &len, "\0");
	free(bnums);
	return b;
}

//Converts an integer to a string.
char *itoa(const int i) {
	//If you manage to create an int longer than 20 characters you can 
	//probably write your own compiler.
	int l = -1;
	int bl = 21;
	char *b;
	while (l < 0) {
		b = malloc(bl);
		l = snprintf(b, 20, "%d", i);
		bl *= 2;
	}
	//muh null terminator
	b[l] = 0;
	return realloc(b, l);
}

//Safely appends "after" to "s", where p is the current offset from s and l is
//the current length of the buffer s. Reallocs if len(s+after) > maxlen(s)
char *append (char *s, long * const p, long * const l, const char * const after) {
	for (int i = 0; after[i] != '\0'; i++) {
		if (*p == *l) {
			*l = 2*(*l);
			s = realloc(s, *l);
		}
		s[*p] = after[i];
		*p = *p + 1;
	}
	return s;
}
