
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


enum {
	LERR_DIV_ZERO,
	LERR_BAD_OP,
	LERR_BAD_NUM
};

enum {
	LVAL_NUM,
	LVAL_ERR
};

typedef struct {
	int type;
	double num;
	int err;
} lval;

lval lval_num(double x) {
	lval v;
	v.type = LVAL_NUM;
	v.num = x;
	return v;
}

lval lval_err(int x) {
	lval v;
	v.type = LVAL_ERR;
	v.err = x;
	return v;
}

void lval_print(lval v) {
	switch (v.type) {
	case LVAL_NUM:
		printf("%.2f", v.num);
		break;
	case LVAL_ERR:
		if (v.err == LERR_DIV_ZERO) printf("Error: Division By Zero!");
		if (v.err == LERR_BAD_OP) printf("Error: Invalid Operator!");
		if (v.err == LERR_BAD_NUM) printf("Error: Invalid Number!");
		break;
	}
}

void lval_println(lval v) {
	lval_print(v);
	putchar('\n');
}

long power(double x, double y) {
	double result = x;
	for (int i = 1; i < y; i++) {
		result = result * x;
	}

	return result;
}

int max_children(mpc_ast_t* t) {

	if (t->children_num == 0) {
		return 0;
	}

	// Start by assuming the current node has the maximum children
	int max_count = t->children_num;

	// Recursively check the children
	for (int i = 0; i < t->children_num; i++) {
		int child_max = max_children(t->children[i]);
		if (child_max > max_count) {
			max_count = child_max;
		}
	}

	return max_count;
}

int countBranches(mpc_ast_t* t) {

	if (t->children_num == 0) return 0;

	int branch_count = 1;
	for (int i = 0; i < t->children_num; i++) {
		branch_count = branch_count + countBranches(t->children[i]);
	}

	return branch_count;
}

int countLeaves(mpc_ast_t* t) {

	if (t->children_num == 0) return 1;

	int leaf_count = 0;
	for (int i = 0; i < t->children_num; i++) {
		leaf_count = leaf_count + countLeaves(t->children[i]);
	}

	return leaf_count;
}

lval eval_op(lval x, char* op, lval y) {

	if (x.type == LVAL_ERR) return x;
	if (y.type == LVAL_ERR) return y;

	if (strcmp(op, "+") == 0) return lval_num(x.num + y.num);
	if (strcmp(op, "-") == 0) return lval_num(x.num - y.num);
	if (strcmp(op, "*") == 0) return lval_num(x.num * y.num);
	if (strcmp(op, "/") == 0) return y.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(x.num / y.num);
	if (strcmp(op, "%") == 0) return lval_num(((long)x.num % (long)y.num));
	if (strcmp(op, "^") == 0) return lval_num(power(x.num, y.num));
	if (strcmp(op, "min") == 0) return (x.num < y.num) ? x : y;
	if (strcmp(op, "max") == 0) return (x.num > y.num) ? x : y;

	return lval_err(LERR_BAD_OP);
}

lval eval(mpc_ast_t* t) {

	mpc_ast_print(t);

	/* If tagged as number return it directly. */
	if (strstr(t->tag, "number")) {
		errno = 0;
		double x = atof(t->contents);
		return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
	}

	char* op = t->children[1]->contents;
	lval x = eval(t->children[2]);

	int i = 3;
	while (strstr(t->children[i]->tag, "expr")) {
		x = eval_op(x, op, eval(t->children[i]));
		i++;
	}

	return x;
}

int main(int argc, char** argv) {
	/* Create some Parsers */
	mpc_parser_t* Number = mpc_new("number");
	mpc_parser_t* Operator = mpc_new("operator");
	mpc_parser_t* Expr = mpc_new("expr");
	mpc_parser_t* Tea = mpc_new("tea");

	/* Define them with the following Language */
	mpca_lang(MPCA_LANG_DEFAULT,
		"                                                                          \
			number   : /-?[0-9]+.?[0-9]*/ ;                                                \
			operator : '+' | '-' | '*' | '/' | '%' | '^' | \"min\" | \"max\";      \
			expr     :  <number> | '(' <operator> <expr>+ ')' ;                    \
			tea      :  /^/ <operator> <expr>+ /$/ ;                               \
		",
		Number, Operator, Expr, Tea);


	/* Print Version and Exit Information */
	puts("Tea Version 0.0.0.0.4");
	puts("made by MrTea");
	puts("Press Ctrl+C to Exit\n");

	/* In a never ending loop */
	while (1) {

		char* input = readline("tea> "); // Output our prompt and get input
		add_history(input); // Add input to history

		/* Attempt to Parse the user Input */
		mpc_result_t r;
		if (mpc_parse("<stdin>", input, Tea, &r)) {
			/* On Success Print the AST */
			lval result = eval(r.output);
			lval_println(result);
			printf("leaves = %i\n", countLeaves(r.output));
			printf("branches = %i\n", countBranches(r.output));
			printf("children num = %i\n", max_children(r.output));

			mpc_ast_delete(r.output);

		}
		else {
			/* Otherwise Print the Error */
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}


		free(input); // Free retrieved input

	}

	mpc_cleanup(4, Number, Operator, Expr, Tea);

	return 0;
}

