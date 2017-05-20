/*
* Copyright 2016 sprocket
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the Free
* Software Foundation; either version 2 of the License, or (at your option)
* any later version.
*/

#ifndef ELASTICPL_H_
#define ELASTICPL_H_

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define MAX_LITERAL_SIZE 260
#define TOKEN_LIST_SIZE 4096
#define PARSE_STACK_SIZE 24000
#define CALL_STACK_SIZE 256

#define MAX_VM_MEMORY_SIZE	100000		// Maximum Number Of Bytes That Can Be Used By VM Memory Model
#define VM_MEMORY_SIZE	64000			// Number Of Integers Supported By ElasticPL
#define VM_FLOAT_SIZE	1000			// Number Of Doubles Supported By ElasticPL

// Max Array Variable Index For Each Data Type
uint64_t max_vm_ints;
uint64_t max_vm_uints;
uint64_t max_vm_longs;
uint64_t max_vm_ulongs;
uint64_t max_vm_floats;
uint64_t max_vm_doubles;

// Index Value Of Main & Verify Functions In AST Array
int ast_func_idx;
int ast_main_idx;
int ast_verify_idx;

typedef enum {
	NODE_ERROR,
	NODE_END_STATEMENT,
	NODE_CONSTANT,
	NODE_VAR_CONST,
	NODE_VAR_EXP,
	NODE_VERIFY,
	NODE_ASSIGN,
	NODE_OR,
	NODE_AND,
	NODE_BITWISE_OR,
	NODE_BITWISE_XOR,
	NODE_BITWISE_AND,
	NODE_COMPL,
	NODE_EQ,
	NODE_NE,
	NODE_LT,
	NODE_GT,
	NODE_LE,
	NODE_GE,
	NODE_INCREMENT_R,
	NODE_INCREMENT_L,
	NODE_ADD_ASSIGN,
	NODE_SUB_ASSIGN,
	NODE_MUL_ASSIGN,
	NODE_DIV_ASSIGN,
	NODE_MOD_ASSIGN,
	NODE_LSHFT_ASSIGN,
	NODE_RSHFT_ASSIGN,
	NODE_AND_ASSIGN,
	NODE_XOR_ASSIGN,
	NODE_OR_ASSIGN,
	NODE_CONDITIONAL,
	NODE_COND_ELSE,
	NODE_ADD,
	NODE_DECREMENT_R,
	NODE_DECREMENT_L,
	NODE_SUB,
	NODE_NEG,
	NODE_MUL,
	NODE_DIV,
	NODE_MOD,
	NODE_RSHIFT,
	NODE_LSHIFT,
	NODE_RROT,
	NODE_LROT,
	NODE_NOT,
	NODE_TRUE,
	NODE_FALSE,
	NODE_BLOCK,
	NODE_EXPRESSION,
	NODE_IF,
	NODE_ELSE,
	NODE_REPEAT,
	NODE_BREAK,
	NODE_CONTINUE,
	NODE_PARAM,
	NODE_SIN,
	NODE_COS,
	NODE_TAN,
	NODE_SINH,
	NODE_COSH,
	NODE_TANH,
	NODE_ASIN,
	NODE_ACOS,
	NODE_ATAN,
	NODE_ATAN2,
	NODE_EXPNT,
	NODE_LOG,
	NODE_LOG10,
	NODE_POW,
	NODE_SQRT,
	NODE_CEIL,
	NODE_FLOOR,
	NODE_ABS,
	NODE_FABS,
	NODE_FMOD,
	NODE_GCD,
	NODE_ARRAY_INT,
	NODE_ARRAY_UINT,
	NODE_ARRAY_LONG,
	NODE_ARRAY_ULONG,
	NODE_ARRAY_FLOAT,
	NODE_ARRAY_DOUBLE,
	NODE_FUNCTION,
	NODE_CALL_FUNCTION,
	NODE_RESULT,
	NODE_INIT_ONCE
} NODE_TYPE;


typedef enum {
	TOKEN_COMMA,
	TOKEN_ASSIGN,
	TOKEN_OR,
	TOKEN_AND,
	TOKEN_BITWISE_OR,
	TOKEN_BITWISE_XOR,
	TOKEN_BITWISE_AND,
	TOKEN_EQ,
	TOKEN_NE,
	TOKEN_LT,
	TOKEN_GT,
	TOKEN_LE,
	TOKEN_GE,
	TOKEN_INCREMENT,
	TOKEN_ADD_ASSIGN,
	TOKEN_SUB_ASSIGN,
	TOKEN_MUL_ASSIGN,
	TOKEN_DIV_ASSIGN,
	TOKEN_MOD_ASSIGN,
	TOKEN_LSHFT_ASSIGN,
	TOKEN_RSHFT_ASSIGN,
	TOKEN_AND_ASSIGN,
	TOKEN_XOR_ASSIGN,
	TOKEN_OR_ASSIGN,
	TOKEN_CONDITIONAL,
	TOKEN_COND_ELSE,
	TOKEN_ADD,
	TOKEN_DECREMENT,
	TOKEN_SUB,
	TOKEN_NEG,
	TOKEN_MUL,
	TOKEN_DIV,
	TOKEN_MOD,
	TOKEN_RSHIFT,
	TOKEN_LSHIFT,
	TOKEN_RROT,
	TOKEN_LROT,
	TOKEN_COMPL,
	TOKEN_NOT,
	TOKEN_CONSTANT,
	TOKEN_TRUE,
	TOKEN_FALSE,
	TOKEN_IF,
	TOKEN_ELSE,
	TOKEN_REPEAT,
	TOKEN_VAR_BEGIN,
	TOKEN_VAR_END,
	TOKEN_BLOCK_BEGIN,
	TOKEN_BLOCK_END,
	TOKEN_OPEN_PAREN,
	TOKEN_CLOSE_PAREN,
	TOKEN_LITERAL,
	TOKEN_END_STATEMENT,
	TOKEN_BREAK,
	TOKEN_CONTINUE,
	TOKEN_VERIFY,
	TOKEN_COMMENT,
	TOKEN_BLOCK_COMMENT,
	TOKEN_TRACE,
	TOKEN_EOF,
	TOKEN_SIN,
	TOKEN_COS,
	TOKEN_TAN,
	TOKEN_SINH,
	TOKEN_COSH,
	TOKEN_TANH,
	TOKEN_ASIN,
	TOKEN_ACOS,
	TOKEN_ATAN,
	TOKEN_ATAN2,
	TOKEN_EXPNT,
	TOKEN_LOG,
	TOKEN_LOG10,
	TOKEN_POW,
	TOKEN_SQRT,
	TOKEN_CEIL,
	TOKEN_FLOOR,
	TOKEN_ABS,
	TOKEN_FABS,
	TOKEN_FMOD,
	TOKEN_GCD,
	TOKEN_ARRAY_INT,
	TOKEN_ARRAY_UINT,
	TOKEN_ARRAY_LONG,
	TOKEN_ARRAY_ULONG,
	TOKEN_ARRAY_FLOAT,
	TOKEN_ARRAY_DOUBLE,
	TOKEN_FUNCTION,
	TOKEN_CALL_FUNCTION,
	TOKEN_RESULT,
	TOKEN_INIT_ONCE
} EPL_TOKEN_TYPE;

typedef enum {
	EXP_NONE,
	EXP_STATEMENT,
	EXP_EXPRESSION,
	EXP_FUNCTION
} EXP_TYPE;

typedef enum {
	DT_NONE,
	DT_STRING,
	DT_INT,
	DT_UINT,
	DT_LONG,
	DT_ULONG,
	DT_FLOAT,
	DT_DOUBLE
} DATA_TYPE;

// Token Type / Literal Value From ElasticPL Source Code
typedef struct {
	int token_id;
	EPL_TOKEN_TYPE type;
	char *literal;
	EXP_TYPE exp;
	int inputs;
	int prec;
	int line_num;
	DATA_TYPE data_type;
} SOURCE_TOKEN;

// List Of All Tokens In ElasticPL Source Code
typedef struct {
	SOURCE_TOKEN *token;
	int num;
	int size;
} SOURCE_TOKEN_LIST;

struct EXP_TOKEN_LIST {
	char* str;
	int len;
	EPL_TOKEN_TYPE type;
	EXP_TYPE exp;
	int inputs;
	int prec;
	DATA_TYPE data_type;
};

typedef struct AST {
	NODE_TYPE type;
	EXP_TYPE exp;
	int32_t value;
	int64_t ivalue;
	uint64_t uvalue;
	double fvalue;
	unsigned char *svalue;
	int token_num;
	int line_num;
	bool end_stmnt;
	DATA_TYPE data_type;
	bool is_64bit;
	bool is_signed;
	bool is_float;
	bool is_vm_mem;
	struct AST*	parent;
	struct AST*	left;
	struct AST*	right;
} ast;

int stack_op_idx;
int stack_exp_idx;
int top_op;

int *stack_op;		// List Of Operators For Parsing
ast **stack_exp;	// List Of Expresions For Parsing / Final Expression List

int vm_ast_cnt;		// Number Of AST Root Nodes In VM
ast **vm_ast;		// Final AST List For VM

// Function Declarations
extern bool create_epl_vm(char *source);
static bool delete_epl_vm();

extern bool init_token_list(SOURCE_TOKEN_LIST *token_list, size_t size);
static DATA_TYPE validate_literal(char *str);
static bool validate_tokens(SOURCE_TOKEN_LIST *token_list);
static bool add_token(SOURCE_TOKEN_LIST *token_list, int token_id, char *literal, DATA_TYPE data_type, int line_num);
extern void delete_token_list(SOURCE_TOKEN_LIST *token_list);
extern bool get_token_list(char *str, SOURCE_TOKEN_LIST *token_list);
static void dump_token_list(SOURCE_TOKEN_LIST *token_list);

extern bool parse_token_list(SOURCE_TOKEN_LIST *token_list);
static bool create_exp(SOURCE_TOKEN *token, int token_num);
static NODE_TYPE get_node_type(SOURCE_TOKEN *token, int token_num);
static bool validate_inputs(SOURCE_TOKEN *token, int token_num, NODE_TYPE node_type);
static ast* pop_exp();
static void push_exp(ast* exp);
static int pop_op();
static void push_op(int token_id);
static ast* add_exp(NODE_TYPE node_type, EXP_TYPE exp_type, bool is_vm_mem, int64_t val_int64, uint64_t val_uint64, double val_double, unsigned char *svalue, int token_num, int line_num, DATA_TYPE data_type, ast* left, ast* right);
extern char* get_node_str(NODE_TYPE node_type);
extern void dump_vm_ast(ast* root);
static void print_node(ast* node);
static bool validate_ast();
static bool validate_functions();
static bool validate_function_calls();

extern char* convert_ast_to_c();
static char* convert(ast* exp);
static char* append_strings(char * old, char * new);
extern uint32_t calc_wcet();
static uint32_t calc_weight(ast* root, uint32_t *depth);
static uint32_t get_node_weight(ast* node);
extern int interpret_ast(bool first_run);
static double interpret(ast* exp);
static void mangle_state(int x);
#ifdef _MSC_VER
static uint32_t rotl32(uint32_t x, int n);
static uint32_t rotr32(uint32_t x, int n);
#else
static uint32_t rotl32(uint32_t x, unsigned int n);
static uint32_t rotr32(uint32_t x, unsigned int n);
#endif

#endif // ELASTICPL_H_