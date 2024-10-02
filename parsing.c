
#include <stdio.h>
#include <stdlib.h>

#include "mpc.h"

/* If we are compiling on Windows compile these functions */
#ifdef _WIN32
#include <string.h>

static char buffer[2048];

/* Fake readline function */
char* readline(char* prompt) {
	fputs(prompt, stdout);
	fgets(buffer, 2048, stdin);
	char* cpy = malloc(strlen(buffer) + 1);
	strcpy(cpy, buffer);
	cpy[strlen(cpy) - 1] = '\0';
	return cpy;
}

/* Fake add_history function */
void add_history(char* unused) {}

/* Otherwise include the editline headers */
#else
#include <editline/readline.h>
#include <editline/history.h>
#endif

int main(int argc, char** argv) {
	/* Create some Parsers */
	mpc_parser_t* Number = mpc_new("number");
	mpc_parser_t* Operator = mpc_new("operator");
	mpc_parser_t* Expr = mpc_new("expr");
	mpc_parser_t* Tea = mpc_new("tea");

	/* Define them with the following Language */
	mpca_lang(MPCA_LANG_DEFAULT,
		"                                                       \
			number   : /-?[0-9]+/ ;                             \
			operator : '+' | '-' | '*' | '/' ;                  \
			expr     :  <number> | '(' <operator> <expr>+ ')' ; \
			tea      :  /^/ <operator> <expr>+ /$/ ;            \
		",
		Number, Operator, Expr, Tea);


	/* Print Version and Exit Information */
	puts("Tea Version 0.0.0.0.1");
	puts("made by MrTea");
	puts("Press Ctrl+C to Exit\n");

	/* In a never ending loop */
	while (1) {

		char* input = readline("tea> "); // Output our prompt and get input
		add_history(input); // Add input to history

		printf("No you're a mrtea %s\n", input); // Echo input back to user
		free(input); // Free retrieved input

	}

	mpc_cleanup(4, Number, Operator, Expr, Tea);

	return 0;
}

