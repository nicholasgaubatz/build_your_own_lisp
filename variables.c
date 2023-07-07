// Reference: https://buildyourownlisp.com/chapter9_s_expressions

#include "mpc.h"

#ifdef _WIN32

static char buffer[2048];

char* readline(char* prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char* cpy = malloc(strlen(buffer)+1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy)-1] = '\0';
  return cpy;
}

void add_history(char* unused) {}

#else
#include <editline/readline.h>
#include <editline/history.h>
#endif

/* Forward delcarations */
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

/* Lisp value */
enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR };

/* Array of builtins and current number of builtins (global, not sure if this is good practice) */
char** builtins;
int builtins_count = 0;

/* Global variable for while loop (not sure if good practice) */
int while_var = 1;

typedef lval*(*lbuiltin)(lenv*, lval*);

/* Struct declarations */
// TODO: organize stuff

/* Declare new lval struct */
struct lval {
    int type;

    long num;
    /* Error and Symbol types have some string data */
    char* err;
    char* sym;
    lbuiltin fun;

    /* Count and Pointer to a list of "lval*" */
    int count;
    lval** cell;
};

/* Declare new lenv (Lispy environment) struct */
struct lenv {
    int count;
    char** syms;
    lval** vals;
};

/* Constructors */

/* Construct a pointer to a new Number lval */
lval* lval_num(long x){
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

/* Construct a pointer to a new Error lval */
lval* lval_err(char* fmt, ...){
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;

    /* Create a va list and initialize it */
    va_list va;
    va_start(va, fmt);

    /* Allocate 512 bytes of space */
    v->err = malloc(512);

    /* Print the error string with a maximum of 511 characters */
    vsnprintf(v->err, 511, fmt, va);

    /* Reallocate to number of bytes actually used */
    v->err = realloc(v->err, strlen(v->err)+1);

    /* Cleanup our va list */
    va_end(va);

    return v;
}

/* Construct a pointer to a new Symbol lval */
lval* lval_sym(char* s){
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

/* Construct a pointer to a new Function lval */
lval* lval_fun(lbuiltin func){
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->fun = func;
    return v;
}

/* Construct a pointer to a new Sexpr lval */
lval* lval_sexpr(void){
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

/* Construct a pointer to a new Qexpr lval */
lval* lval_qexpr(void){
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

/* Construct a new lenv */
lenv* lenv_new(void){
    lenv* e = malloc(sizeof(lenv));
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

/* Deletion and addition */

/* Delete an lval */
void lval_del(lval* v){
    switch(v->type){
        /* Do nothing special for number or function types */
        case LVAL_NUM: break;
        case LVAL_FUN: break;

        /* For Err or Sym free the string data */
        case LVAL_ERR:
            free(v->err);
            break;
        case LVAL_SYM:
            free(v->sym);
            break;

        /* For Sexpr or Qexpr delete all elements inside */
        case LVAL_QEXPR:
        case LVAL_SEXPR:
            for(int i=0; i<v->count; i++){
                lval_del(v->cell[i]);
            }
            /* Also free memory allocated to contain the pointers */
            free(v->cell);
            break;
    }

    /* Free the memory allocated for the "lval" struct itself */
    free(v);
}

/* Delete an lenv */
void lenv_del(lenv* e){
    for(int i=0; i<e->count; i++){
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }
    free(e->syms);
    free(e->vals);
    free(e);
}

lval* lval_add(lval* v, lval* x){
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count - 1] = x;
    return v;
}

/* Print */

void lval_print(lval* v);

void lval_expr_print(lval* v, char open, char close){
    putchar(open);

    for(int i=0; i < v->count; i++){
        /* Print value contained within */
        lval_print(v->cell[i]);

        /* Don't print trailing space if last element */
        if(i < v->count - 1){
            putchar(' ');
        }
    }

    putchar(close);
}

/* Print an "lval" */
void lval_print(lval* v){
    switch(v->type){
        case LVAL_NUM:
            printf("%li", v->num);
            break;
        case LVAL_ERR:
            printf("Error: %s", v->err);
            break;
        case LVAL_SYM:
            printf("%s", v->sym);
            break;
        case LVAL_FUN:
            printf("<function>");
            break;
        case LVAL_SEXPR:
            lval_expr_print(v, '(', ')');
            break;
        case LVAL_QEXPR:
            lval_expr_print(v, '{', '}');
            break;
    }
}

/* Print an "lval" followed by a newline */
void lval_println(lval* v){
    lval_print(v);
    putchar('\n');
}

/* Print all named values in an "lenv" */
void lenv_print(lenv* e){
    for(int i=0; i<e->count; i++){
        if(strstr(e->syms[i], "%")){
            printf("%%"); // TODO: fix this, may give error if variable name contains '%'
        } else{
            printf(e->syms[i]);
        }
        putchar('\n');
    }
}

/* Create a new copy of an lval */
lval* lval_copy(lval* v){
    lval* x = malloc(sizeof(lval));
    x->type = v->type;

    switch(v->type){
        /* Copy functions and numbers directly */
        case LVAL_FUN:
            x->fun = v->fun;
            break;
        case LVAL_NUM:
            x->num = v->num;
            break;

        /* Copy strings using malloc and strcpy */
        case LVAL_ERR:
            x->err = malloc(sizeof(v->err) + 1);
            strcpy(x->err, v->err);
            break;
        case LVAL_SYM:
            x->sym = malloc(sizeof(v->sym) + 1);
            strcpy(x->sym, v->sym);
            break;

        /* Copy lists by copying each sub-expression */
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x->count = v->count;
            x->cell = malloc(sizeof(lval*) * x->count);
            for(int i=0; i<x->count; i++){
                x->cell[i] = lval_copy(v->cell[i]);
            }
            break;
    }

    return x;
}

/* Removes item i from the lval and returns it */
lval* lval_pop(lval* v, int i){
    /* Find the item at i */
    lval* x = v->cell[i];

    /* Shift memory after the item at "i" over the top */
    memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count - i - 1)); // Copies n characters from str2 to str1; void *memmove(void *_DST, const void *_SRC, size_t n)

    /* Decrease the count of items in the list */
    v->count--;

    /* Reallocate the memory used */
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);

    return x;
}

/* Returns item i from the lval and deletes the lval */
lval* lval_take(lval* v, int i){
    lval* x = lval_pop(v, i);
    lval_del(v);
    return x;
}

/* Gets symbol from environment */
lval* lenv_get(lenv* e, lval* k){
    /* Iterate over all items in the environment */
    for(int i=0; i<e->count; i++){
        /* Check if the stored string matches the symbol string. If it does, return a copy of the value */
        if(strcmp(e->syms[i], k->sym)==0){
            return lval_copy(e->vals[i]);
        }
    }
    /* If no symbol found, return error */
    return lval_err("Unbound symbol '%s'", k->sym);
}

/* Put a new variable into the environment */
void lenv_put(lenv* e, lval* k, lval* v){
    /* See if the variable already exists */
    for(int i=0; i<e->count; i++){
        /* If variable is found, delete item at that position and replace with variable supplied by user */
        if(strcmp(e->syms[i], k->sym)==0){
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return;
        }
    }

    /* If no existing entry is found, allocate space for new entry */
    e->count++;
    e->vals = realloc(e->vals, sizeof(lval*) * e->count);
    e->syms = realloc(e->syms, sizeof(char*) * e->count);

    /* Copy contents of lval and symbol string into new location */
    e->vals[e->count-1] = lval_copy(v);
    e->syms[e->count-1] = malloc(strlen(k->sym) + 1);
    strcpy(e->syms[e->count-1], k->sym);
}

/* Builtins */

/* Takes type enumeration as input and returns string representation */
char* ltype_name(int t){
    switch(t){
        case LVAL_NUM: return "Number";
        case LVAL_ERR: return "Error";
        case LVAL_SYM: return "Symbol";
        case LVAL_FUN: return "Function";
        case LVAL_SEXPR: return "S-Expression";
        case LVAL_QEXPR: return "Q-Expression";
    }
}

/* Macro to make error messages easier */
#define LASSERT(args, cond, fmt, ...) \
    if (!(cond)) { \
        lval* err = lval_err(fmt, ##__VA_ARGS__); \
        lval_del(args); \
        return err; \
    }

/* Macro for testing for the incorrect number of arguments */
#define INCARGS(args, num, func) \
    if (args->count != num) { \
        lval* err = lval_err("Function '%s' passed incorrect number of arguments. Got %i, expected %i.", func, args->count, num); \
        lval_del(args); \
        return err; \
    }

/* Macro for testing whether i-th lval is of the correct type */
#define INCTYPE(args, i, typ, func) \
    if (args->cell[0]->type != typ) { \
        lval* err = lval_err("Function '%s' passed incorrect type for argument 0. Got %s, expected %s.", func, ltype_name(args->cell[0]->type), ltype_name(typ)); \
        lval_del(args); \
        return err; \
    }

/* Macro for testing for being called with the empty list */
#define EMPLST(args, func) \
    if (args->cell[0]->count == 0) { \
        lval* err = lval_err("Function '%s' passed empty list {}.", func); \
        lval_del(args); \
        return err; \
    }

int power(int x, int y){
    if(y==0){
        return 1;
    }
    else{
        return x * pow(x,y-1);
    }
}

lval* builtin_op(lenv* e, lval* a, char* op){
    /* Ensure all arguments are numbers */
    for(int i=0; i<a->count; i++){
        INCTYPE(a, i, LVAL_NUM, op);
    }

    /* Pop the first element */
    lval* x = lval_pop(a, 0);

    /* If no arguments and sub, perform unary negation */
    if(strcmp(op, "-")==0 && a->count==0){
        x->num = -x->num;
    }

    /* While there are still elements remaining */
    while(a->count > 0){
        /* Pop the next element */
        lval* y = lval_pop(a, 0);

        if(strcmp(op, "+")==0) { x->num += y->num; }
        if(strcmp(op, "-")==0) { x->num -= y->num; }
        if(strcmp(op, "*")==0) { x->num *= y->num; }
        if(strcmp(op, "/")==0) {
            /* If second operand is zero return error */
            if(y->num==0){
                lval_del(x);
                lval_del(y);
                x = lval_err("Division by zero!");
                break;
            }
            x->num /= y->num;
        }
        if(strcmp(op, "%")==0) { x->num %= y->num; }
        if(strcmp(op, "^")==0) { // Computes powers using recursion, but only if exponent is nonnegative. Also truncates noninteger exponents
            if(y->num<0){
                lval_del(x);
                lval_del(y);
                x = lval_err("Invalid number!");
                break;
            }
            x->num = power(x->num, (int) y->num);
        }
        if(strcmp(op, "min")==0) {
            int v = x->num;
            int w = y->num;
            x->num = v*(v<=w) + w*(v>w); // A fun little way to get the minimum of two numbers :)
        }
        if(strcmp(op, "max")==0) {
            int v = x->num;
            int w = y->num;
            y->num = v*(v>=w) + w*(v<w);
        }

        lval_del(y);
    }

    lval_del(a);
    return x;
}

lval* builtin_add(lenv* e, lval* a){
    return builtin_op(e, a, "+");
}

lval* builtin_sub(lenv* e, lval* a){
    return builtin_op(e, a, "-");
}

lval* builtin_mul(lenv* e, lval* a){
    return builtin_op(e, a, "*");
}

lval* builtin_div(lenv* e, lval* a){
    return builtin_op(e, a, "/");
}

lval* builtin_mod(lenv* e, lval* a){
    return builtin_op(e, a, "%");
}

lval* builtin_pow(lenv* e, lval* a){
    return builtin_op(e, a, "^");
}

lval* builtin_min(lenv* e, lval* a){
    return builtin_op(e, a, "min");
}

lval* builtin_max(lenv* e, lval* a){
    return builtin_op(e, a, "max");
}

/* Takes a Q-Expression and returns a Q-Expression with only the first element */
lval* builtin_head(lenv* e, lval* a){
    /* Check error conditions */
    INCARGS(a, 1, "head");
    INCTYPE(a, 0, LVAL_QEXPR, "head");
    EMPLST(a, "head");

    /* Otherwise take first argument */
    lval* v = lval_take(a, 0);

    /* Delete all elements that are not head and return */
    while (v->count > 1){
        lval_del(lval_pop(v, 1));
    }

    return v;
}

/* Takes a Q-Expression and returns a Q-Expression with the first element removed */
lval* builtin_tail(lenv* e, lval* a){
    /* Check error conditions */
    INCARGS(a, 1, "tail");
    INCTYPE(a, 0, LVAL_QEXPR, "tail");
    EMPLST(a, "tail");

    /* Otherwise take all but first argument */
    lval* v = lval_take(a, 0);
    lval_del(lval_pop(v, 0));

    return v;
}

/* Takes one or more arguments and returns a new Q-Expression containing the arguments */
lval* builtin_list(lenv* e, lval* a){
    a->type = LVAL_QEXPR;
    return a;
}

lval* lval_eval(lenv* e, lval* v);

/* Takes a Q-Expression and evaluates it as if it were a S-Expression */
lval* builtin_eval(lenv* e, lval* a){
    INCARGS(a, 1, "eval");
    INCTYPE(a, 0, LVAL_QEXPR, "eval");
    
    lval* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    
    return lval_eval(e, x);
}

lval* lval_join(lval* x, lval* y){
    /* For each cell in 'y', add it to 'x' */
    while(y->count){
        x = lval_add(x, lval_pop(y, 0));
    }

    /* Delete the empty 'y' and return 'x' */
    lval_del(y);

    return x;
}

/* Takes one or more Q-Expressions and returns a Q-Expression of them conjoined together */
lval* builtin_join(lenv* e, lval* a){
    /* Ensure each argument is a q-expression */
    for(int i=0; i < a->count; i++){
        INCTYPE(a, i, LVAL_QEXPR, "join");
    }

    lval* x = lval_pop(a, 0);

    while(a->count){
        x = lval_join(x, lval_pop(a, 0));
    }

    lval_del(a);

    return x;
}

/* Takes a value and a Q-Expression and appends it to the front */
lval* builtin_cons(lenv* e, lval* a){
    /* Ensure the first argument is a number or symbol and the second argument is a Q-expressions */
    INCARGS(a, 2, "cons");
    INCTYPE(a, 0, LVAL_NUM, "cons");
    INCTYPE(a, 1, LVAL_QEXPR, "cons");

    lval* x = lval_pop(a, 1);
    a = builtin_list(e, a);
    return lval_join(a, x);
    // Not sure whether this leads to a memory leak or not...
}

/* Returns the number of elements in a Q-Expression */
lval* builtin_len(lenv* e, lval* a){
    /* Ensure the one and only argument is a Q-Expression */
    INCARGS(a, 1, "len");
    INCTYPE(a, 0, LVAL_QEXPR, "len");

    lval* n = lval_num(0);
    a = lval_take(a, 0);
    while(a->count){
        lval* x = lval_pop(a, 0);
        lval_del(x);
        n->num++;
    }
    lval_del(a);

    return n;
}

/* Returns all of a Q-Expression except the final element */
lval* builtin_init(lenv* e, lval* a){
    /* Check error conditions */
    INCARGS(a, 1, "init");
    INCTYPE(a, 0, LVAL_QEXPR, "init");
    EMPLST(a, "head");

    /* Otherwise take all but last argument */
    lval* v = lval_take(a, 0);
    lval_del(lval_pop(v, v->count-1));

    return v;
}

/* Assigns values to a list of variables (cannot be builtin variables) */
lval* builtin_def(lenv* e, lval* a){
    /* Check that first cell is a Q-Expression */
    INCTYPE(a, 0, LVAL_QEXPR, "def");

    /* First argument is symbol list */
    lval* syms = a->cell[0];

    /* Ensure all elements of first list are symbols */
    for(int i=0; i < syms->count; i++){
        INCTYPE(syms, i, LVAL_SYM, "def");
    }

    /* Check correct number of symbols and values */
    LASSERT(a, syms->count == a->count-1, "Function 'def' cannot define incorrect number of values to symbols");

    /* Assign copies of values to symbols, only if new symbol is not builtin */
    for(int i=0; i < syms->count; i++){
        for(int j=0; j < builtins_count; j++){
            LASSERT(a, strcmp(syms->cell[i]->sym, builtins[j]), "Function 'def' cannot define builtin symbol");
        }
        lenv_put(e, syms->cell[i], a->cell[i+1]);
    }

    lval_del(a);
    
    return lval_sexpr();
}

/* Prints all of the named values in an environment if given 0, otherwise returns error */
lval* builtin_values(lenv* e, lval* a){
    /* Check error conditions */
    INCARGS(a, 1, "values");
    INCTYPE(a, 0, LVAL_SEXPR, "values");
    LASSERT(a, a->cell[0]->count==0, "Function 'values' passed invalid input");

    lenv_print(e);
    lval_del(a);

    return lval_sexpr();
}

/* Stops the prompt and exits */
lval* builtin_exit(lenv* e, lval* a){
    INCARGS(a, 1, "values");
    INCTYPE(a, 0, LVAL_SEXPR, "values");
    LASSERT(a, a->cell[0]->count==0, "Function 'values' passed invalid input");

    while_var = 2;
    lval_del(a);

    return lval_sexpr();
}

void lenv_add_builtin_to_array(char* name){
    builtins = realloc(builtins, 1024); // Reallocating 64 bytes crashes the program, so need more
    builtins[builtins_count] = name;
    builtins_count++;
}

/* Adds new builtin to environment */
void lenv_add_builtin(lenv* e, char* name, lbuiltin func){
    lenv_add_builtin_to_array(name);
    
    lval* k = lval_sym(name);
    lval* v = lval_fun(func);
    lenv_put(e, k, v);
    lval_del(k);
    lval_del(v);
}

void lenv_add_builtins(lenv* e){
    /* List functions */
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "eval", builtin_eval);
    lenv_add_builtin(e, "join", builtin_join);
    lenv_add_builtin(e, "cons", builtin_cons);
    lenv_add_builtin(e, "len", builtin_len);
    lenv_add_builtin(e, "init", builtin_init);

    /* Mathematical functions */
    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "add", builtin_add);
    lenv_add_builtin(e, "-", builtin_sub);
    lenv_add_builtin(e, "sub", builtin_sub);
    lenv_add_builtin(e, "*", builtin_mul);
    lenv_add_builtin(e, "mul", builtin_mul);
    lenv_add_builtin(e, "/", builtin_div);
    lenv_add_builtin(e, "div", builtin_div);
    lenv_add_builtin(e, "%", builtin_mod);
    lenv_add_builtin(e, "mod", builtin_mod);
    lenv_add_builtin(e, "^", builtin_pow);
    lenv_add_builtin(e, "pow", builtin_pow);
    lenv_add_builtin(e, "min", builtin_min);
    lenv_add_builtin(e, "max", builtin_max);

    /* Variable functions */
    lenv_add_builtin(e, "def", builtin_def);
    lenv_add_builtin(e, "values", builtin_values);

    /* Misc. functions */
    lenv_add_builtin(e, "exit", builtin_exit);
}

lval* lval_eval_sexpr(lenv* e, lval* v){
    /* Evaluate children */
    for(int i=0; i<v->count; i++){
        v->cell[i] = lval_eval(e, v->cell[i]);
    }

    /* Error checking */
    for(int i=0; i<v->count; i++){
        if(v->cell[i]->type==LVAL_ERR){
            return lval_take(v, i);
        }
    }

    /* Empty expression */
    if(v->count==0){
        return v;
    }

    /* Single expression */
    if(v->count==1){
        return lval_take(v, 0);
    }

    /* Ensure first element is a function after evaluation */
    lval* f = lval_pop(v, 0);
    if(f->type != LVAL_FUN){
        lval_del(v);
        lval_del(f);
        return lval_err("First element is not a function!");
    }

    /* If so, call function to get result */
    lval* result = f->fun(e, v);
    lval_del(f);

    return result;
}

lval* lval_eval(lenv* e, lval* v) {
    /* Evaluate a symbol, returning an error if the symbol isn't in the environment */
    if(v->type==LVAL_SYM){
        lval* x = lenv_get(e, v);
        lval_del(v);
        return x;
    }
    
    /* Evaluate S-expressions */
    if (v->type == LVAL_SEXPR) { 
        return lval_eval_sexpr(e, v); 
    }
    
    /* All other lval types remain the same */
    return v;
}

lval* lval_read_num(mpc_ast_t* t){
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE
        ? lval_num(x)
        : lval_err("invalid number");
}

lval* lval_read(mpc_ast_t* t){
    /* If Number or Symbol, return conversion to that type */
    if(strstr(t->tag, "number")){
        return lval_read_num(t);
    }
    if(strstr(t->tag, "symbol")){
        return lval_sym(t->contents);
    }

    /* If root (>) or sexpr then create empty list */
    lval* x = NULL;
    if(strcmp(t->tag, ">")==0){
        x = lval_sexpr();
    }
    if(strstr(t->tag, "sexpr")){
        x = lval_sexpr();
    }
    if(strstr(t->tag, "qexpr")){
        x = lval_qexpr();
    }

    /* Fill this list with any valid expression contained within */
    for(int i=0; i < t->children_num; i++){
        /* For loop skips these */
        if(strcmp(t->children[i]->contents, "(")==0){
            continue;
        }
        if(strcmp(t->children[i]->contents, ")")==0){
            continue;
        }
        if(strcmp(t->children[i]->contents, "{")==0){
            continue;
        }
        if(strcmp(t->children[i]->contents, "}")==0){
            continue;
        }
        if(strcmp(t->children[i]->tag, "regex")==0){
            continue;
        }

        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}

int main(int argc, char** argv) {
  
  /* Create Some Parsers */
  mpc_parser_t* Number   = mpc_new("number");
  mpc_parser_t* Symbol   = mpc_new("symbol");
  mpc_parser_t* Sexpr    = mpc_new("sexpr");
  mpc_parser_t* Qexpr    = mpc_new("qexpr");
  mpc_parser_t* Expr     = mpc_new("expr");
  mpc_parser_t* Lispy    = mpc_new("lispy");
  
  /* Define them */
  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                     \
      number   : /-?[0-9]+((\\.)[0-9]+)?/ ;               \
      symbol   : /[a-zA-Z0-9_+\\-*\\/^%\\\\=<>!&]+/ ;       \
      sexpr    : '(' <expr>* ')' ;                        \
      qexpr    : '{' <expr>* '}' ;                        \
      expr     : <number> | <symbol> | <sexpr> | <qexpr> ;\
      lispy    : /^/ <expr>* /$/ ;                        \
    ",
    Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
  
  puts("Lispy Version 0.0.0.0.11");
  puts("Press Ctrl+c to Exit\n");

  lenv* e = lenv_new();
  lenv_add_builtins(e);
  
  while (while_var>0) {
  
    while(while_var==2){
        char* input = readline("Exit Lispy? (y/n) ");
        add_history(input);

        /* Attempt to parse the user input */
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            /* On success print and delete the AST */
            lval* x = lval_eval(e, lval_read(r.output));
            if(strcmp(x->err, "Unbound symbol 'y'")==0){
                while_var = 0;
            } else{
                while_var = 1;
            }
            lval_del(x);
            mpc_ast_delete(r.output);
        } else {
            /* Otherwise print and delete the Error */
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
    
        free(input);
    }
    
    if(while_var == 1){
        char* input = readline("lispy> ");
        add_history(input);
        
        /* Attempt to parse the user input */
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            /* On success print and delete the AST */
            lval* x = lval_eval(e, lval_read(r.output));
            lval_println(x);
            lval_del(x);
            mpc_ast_delete(r.output);
        } else {
            /* Otherwise print and delete the Error */
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
        
        free(input);
    }
  }

  lenv_del(e);
  
  /* Undefine and delete our parsers */
  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
  
  return 0;
}