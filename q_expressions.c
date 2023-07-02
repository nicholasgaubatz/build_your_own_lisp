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

/* Create enumeration of possible lval types */
enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };

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

/* Construct a pointer to a new Qexpr lval */
lval* lval_qexpr(void){
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
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

/* Macro to make error messages easier */
#define LASSERT(args, cond, err) \
    if (!(cond)) { lval_del(args); return lval_err(err); }

/* Takes a Q-Expression and returns a Q-Expression with only the first element */
lval* builtin_head(lval* a){
    /* Check error conditions */
    LASSERT(a, a->count == 1, "Function 'head' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'head' passed incorrect types!");
    LASSERT(a, a->cell[0]->count != 0, "Function 'head' passed {}!");

    /* Otherwise take first argument */
    lval* v = lval_take(a, 0);

    /* Delete all elements that are not head and return */
    while (v->count > 1){
        lval_del(lval_pop(v, 1));
    }

    return v;
}

/* Takes a Q-Expression and returns a Q-Expression with the first element removed */
lval* builtin_tail(lval* a){
    /* Check error conditions */
    LASSERT(a, a->count == 1, "Function 'tail' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'tail' passed incorrect types!");
    LASSERT(a, a->cell[0]->count != 0, "Function 'tail' passed {}!");

    /* Otherwise take all but first argument */
    lval* v = lval_take(a, 0);
    lval_del(lval_pop(v, 0));

    return v;
}

/* Takes one or more arguments and returns a new Q-Expression containing the arguments */
lval* builtin_list(lval* a){
    a->type = LVAL_QEXPR;
    return a;
}

lval* lval_eval(lval* v);

/* Takes a Q-Expression and evaluates it as if it were a S-Expression */
lval* builtin_eval(lval* a){
    LASSERT(a, a->count == 1, "Function 'eval' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'eval' passed incorrect type!");
    
    lval* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    
    return lval_eval(x);
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
lval* builtin_join(lval* a){
    /* Ensure each argument is a q-expression */
    for(int i=0; i < a->count; i++){
        LASSERT(a, a->cell[i]->type == LVAL_QEXPR, "Function 'join' passed incorrect type!");
    }

    lval* x = lval_pop(a, 0);

    while(a->count){
        x = lval_join(x, lval_pop(a, 0));
    }

    lval_del(a);

    return x;
}

/* Takes a value and a Q-Expression and appends it to the front */
lval* builtin_cons(lval* a){
    /* Ensure the first argument is a number or symbol and the second argument is a Q-expressions */
    LASSERT(a, a->count == 2, "Function 'cons' passed too few or many arguments!");
    LASSERT(a, a->cell[0]->type==LVAL_NUM || a->cell[0]->type==LVAL_SYM, "Function 'cons' passed incorrect type!");
    LASSERT(a, a->cell[1]->type == LVAL_QEXPR, "Function 'cons' passed incorrect type!")

    lval* x = lval_pop(a, 1);
    a = builtin_list(a);
    return lval_join(a, x);
    // Not sure whether this leads to a memory leak or not...
}

/* Returns the number of elements in a Q-Expression */
lval* builtin_len(lval* a){
    /* Ensure the one and only argument is a Q-Expression */
    LASSERT(a, a->count == 1, "Function 'len' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'len' passed incorrect type!");

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
lval* builtin_init(lval* a){
    /* Check error conditions */
    LASSERT(a, a->count == 1, "Function 'init' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'init' passed incorrect types!");
    LASSERT(a, a->cell[0]->count != 0, "Function 'init' passed {}!");

    /* Otherwise take all but last argument */
    lval* v = lval_take(a, 0);
    lval_del(lval_pop(v, v->count-1));

    return v;
}

// TODO: can we do something to make this cleaner? Maybe something with macros?
lval* builtin(lval* a, char* func) {
    if (strcmp("list", func) == 0){
        return builtin_list(a);
    }
    if (strcmp("head", func) == 0){
        return builtin_head(a);
    }
    if (strcmp("tail", func) == 0){
        return builtin_tail(a);
    }
    if (strcmp("join", func) == 0){
        return builtin_join(a);
    }
    if (strcmp("eval", func) == 0){
        return builtin_eval(a);
    }
    if (strstr("+-/*\%^", func)){
        return builtin_op(a, func);
    }
    if (strcmp("add", func) == 0){
        return builtin_op(a, func);
    }
    if (strcmp("sub", func) == 0){
        return builtin_op(a, func);
    }
    if (strcmp("mul", func) == 0){
        return builtin_op(a, func);
    }
    if (strcmp("div", func) == 0){
        return builtin_op(a, func);
    }
    if (strcmp("min", func) == 0){
        return builtin_op(a, func);
    }
    if (strcmp("max", func) == 0){
        return builtin_op(a, func);
    }
    if (strcmp("cons", func) == 0){
        return builtin_cons(a);
    }
    if (strcmp("len", func) == 0){
        return builtin_len(a);
    }
    if (strcmp("init", func) == 0){
        return builtin_init(a);
    }
  lval_del(a);
  return lval_err("Unknown Function!");
}

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
    lval* result = builtin(v, f->sym);
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
  
  /* Define them with the following Language (added 'add', 'sub', 'mul', 'div', '%') (also added decimal number support) */
  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                     \
      number   : /-?[0-9]+((\\.)[0-9]+)?/ ;               \
      symbol   : \"list\" | \"head\" | \"tail\"           \
               | \"eval\" | \"join\"                      \
               | '+' | '-' | '*' | '/' | \"add\"          \
               | \"sub\" | \"mul\" | \"div\" | '\%'       \
               | '^' | \"min\" | \"max\" | \"cons\"       \
               | \"len\" | \"init\" ;                     \
      sexpr    : '(' <expr>* ')' ;                        \
      qexpr    : '{' <expr>* '}' ;                        \
      expr     : <number> | <symbol> | <sexpr> | <qexpr> ;\
      lispy    : /^/ <expr>* /$/ ;                        \
    ",
    Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
  
  puts("Lispy Version 0.0.0.0.10");
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
  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
  
  return 0;
}