#include "stdarg.h"
#include "stdbool.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

typedef struct {
	char *infn;
	char *outfn;
	bool winMode;
} brainfucc_flags;

typedef struct {
	char *data;
	long capacity;
	long size;
} array;

array *append (array *, int, ...);
char *compile (const char * const, const long, bool);
char *itoa (const int);
brainfucc_flags ParseArgs(int argc, char **argv);

int main (int argc, char **argv) {
	brainfucc_flags flags = ParseArgs(argc, argv);
	FILE *f = fopen(flags.infn, "r");
	if (f == NULL) {
		printf("Error opening file: %s\n", flags.infn);
		return 1;
	}
	//count number of bytes in file
	fseek(f, 0L, SEEK_END);
	long l = ftell(f);
	char *b = malloc(l);
	//return to beginning of file
	rewind(f);
	if (b != NULL) {
		fread(b, l, 1, f);
	} else {
		printf("Malloc error: %s may be empty.\n", flags.infn);
		return 1;
	}
	fclose(f);
	f = NULL;

	FILE *outf = fopen(flags.outfn, "w");
	if (outf == NULL) {
		printf("Error writing to file.\n");
		return 1;
	}
	fprintf(outf, "%s", compile(b, l, flags.winMode));
	fclose(outf);
	free(b);
	return 0;
}

brainfucc_flags ParseArgs(int argc, char **argv) {
	brainfucc_flags ret = {"no input file given", "a.s", false};
	for (int i = 0; i < argc; i++) {
		if (strcmp("-o", argv[i]) == 0) {
			ret.outfn = argv[++i];
		} else if (strcmp("-w", argv[i]) == 0 ||
			   strcmp("-windows", argv[i]) == 0) {
			ret.winMode = true;
		} else {
			ret.infn = argv[i];
		}
	}
	return ret;
}

//Compiles the brainfuck program stored in "in". "in" has length "l".
//Characters that aren't part of the brainfuck language are simply ignored.
char *compile (const char * const in, const long l, bool winMode) {
	array output = {NULL, 1024, 0};
	output.data = malloc(output.capacity);

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

	//The first integer argument register: On Windows this is rcx, almost
	//everywhere else that I know of this is rdi.
	char *arg0reg = winMode ? "%rcx" : "%rdi";
	//The first byte argument register.
	char *arg0regb = winMode ? "%cl" : "%dil";


	append(&output, 5, ".global main\n"
			"main:\n\t"
		      	"mov $30000, ", arg0reg, "\n\t"
		      	"call malloc\n\t"
			"mov %rax, ", arg0reg, "\n\t");
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
				append(&output, 5, "add $", seqs, ", ", arg0reg, "\n\t");
			} else if (lastsymbol == '<') {
				append(&output, 5, "sub $", seqs, ", ", arg0reg, "\n\t");
			} else if (lastsymbol == '+') {
				append(&output, 5, "addb $", seqs, ", (", arg0reg, ")\n\t");
			} else if (lastsymbol == '-') {
				append(&output, 5, "subb $", seqs, ", (", arg0reg, ")\n\t");
			}
			free(seqs);
		}
		
		if (currsymbol == '.') {
			append(&output, 9, "push ", arg0reg, "\n\t"
					   "movb (", arg0reg, "), ", arg0regb, "\n\t"
					   "call putchar\n\t"
					   "pop ", arg0reg, "\n\t");
		} else if (currsymbol == ',') {
			append(&output, 7, "push ", arg0reg, "\n\t"
					   "call getchar\n\t"
					   "pop ", arg0reg, "\n\t"
					   "movb %al, (", arg0reg, ")\n\t");
		} else if (currsymbol == '[') {
			if (in[i+1] == '-' && in[i+2] == ']') {
				//This is a common pattern to clear the memory
				//pointed to.
				append(&output, 3, "movb $0, (", arg0reg, ")\n\t");
				i += 2;
			} else {
				int bnum = cb;
				if (bi > blen) {
					blen *= 2;
					bnums = realloc(bnums, blen);
				}
				bnums[bi++] = cb++;
				char *bnumstring = itoa(bnum);
				append(&output, 7, "l", bnumstring, ":\n\t"
						   "cmpb $0, (", arg0reg, ")\n\t"
						   "je l", bnumstring, "e\n\t");
				free(bnumstring);
			}
		} else if (currsymbol == ']') {
			if (bi < 0) {
				printf("Error: Unbalanced brackets.\n");
				exit(1);
			}
			int bnum = bnums[--bi];
			char *bnumstring = itoa(bnum);
			append(&output, 7, "cmpb $0, (", arg0reg, ")\n\t"
					   "jne l", bnumstring, "\n\t"
					   "l", bnumstring, "e:\n\t");
			free(bnumstring);
		}
		lastsymbol = currsymbol;
	}

	append(&output, 1, "\0");
	free(bnums);
	return output.data;
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
	//muh NULL terminator
	b[l] = 0;
	return realloc(b, l);
}

//All varargs must be char * to null-terminated strings to append to a.
//elems is the number of varargs given.
array *append(array *a, int elems, ...) {
	va_list ap;
	va_start(ap, elems);

	for (int i = 0; i < elems; i++) {
		char *after = va_arg(ap, char *);
		for (int j = 0; after[j] != '\0'; j++) {
			if (a->size == a->capacity) {
				a->capacity = 2*(a->capacity);
				a->data = realloc(a->data, a->capacity);
			}
			a->data[a->size] = after[j];
			a->size += 1;
		}
	}
	va_end(ap);
	return a;
}
