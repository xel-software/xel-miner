/*
* Copyright 2016 sprocket
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the Free
* Software Foundation; either version 2 of the License, or (at your option)
* any later version.
*/

#include <stdio.h>

#include "ElasticPL.h"
#include "../miner.h"

/*****************************************************************************
ElasticPL Token List

Format:  Str, Len, Type, Exp, Inputs, Prec, Initial Data Type

Str:		Token String
Len:		String Length Used For "memcmp"
Type:		Enumerated Token Type
Exp:		Enumerated Num Of Expressions To Link To Node
Inputs:		Number Of Required Inputs To Operator / Function
Prec:		(Precedence) Determines Parsing Order
Data Type:  Data Type Of Value Returned By Operator / Function
******************************************************************************/
struct EXP_TOKEN_LIST epl_token[] = {
	{ "//",							2,	TOKEN_COMMENT,		EXP_NONE,		0,	0,	DT_NONE },
	{ "/*",							2,	TOKEN_BLOCK_COMMENT,EXP_NONE,		0,	0,	DT_NONE },
	{ ";",							1,	TOKEN_END_STATEMENT,EXP_NONE,		0,	0,	DT_NONE },
	{ ",",							1,	TOKEN_COMMA,		EXP_NONE,		0,	0,	DT_NONE },
	{ "{",							1,	TOKEN_BLOCK_BEGIN,	EXP_STATEMENT,	2,	1,	DT_NONE },
	{ "}",							1,	TOKEN_BLOCK_END,	EXP_STATEMENT,	2,	1,	DT_NONE },
	{ "()",							2,	TOKEN_CALL_FUNCTION,EXP_STATEMENT,	1,	2,	DT_NONE },
	{ "(",							1,	TOKEN_OPEN_PAREN,	EXP_NONE,		0,	1,	DT_INT },
	{ ")",							1,	TOKEN_CLOSE_PAREN,	EXP_NONE,		0,	1,	DT_INT },
	{ "array_int",					9,	TOKEN_ARRAY_INT,	EXP_STATEMENT,	1,	0,	DT_NONE },
	{ "array_uint",					10,	TOKEN_ARRAY_UINT,	EXP_STATEMENT,	1,	0,	DT_NONE },
	{ "array_long",					10,	TOKEN_ARRAY_LONG,	EXP_STATEMENT,	1,	0,	DT_NONE },
	{ "array_ulong",				11,	TOKEN_ARRAY_ULONG,	EXP_STATEMENT,	1,	0,	DT_NONE },
	{ "array_float",				11,	TOKEN_ARRAY_FLOAT,	EXP_STATEMENT,	1,	0,	DT_NONE },
	{ "array_double",				12,	TOKEN_ARRAY_DOUBLE,	EXP_STATEMENT,	1,	0,	DT_NONE },
	{ "submit_sz",					9,	TOKEN_SUBMIT_SZ,	EXP_STATEMENT,	1,	0,	DT_NONE },
	{ "submit_idx",					10,	TOKEN_SUBMIT_IDX,	EXP_STATEMENT,	1,	0,	DT_NONE },
	{ "repeat",						6,	TOKEN_REPEAT,		EXP_STATEMENT,	4,	2,	DT_NONE },
	{ "if",							2,	TOKEN_IF,			EXP_STATEMENT,	2,	2,	DT_NONE },
	{ "else",						4,	TOKEN_ELSE,			EXP_STATEMENT,	2,	2,	DT_NONE },
	{ "break",						5,	TOKEN_BREAK,		EXP_STATEMENT,	0,	2,	DT_NONE },
	{ "continue",					8,	TOKEN_CONTINUE,		EXP_STATEMENT,	0,	2,	DT_NONE },
	{ "function",					8,	TOKEN_FUNCTION,		EXP_STATEMENT,	2,	2,	DT_NONE },
	{ "verify_bty",					10,	TOKEN_VERIFY_BTY,	EXP_FUNCTION,	1,	2,	DT_NONE },
	{ "verify_pow",					10,	TOKEN_VERIFY_POW,	EXP_FUNCTION,	4,	2,	DT_NONE },

	{ "i[",							2,	TOKEN_VAR_BEGIN,	EXP_EXPRESSION,	1,	4,	DT_INT },
	{ "u[",							2,	TOKEN_VAR_BEGIN,	EXP_EXPRESSION,	1,	4,	DT_UINT },
	{ "l[",							2,	TOKEN_VAR_BEGIN,	EXP_EXPRESSION,	1,	4,	DT_LONG },
	{ "ul[",						3,	TOKEN_VAR_BEGIN,	EXP_EXPRESSION,	1,	4,	DT_ULONG },
	{ "f[",							2,	TOKEN_VAR_BEGIN,	EXP_EXPRESSION,	1,	4,	DT_FLOAT },
	{ "d[",							2,	TOKEN_VAR_BEGIN,	EXP_EXPRESSION,	1,	4,	DT_DOUBLE },
	{ "m[",							2,	TOKEN_VAR_BEGIN,	EXP_EXPRESSION,	1,	4,	DT_UINT_M },
	{ "s[",							2,	TOKEN_VAR_BEGIN,	EXP_EXPRESSION,	1,	4,	DT_UINT_S },
	{ "]",							1,	TOKEN_VAR_END,		EXP_EXPRESSION,	1,	4,	DT_INT },

	{ "++",							2,	TOKEN_INCREMENT,	EXP_EXPRESSION,	1,	5,	DT_INT },	// Increment
	{ "--",							2,	TOKEN_DECREMENT,	EXP_EXPRESSION,	1,	5,	DT_INT },	// Decrement

	{ "+=",							2,	TOKEN_ADD_ASSIGN,	EXP_STATEMENT,	2,	18,	DT_INT },	// Assignment
	{ "-=",							2,	TOKEN_SUB_ASSIGN,	EXP_STATEMENT,	2,	18,	DT_INT },	// Assignment
	{ "*=",							2,	TOKEN_MUL_ASSIGN,	EXP_STATEMENT,	2,	18,	DT_INT },	// Assignment
	{ "/=",							2,	TOKEN_DIV_ASSIGN,	EXP_STATEMENT,	2,	18,	DT_FLOAT },	// Assignment
	{ "%=",							2,	TOKEN_MOD_ASSIGN,	EXP_STATEMENT,	2,	18,	DT_INT },	// Assignment
	{ "<<=",						3,	TOKEN_LSHFT_ASSIGN,	EXP_STATEMENT,	2,	18,	DT_INT },	// Assignment
	{ ">>=",						3,	TOKEN_RSHFT_ASSIGN,	EXP_STATEMENT,	2,	18,	DT_INT },	// Assignment
	{ "&=",							2,	TOKEN_AND_ASSIGN,	EXP_STATEMENT,	2,	18,	DT_INT },	// Assignment
	{ "^=",							2,	TOKEN_XOR_ASSIGN,	EXP_STATEMENT,	2,	18,	DT_INT },	// Assignment
	{ "|=",							2,	TOKEN_OR_ASSIGN,	EXP_STATEMENT,	2,	18,	DT_INT },	// Assignment

	{ "+",							1,	TOKEN_ADD,			EXP_EXPRESSION,	2,	7,	DT_INT },	// Additive
	{ "-",							1,	TOKEN_SUB,			EXP_EXPRESSION,	2,	7,	DT_INT },	// Additive
	{ "-",							1,	TOKEN_NEG,			EXP_EXPRESSION,	1,	5,	DT_INT },	// Additive

	{ "*",							1,	TOKEN_MUL,			EXP_EXPRESSION,	2,	6,	DT_INT },	// Multiplicative
	{ "/",							1,	TOKEN_DIV,			EXP_EXPRESSION,	2,	6,	DT_FLOAT },	// Multiplicative
	{ "%",							1,	TOKEN_MOD,			EXP_EXPRESSION,	2,	6,	DT_INT },	// Multiplicative

	{ "<<<",						3,	TOKEN_LROT,			EXP_EXPRESSION,	2,	8,	DT_INT },	// Shift
	{ "<<",							2,	TOKEN_LSHIFT,		EXP_EXPRESSION,	2,	8,	DT_INT },	// Shift
	{ ">>>",						3,	TOKEN_RROT,			EXP_EXPRESSION,	2,	8,	DT_INT },	// Shift
	{ ">>",							2,	TOKEN_RSHIFT,		EXP_EXPRESSION,	2,	8,	DT_INT },	// Shift

	{ "<=",							2,	TOKEN_LE,			EXP_EXPRESSION,	2,	9,	DT_INT },	// Relational
	{ ">=",							2,	TOKEN_GE,			EXP_EXPRESSION,	2,	9,	DT_INT },	// Relational
	{ "<",							1,	TOKEN_LT,			EXP_EXPRESSION,	2,	9,	DT_INT },	// Relational
	{ ">",							1,	TOKEN_GT,			EXP_EXPRESSION,	2,	9,	DT_INT },	// Relational

	{ "==",							2,	TOKEN_EQ,			EXP_EXPRESSION,	2,	10,	DT_INT },	// Equality
	{ "!=",							2,	TOKEN_NE,			EXP_EXPRESSION,	2,	10,	DT_INT },	// Equality

	{ "&&",							2,	TOKEN_AND,			EXP_EXPRESSION,	2,	14,	DT_INT },	// Logical AND
	{ "||",							2,	TOKEN_OR,			EXP_EXPRESSION,	2,	15,	DT_INT },	// Logical OR

	{ "&",							1,	TOKEN_BITWISE_AND,	EXP_EXPRESSION,	2,	11,	DT_INT },	// Bitwise AND
	{ "and",						3,	TOKEN_BITWISE_AND,	EXP_EXPRESSION,	2,	11,	DT_INT },	// Bitwise AND
	{ "^",							1,	TOKEN_BITWISE_XOR,	EXP_EXPRESSION,	2,	12,	DT_INT },	// Bitwise XOR
	{ "xor",						3,	TOKEN_BITWISE_XOR,	EXP_EXPRESSION,	2,	12,	DT_INT },	// Bitwise XOR
	{ "|",							1,	TOKEN_BITWISE_OR,	EXP_EXPRESSION,	2,	13,	DT_INT },	// Bitwise OR
	{ "or",							2,	TOKEN_BITWISE_OR,	EXP_EXPRESSION,	2,	13,	DT_INT },	// Bitwise OR

	{ "=",							1,	TOKEN_ASSIGN,		EXP_STATEMENT,	2,	18,	DT_INT },	// Assignment

	{ "?",							1,	TOKEN_CONDITIONAL,	EXP_STATEMENT,	2,	16,	DT_INT },	// Conditional
	{ ":",							1,	TOKEN_COND_ELSE,	EXP_STATEMENT,	2,	17,	DT_INT },	// Conditional

	{ "~",							1,	TOKEN_COMPL,		EXP_EXPRESSION,	1,	5,	DT_INT },	// Unary Operator
	{ "!",							1,	TOKEN_NOT,			EXP_EXPRESSION,	1,	5,	DT_INT },	// Unary Operator
	{ "true",						4,	TOKEN_TRUE,			EXP_EXPRESSION,	0,	40,	DT_INT },	// Unary Operator
	{ "false",						5,	TOKEN_FALSE,		EXP_EXPRESSION,	0,	40,	DT_INT },	// Unary Operator

	{ "sinh",						4,	TOKEN_SINH,			EXP_FUNCTION,	1,	2,	DT_FLOAT },	// Built In Math Functions
	{ "sin",						3,	TOKEN_SIN,			EXP_FUNCTION,	1,	2,	DT_FLOAT },	// Built In Math Functions
	{ "cosh",						4,	TOKEN_COSH,			EXP_FUNCTION,	1,	2,	DT_FLOAT },	// Built In Math Functions
	{ "cos",						3,	TOKEN_COS,			EXP_FUNCTION,	1,	2,	DT_FLOAT },	// Built In Math Functions
	{ "tanh",						4,	TOKEN_TANH,			EXP_FUNCTION,	1,	2,	DT_FLOAT },	// Built In Math Functions
	{ "tan",						3,	TOKEN_TAN,			EXP_FUNCTION,	1,	2,	DT_FLOAT },	// Built In Math Functions
	{ "asin",						4,	TOKEN_ASIN,			EXP_FUNCTION,	1,	2,	DT_FLOAT },	// Built In Math Functions
	{ "acos",						4,	TOKEN_ACOS,			EXP_FUNCTION,	1,	2,	DT_FLOAT },	// Built In Math Functions
	{ "atan2",						5,	TOKEN_ATAN2,		EXP_FUNCTION,	2,	2,	DT_FLOAT },	// Built In Math Functions
	{ "atan",						4,	TOKEN_ATAN,			EXP_FUNCTION,	1,	2,	DT_FLOAT },	// Built In Math Functions
	{ "exp",						3,	TOKEN_EXPNT,		EXP_FUNCTION,	1,	2,	DT_FLOAT },	// Built In Math Functions
	{ "log10",						5,	TOKEN_LOG10,		EXP_FUNCTION,	1,	2,	DT_FLOAT },	// Built In Math Functions
	{ "log",						3,	TOKEN_LOG,			EXP_FUNCTION,	1,	2,	DT_FLOAT },	// Built In Math Functions
	{ "pow",						3,	TOKEN_POW,			EXP_FUNCTION,	2,	2,	DT_FLOAT },	// Built In Math Functions
	{ "sqrt",						4,	TOKEN_SQRT,			EXP_FUNCTION,	1,	2,	DT_FLOAT },	// Built In Math Functions
	{ "ceil",						4,	TOKEN_CEIL,			EXP_FUNCTION,	1,	2,	DT_FLOAT },	// Built In Math Functions
	{ "floor",						5,	TOKEN_FLOOR,		EXP_FUNCTION,	1,	2,	DT_FLOAT },	// Built In Math Functions
	{ "fabs",						4,	TOKEN_FABS,			EXP_FUNCTION,	1,	2,	DT_FLOAT },	// Built In Math Functions
	{ "abs",						3,	TOKEN_ABS,			EXP_FUNCTION,	1,	2,	DT_INT },	// Built In Math Functions
	{ "fmod",						4,	TOKEN_FMOD,			EXP_FUNCTION,	2,	2,	DT_FLOAT },	// Built In Math Functions
	{ "gcd",						3,	TOKEN_GCD,			EXP_FUNCTION,	2,	2,	DT_FLOAT },	// Built In Math Functions
};

extern bool init_token_list(SOURCE_TOKEN_LIST *token_list, size_t size) {
	token_list->token = malloc(size * sizeof(SOURCE_TOKEN));
	token_list->num = 0;
	token_list->size = size;

	if (!token_list->token)
		return false;
	else
		return true;
}

static bool add_token(SOURCE_TOKEN_LIST *token_list, int token_id, char *literal, DATA_TYPE data_type, int line_num) {
	char *str;

	// Increase Token List Size If Needed
	if (token_list->num == token_list->size) {
		token_list->size += 1024;
		token_list->token = (SOURCE_TOKEN *)realloc(token_list->token, token_list->size * sizeof(SOURCE_TOKEN));

		if (!token_list->token)
			return false;
	}

	// EPL Tokens
	if (token_id >= 0) {

		// Determine If '-' Is Binary Or Unary
		if (epl_token[token_id].type == TOKEN_SUB) {
			if (token_list->num == 0)
				token_id++;
			else if (token_list->token[token_list->num - 1].type != TOKEN_CLOSE_PAREN) {
				if (token_list->token[token_list->num - 1].exp != EXP_EXPRESSION || token_list->token[token_list->num - 1].inputs > 1)
					token_id++;
			}
		}

		token_list->token[token_list->num].type = epl_token[token_id].type;
		token_list->token[token_list->num].exp = epl_token[token_id].exp;
		token_list->token[token_list->num].literal = NULL;
		token_list->token[token_list->num].inputs = epl_token[token_id].inputs;
		token_list->token[token_list->num].prec = epl_token[token_id].prec;
		token_list->token[token_list->num].data_type = epl_token[token_id].data_type;
	}
	// Literals
	else if (literal != NULL) {
		str = calloc(1, strlen(literal) + 1);

		if (!str) return false;

		strcpy(str, literal);

		token_list->token[token_list->num].literal = str;
		token_list->token[token_list->num].data_type = data_type;
		token_list->token[token_list->num].type = TOKEN_LITERAL;
		token_list->token[token_list->num].exp = EXP_EXPRESSION;
		token_list->token[token_list->num].inputs = 0;
		token_list->token[token_list->num].prec = -1;
	}
	// Error
	else {
		return false;
	}

	token_list->token[token_list->num].token_id = token_id;
	token_list->token[token_list->num].line_num = line_num;
	token_list->num++;

	return true;
}

extern void delete_token_list(SOURCE_TOKEN_LIST *token_list) {
	int i;

	for (i = 0; i < token_list->num; i++) {
		if (token_list->token[i].literal)
			free(token_list->token[i].literal);
	}

	free(token_list->token);
	token_list->token = NULL;
	token_list->num = 0;
	token_list->size = 0;
}

static DATA_TYPE validate_literal(char *str) {
	int i, len;
	int max_hex_len = 18;
	int max_bin_len = 66;
	int max_int_len = 21;
	int max_dbl_len = 21;
	char *ptr;
	bool string = false;

	if (!str)
		return false;

	len = strlen(str);

	// Validate Hex Numbers
	if (strstr(str, "0x") == str) {
		if ((len <= 2) || (len > max_hex_len))
			return DT_NONE;

		for (i = 2; i < len; i++) {
			if (!(str[i] >= '0' && str[i] <= '9') && !(str[i] >= 'a' && str[i] <= 'f'))
				return DT_NONE;
		}
		return (string ? DT_STRING : DT_INT);
	}

	// Validate Binary Numbers
	if (strstr(str, "0b") == str) {
		if ((len <= 2) || (len > max_bin_len))
			return DT_NONE;

		for (i = 2; i < len; i++) {
			if ((str[i] != '0') && (str[i] != '1'))
				return DT_NONE;
		}
		return (string ? DT_STRING : DT_INT);
	}

	// Validate Doubles
	ptr = strstr(str, ".");
	if (ptr) {
		if ((len <= 1) || (len > max_dbl_len))
			return DT_NONE;

		len = ptr - str;
		for (i = 0; i < len; i++) {
			if ((i == 0) && (str[0] == '-'))
				continue;

			if (!(str[i] >= '0' && str[i] <= '9'))
				return DT_NONE;
		}

		len = strlen(ptr) - 1;
		for (i = 1; i < len; i++) {
			if (!(ptr[i] >= '0' && ptr[i] <= '9'))
				return DT_NONE;
		}
		return (string ? DT_STRING : DT_FLOAT);
	}

	// Validate Ints
	if (((str[0] == '-') && (len > (max_int_len + 1))) || (len > max_int_len))
		return DT_NONE;

	for (i = 0; i < len; i++) {
		if ((i == 0) && (str[0] == '-'))
			continue;

		if (!(str[i] >= '0' && str[i] <= '9'))
			return DT_NONE;
	}

	return (string ? DT_STRING : DT_INT);
}

static bool validate_tokens(SOURCE_TOKEN_LIST *token_list) {
	int i;

	for (i = 0; i < token_list->num; i++) {

		// Validate That If/Repeat/Functions Have '('
		if ((token_list->token[i].type == TOKEN_IF) ||
			(token_list->token[i].type == TOKEN_REPEAT) ||
			(token_list->token[i].exp == EXP_FUNCTION) ) {

			if ((i == (token_list->num - 1)) || (token_list->token[i + 1].type != TOKEN_OPEN_PAREN)) {
				applog(LOG_ERR, "Syntax Error - Missing '('  Line: %d", token_list->token[i].line_num);
				return false;
			}
		}
	}

	return true;
}

extern bool get_token_list(char *str, SOURCE_TOKEN_LIST *token_list) {
	char c, *cmnt, literal[MAX_LITERAL_SIZE];
	int i, idx, len, token_id, line_num, token_list_sz, literal_idx;
	DATA_TYPE data_type;
	bool literal_str = false;

	token_list_sz = sizeof(epl_token) / sizeof(epl_token[0]);

	len = strlen(str);

	idx = 0;
	line_num = 1;
	literal_idx = 0;
	memset(literal, 0, 100);

	while (idx < len) {
		token_id = -1;
		c = str[idx];

		if (literal_idx > 0) {
			if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_'))
				literal_str = true;
			else
				literal_str = false;
		}
		
		if (!literal_str) {

			// Remove Whitespace
			if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f') {

				if (literal_idx > 0) {

					// Check If '-' Token Needs To Be Moved To Literal
					printf("SHIT %d\n",token_list->num);
					if (token_list->token[token_list->num - 1].type == TOKEN_NEG) {
						for (i = MAX_LITERAL_SIZE - 2; i > 0; i--)
							literal[i] = literal[i - 1];
						literal[0] = '-';

						// Remove '-' From Token List
						token_list->num--;
					}

					if (token_list->token[token_list->num - 1].type == TOKEN_FUNCTION) {
						data_type = DT_STRING;
					}
					else if (token_list->token[token_list->num - 2].type == TOKEN_CALL_FUNCTION) {
						data_type = DT_STRING;
					}
					else {
						data_type = validate_literal(literal);
						if (data_type == DT_NONE) {
							applog(LOG_ERR, "Syntax Error - Invalid Literal: '%s'  Line: %d", literal, line_num);
							return false;
						}
					}
					add_token(token_list, -1, literal, data_type, line_num);
					literal_idx = 0;
					memset(literal, 0, MAX_LITERAL_SIZE);
				}

				// Increment Line Number Counter
				if (c == '\n') {
					line_num++;
				}

				idx++;
				continue;
			}

			// Check For EPL Token
			for (i = 0; i < token_list_sz; i++) {

				if (memcmp(&str[idx], epl_token[i].str, epl_token[i].len) == 0) {
					token_id = i;
					break;
				}

			}
		}

		if (token_id >= 0) {

			// Remove Single Comments
			if (epl_token[token_id].type == TOKEN_COMMENT) {
				cmnt = strstr(&str[idx], "\n");
				if (cmnt)
					idx += &cmnt[0] - &str[idx] + 1;
				line_num++;
				continue;
			}

			// Remove Block Comments
			if (epl_token[token_id].type == TOKEN_BLOCK_COMMENT) {
				cmnt = strstr(&str[idx], "*/");
				if (!cmnt) {
					applog(LOG_ERR, "Syntax Error - Missing '*/'  Line: %d", line_num);
					return false;
				}

				// Count The Number Of Lines Skipped
				i = &cmnt[0] - &str[idx];
				while (i >= 0) {
					if (str[idx + i] == '\n')
						line_num++;
					i--;
				}

				idx += &cmnt[0] - &str[idx] + 2;
				continue;
			}

			// Add Literals To Token List
			if (literal_idx > 0) {

				// Check If '-' Token Needs To Be Moved To Literal
				if (token_list->token[token_list->num - 1].type == TOKEN_NEG) {
					for (i = MAX_LITERAL_SIZE - 2; i > 0; i--)
						literal[i] = literal[i - 1];
					literal[0] = '-';

					// Remove '-' From Token List
					token_list->num--;
				}

				if (epl_token[token_id].type == TOKEN_CALL_FUNCTION) {
					data_type = DT_STRING;
				}
				else if (token_list->token[token_list->num - 1].type == TOKEN_CALL_FUNCTION) {
					data_type = DT_STRING;
				}
				else {
					data_type = validate_literal(literal);
					if (data_type == DT_NONE) {
						applog(LOG_ERR, "Syntax Error - Invalid Literal: '%s'  Line: %d", literal, line_num);
						return false;
					}
				}

				add_token(token_list, -1, literal, data_type, line_num);
				literal_idx = 0;
				memset(literal, 0, 100);
			}

			add_token(token_list, token_id, NULL, DT_NONE, line_num);
			idx += epl_token[token_id].len;
		}
		else {
			literal[literal_idx] = c;
			literal_idx++;
			idx++;

			if (literal_idx > MAX_LITERAL_SIZE) {
				applog(LOG_ERR, "Syntax Error - Invalid Literal: '%s'  Line: %d", literal, line_num);
				return false;
			}
		}
	}

	if (!validate_tokens(token_list))
		return false;

	//if (opt_debug_epl)
	//	dump_token_list(token_list);

	return true;
}


// Temporary - For Debugging Only
static void dump_token_list(SOURCE_TOKEN_LIST *token_list)
{
	int i;

	fprintf(stdout, "\nNum\tLine\tToken\t\tToken ID\n");
	printf("----------------------------------------\n");
	for (i = 0; i < token_list->num; i++) {
		if (token_list->token[i].type == TOKEN_LITERAL)
			fprintf(stdout, "%d:\t%d\t%s\t\t%d\n", i, token_list->token[i].line_num, token_list->token[i].literal, token_list->token[i].type);
		else
			fprintf(stdout, "%d:\t%d\t%s\t\t%d\n", i, token_list->token[i].line_num, epl_token[token_list->token[i].token_id].str, token_list->token[i].type);
	}
}