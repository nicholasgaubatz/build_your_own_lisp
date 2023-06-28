// Reference: https://buildyourownlisp.com/chapter8_error_handling

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
enum { LVAL_NUM, LVAL_ERR };

/* Declare new lval struct */
typedef struct {
    int type;
    long num;
    int err;
} lval;

/* Create a new number type lval */
lval lval_num(long x){
    lval v;
    v.type = LVAL_NUM;
    v.num = x;
    return v;
}

/* Create a new error type lval */
lval lval_err(int x){
    lval v;
    v.type = LVAL_ERR;
    v.err = x;
    return v;
}

/* Print an "lval" */
void lval_print(lval v){
    switch(v.type){
        /* If the type is a number, print it and then break out of switch */
        case LVAL_NUM: printf("%li", v.num); break;

        /* If the type is an error */
        case LVAL_ERR:
            /* Check the type of error and print it */
            if(v.err==LERR_DIV_ZERO){
                printf("Error: Division by Zero!");
            }
            if(v.err==LERR_BAD_OP){
                printf("Error: Invalid Operator!");
            }
            if(v.err==LERR_BAD_NUM){
                printf("Error: Invalid Number!");
            }
        break;
    }
}

/* Print an "lval" followed by a newline */
void lval_println(lval v){
    lval_print(v);
    putchar('\n'); // Interesting note: if instead we put "\n", we get "warning: passing argument 1 of 'putchar' makes integer from pointer without a cast"
}

/* Use operator string to see which operation to perform */
lval eval_op(lval x, char* op, lval y) {
    /* If either x or y is an error, return it. */
    if(x.type==LVAL_ERR){
        return x;
    }
    if(y.type==LVAL_ERR){
        return y;
    }

    /* Otherwise, continue as normal. */    
    if(strcmp(op, "+")==0 || strcmp(op, "add")==0) {
        return lval_num(x.num+y.num);
    }
    if(strcmp(op, "-")==0 || strcmp(op, "sub")==0) {
        return lval_num(x.num-y.num);
    }
    if(strcmp(op, "*")==0 || strcmp(op, "mul")==0) {
        return lval_num(x.num*y.num);
    }
    if(strcmp(op, "/")==0 || strcmp(op, "div")==0) {
        /* If second operand is zero return error */
        return y.num==0
            ? lval_err(LERR_DIV_ZERO)
            : lval_num(x.num/y.num);
    }
    if(strcmp(op, "\%")==0) {
        return lval_num(x.num%y.num);
    }
    if(strcmp(op, "^")==0) { // Computes powers using recursion
        if(y.num==0){
            return lval_num(1);
        }
        else{
            return lval_num(x.num * eval_op(lval_num(x.num), "^", lval_num(y.num-1)).num);
        }
    }
    if(strcmp(op, "min")==0) {
        int v = x.num;
        int w = y.num;
        return lval_num(v*(v<=w) + w*(v>w)); // A fun little way to get the minimum of two numbers :)
    }
    if(strcmp(op, "max")==0) {
        int v = x.num;
        int w = y.num;
        return lval_num(v*(v>=w) + w*(v<w));
    }

    return lval_err(LERR_BAD_OP);
}

lval eval(mpc_ast_t* t) {
    /* If tagged as number return it directly */
    if(strstr(t->tag, "number")){
        /* Check if there is some error in conversion. */
        errno = 0;
        long x = strtol(t->contents, NULL, 10); // Converts string to long: long int strtol(const char *str, char **endptr, int base)
        return errno != ERANGE
            ? lval_num(x)
            : lval_err(LERR_BAD_NUM);
    }

    /* The operator is always the second child */
    char* op = t->children[1]->contents;

    /* Store the third child in 'x' */
    lval x = eval(t->children[2]);

    /* Iterate the remaining children and combine */
    int i = 3;
    while(strstr(t->children[i]->tag, "expr")){
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }

    /* If the minus operator receives one argument, it negates it. Not sure if this is the best way to do this. */
    if(strcmp(op, "-")==0 && i==3){
        return lval_num(-1*x.num);
    }
    else{
        return x;
    }
}

int main(int argc, char** argv) {
  
  /* Create Some Parsers */
  mpc_parser_t* Number   = mpc_new("number");
  mpc_parser_t* Operator = mpc_new("operator");
  mpc_parser_t* Expr     = mpc_new("expr");
  mpc_parser_t* Lispy    = mpc_new("lispy");
  
  /* Define them with the following Language (added 'add', 'sub', 'mul', 'div', '%') (also added decimal number support) */
  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                     \
      number   : /-?[0-9]+((\\.)[0-9]+)?/ ;                             \
      operator : '+' | '-' | '*' | '/' | \"add\" | \"sub\" | \"mul\" | \"div\" | '\%' | '^' | \"min\" | \"max\" ;                  \
      expr     : <number> | '(' <operator> <expr>+ ')' ;  \
      lispy    : /^/ <operator> <expr>+ /$/ ;             \
    ",
    Number, Operator, Expr, Lispy);
  
  puts("Lispy Version 0.0.0.0.2");
  puts("Press Ctrl+c to Exit\n");
  
  while (1) {
  
    char* input = readline("lispy> ");
    add_history(input);
    
    /* Attempt to parse the user input */
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      /* On success print and delete the AST */
      lval result = eval(r.output);
      lval_println(result);
      mpc_ast_delete(r.output);
    } else {
      /* Otherwise print and delete the Error */
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    
    free(input);
  }
  
  /* Undefine and delete our parsers */
  mpc_cleanup(4, Number, Operator, Expr, Lispy);
  
  return 0;
}