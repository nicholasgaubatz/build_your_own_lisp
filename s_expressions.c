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

/* Create enumeration of possible error types */
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

/* Create enumeration of possible lval types */
enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR };

/* Declare new lval struct */
typedef struct lval {
    int type;
    long num;
    /* Error and Symbol types have some string data */
    char* err;
    char* sym;
    /* Count and Pointer to a list of "lval*" */
    int count;
    struct lval** cell;
} lval;

/* Construct a pointer to a new Number lval */
lval* lval_num(long x){
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

/* Construct a pointer to a new Error lval */
lval* lval_err(char* m){
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err = malloc(strlen(m) + 1);
    strcpy(v->err, m);
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

/* Construct a pointer to a new Sexpr lval */
lval* lval_sexpr(void){
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

/* Delete an lval */
void lval_del(lval* v){
    switch(v->type){
        /* Do nothing special for number type */
        case LVAL_NUM: break;

        /* For Err or Sym free the string data */
        case LVAL_ERR:
            free(v->err);
            break;
        case LVAL_SYM:
            free(v->sym);
            break;

        /* For Sexpr delete all elements inside */
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

lval* lval_add(lval* v, lval* x){
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count - 1] = x;
    return v;
}

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
        case LVAL_SEXPR:
            lval_expr_print(v, '(', ')');
            break;
    }
}

/* Print an "lval" followed by a newline */
void lval_println(lval* v){
    lval_print(v);
    putchar('\n');
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

int power(int x, int y){
    if(y==0){
        return 1;
    }
    else{
        return x * pow(x,y-1);
    }
}

lval* builtin_op(lval* a, char* op){
    /* Ensure all arguments are numbers */
    for(int i=0; i<a->count; i++){
        if(a->cell[i]->type != LVAL_NUM){
            lval_del(a);
            return lval_err("Cannot operate on a non-number!");
        }
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

        if(strcmp(op, "+")==0 || strcmp(op, "add")==0) {
            x->num += y->num;
        }
        if(strcmp(op, "-")==0 || strcmp(op, "sub")==0) {
            x->num -= y->num;
        }
        if(strcmp(op, "*")==0 || strcmp(op, "mul")==0) {
            x->num *= y->num;
        }
        if(strcmp(op, "/")==0 || strcmp(op, "div")==0) {
            /* If second operand is zero return error */
            if(y->num==0){
                lval_del(x);
                lval_del(y);
                x = lval_err("Division by zero!");
                break;
            }
            x->num /= y->num;
        }
        if(strcmp(op, "\%")==0) {
            x->num %= y->num;
        }
        if(strcmp(op, "^")==0) { // Computes powers using recursion
            x->num = power(x->num, y->num);
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

lval* lval_eval(lval* v);

lval* lval_eval_sexpr(lval* v){
    /* Evaluate children */
    for(int i=0; i<v->count; i++){
        v->cell[i] = lval_eval(v->cell[i]);
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

    /* Ensure first element is a symbol */
    lval* f = lval_pop(v, 0);
    if(f->type != LVAL_SYM){
        lval_del(f);
        lval_del(v);
        return lval_err("S-expression does not start with symbol!");
    }

    /* Call builtin with operator */
    lval* result = builtin_op(v, f->sym);
    lval_del(f);
    return result;
}

lval* lval_eval(lval* v) {
    /* Evaluate S-expressions */
    if (v->type == LVAL_SEXPR) { 
        return lval_eval_sexpr(v); 
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

    /* Fill this list with any valid expression contained within */
    for(int i=0; i < t->children_num; i++){
        /* For loop skips these */
        if(strcmp(t->children[i]->contents, "(")==0){
            continue;
        }
        if(strcmp(t->children[i]->contents, ")")==0){
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
  mpc_parser_t* Expr     = mpc_new("expr");
  mpc_parser_t* Lispy    = mpc_new("lispy");
  
  /* Define them with the following Language (added 'add', 'sub', 'mul', 'div', '%') (also added decimal number support) */
  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                     \
      number   : /-?[0-9]+((\\.)[0-9]+)?/ ;               \
      symbol   : '+' | '-' | '*' | '/' | \"add\" | \"sub\" | \"mul\" | \"div\" | '\%' | '^' | \"min\" | \"max\" ;                  \
      sexpr    : '(' <expr>* ')' ;                        \
      expr     : <number> | <symbol> | <sexpr> ;    \
      lispy    : /^/ <expr>* /$/ ;               \
    ",
    Number, Symbol, Sexpr, Expr, Lispy);
  
  puts("Lispy Version 0.0.0.0.9");
  puts("Press Ctrl+c to Exit\n");
  
  while (1) {
  
    char* input = readline("lispy> ");
    add_history(input);
    
    /* Attempt to parse the user input */
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      /* On success print and delete the AST */
      lval* x = lval_eval(lval_read(r.output));
      lval_println(x);
      lval_del(x);
    } else {
      /* Otherwise print and delete the Error */
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    
    free(input);
  }
  
  /* Undefine and delete our parsers */
  mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lispy);
  
  return 0;
}