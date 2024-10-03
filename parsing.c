
#include "mpc.h"

#ifdef _WIN32

static char buffer[2048];

char* readline(char* prompt) {
	fputs(prompt, stdout);
	fgets(buffer, 2048, stdin);
	char* cpy = malloc(strlen(buffer) + 1);
	strcpy(cpy, buffer);
	cpy[strlen(cpy) - 1] = '\0';
	return cpy;
}

void add_history(char* unused) {}

#else
#include <editline/readline.h>
#include <editline/history.h>
#endif

enum {
	LVAL_ERR,
	LVAL_NUM,
	LVAL_SYM,
	LVAL_SEXPR
};

typedef struct lval {
	int type;
	double num;
	char* err;
	char* sym;
	int count;
	struct lval** cell;
} lval;


void lval_print(lval* v);
lval* lval_eval(lval* v);

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

lval* lval_num(double x) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_NUM;
	v->num = x;
	return v;
}

lval* lval_err(char* m) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_ERR;
	v->err = malloc(strlen(m) + 1);
	strcpy(v->err, m);
	return v;
}

lval* lval_sym(char* s) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_SYM;
	v->sym = malloc(strlen(s) + 1);
	strcpy(v->sym, s);
	return v;
}

lval* lval_sexpr(void) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_SEXPR;
	v->count = 0;
	v->cell = NULL;
	return v;
}

void lval_del(lval* v) {
	
	switch (v->type) {
		case LVAL_NUM: 
			break;

		case LVAL_ERR: 
			free(v->err);
			break;
		case LVAL_SYM:
			free(v->sym);
			break;

		case LVAL_SEXPR: {
			for (int i = 0; i < v->count; i++) {
				lval_del(v->cell[i]);
			}

			free(v->cell);
			break;
		}
	}

	free(v);
}

lval* lval_add(lval* v, lval* x) {
	v->count++;
	v->cell = realloc(v->cell, sizeof(lval*) * v->count);
	v->cell[v->count - 1] = x;
	return v;
}

lval* lval_pop(lval* v, int i) {
	lval* x = v->cell[i];

	memmove(&v->cell[i], &v->cell[i + 1], sizeof(lval*) * (v->count - i - 1));

	v->count--;

	v->cell = realloc(v->cell, sizeof(lval*) * v->count);
	return x;
}

lval* lval_take(lval* v, int i) {
	lval* x = lval_pop(v, i);
	lval_del(v);
	return x;
}

void lval_expr_print(lval* v, char open, char close) {
	putchar(open);
	for (int i = 0; i < v->count; i++) {

		lval_print(v->cell[i]);

		if (i != (v->count - 1)) putchar(' ');
	}
	putchar(close);
}

void lval_print(lval* v) {
	switch (v->type) {
	case LVAL_NUM:
		printf("%.2f", v->num);
		break;
	case LVAL_ERR:
		printf("Error: %s", v->err);
		break;
	case LVAL_SYM:
		printf("%s", v->sym);
		break;
	case LVAL_SEXPR:
		lval_expr_print(v, '(', ')');
		break;
	}
}

void lval_println(lval* v) {
	lval_print(v);
	putchar('\n');
}


lval* builtin_op(lval* a, char* op) {

	for (int i = 0; i < a->count; i++) {
		if (a->cell[i]->type != LVAL_NUM) {
			lval_del(a);
			return lval_err("Cannot operate on non-number!");
		}
	}

	lval* x = lval_pop(a, 0);

	if ((strcmp(op, "-") == 0) && a->count == 0) x->num = -x->num;

	while (a->count > 0) {

		lval* y = lval_pop(a, 0);

		if (strcmp(op, "+") == 0) x->num += y->num;
		if (strcmp(op, "-") == 0) x->num -= y->num;
		if (strcmp(op, "*") == 0) x->num *= y->num;
		if (strcmp(op, "/") == 0) {
			if (y->num == 0) {
				lval_del(x);
				lval_del(y);
				x = lval_err("Division By Zero!");
				break;
			}
			x->num /= y->num;
		}
		if (strcmp(op, "%") == 0) x->num = ((long)x->num % (long)y->num);
		if (strcmp(op, "^") == 0) x->num = power(x->num, y->num);
		if (strcmp(op, "min") == 0) x->num = (x->num < y->num) ? x->num : y->num;
		if (strcmp(op, "max") == 0) x->num = (x->num > y->num) ? x->num : y->num;

		lval_del(y);
	}

	lval_del(a);

	return x;
}


lval* lval_eval_sexpr(lval* v) {

	for (int i = 0; i < v->count; i++) {
		v->cell[i] = lval_eval(v->cell[i]);
	}

	for (int i = 0; i < v->count; i++) {
		if (v->cell[i]->type == LVAL_ERR) return lval_take(v, i);
	}

	if (v->count == 0) return v;

	if (v->count == 1) return lval_take(v, 0);

	lval* f = lval_pop(v, 0);
	if (f->type != LVAL_SYM) {
		lval_del(f);
		lval_del(v);
		return lval_err("S-expression Does not start with symbol");
	}

	lval* result = builtin_op(v, f->sym);
	lval_del(f);
	return result;
}

lval* lval_eval(lval* v) {

	if (v->type == LVAL_SEXPR) return lval_eval_sexpr(v);


	return v;
}

lval* lval_read_num(mpc_ast_t* t) {
	errno = 0;
	double x = atof(t->contents);
	return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval* lval_read(mpc_ast_t* t) {

	if (strstr(t->tag, "number")) return lval_read_num(t);
	if (strstr(t->tag, "symbol")) return lval_sym(t->contents);

	lval* x = NULL;
	if (strcmp(t->tag, ">") == 0) x = lval_sexpr();
	if (strstr(t->tag, "sexpr")) x = lval_sexpr();

	for (int i = 0; i < t->children_num; i++) {
		if (strcmp(t->children[i]->contents, "(") == 0) continue;
		if (strcmp(t->children[i]->contents, ")") == 0) continue;
		if (strcmp(t->children[i]->tag, "regex") == 0) continue;

		x = lval_add(x, lval_read(t->children[i]));
	}

	return x;
}

int main(int argc, char** argv) {
	/* Create some Parsers */
	mpc_parser_t* Number = mpc_new("number");
	mpc_parser_t* Symbol = mpc_new("symbol");
	mpc_parser_t* Sexpr = mpc_new("sexpr");
	mpc_parser_t* Expr = mpc_new("expr");
	mpc_parser_t* Tea = mpc_new("tea");

	/* Define them with the following Language */
	mpca_lang(MPCA_LANG_DEFAULT,
		"                                                                          \
			number   : /-?[0-9]+/ ;                                                \
			symbol   : '+' | '-' | '*' | '/' | '%' | '^' | \"min\" | \"max\";      \
            sexpr    : '(' <expr>* ')' ;                                           \
			expr     :  <number> | <symbol> | <sexpr> ;                            \
			tea      :  /^/ <expr>* /$/ ;                                          \
		",
		Number, Symbol, Sexpr, Expr, Tea);


	/* Print Version and Exit Information */
	puts("Tea Version 0.0.0.0.5");
	puts("made by MrTea");
	puts("Press Ctrl+C to Exit\n");

	/* In a never ending loop */
	while (1) {

		char* input = readline("tea> "); // Output our prompt and get input
		add_history(input); // Add input to history

		/* Attempt to Parse the user Input */
		mpc_result_t r;

		if (mpc_parse("<stdin>", input, Tea, &r)) {
			lval* x = lval_eval(lval_read(r.output));
			mpc_ast_print(r.output);
			lval_println(x);
			printf("leaves = %i\n", countLeaves(r.output));
			printf("branches = %i\n", countBranches(r.output));
			printf("children num = %i\n", max_children(r.output));
			lval_del(x);
			mpc_ast_delete(r.output);
		}
		else {
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}

		free(input); // Free retrieved input

	}

	mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Tea);

	return 0;
}

