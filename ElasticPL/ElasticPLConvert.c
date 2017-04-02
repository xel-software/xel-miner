/*
* Copyright 2016 sprocket
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the Free
* Software Foundation; either version 2 of the License, or (at your option)
* any later version.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#include "ElasticPL.h"
#include "ElasticPLFunctions.h"
#include "../miner.h"

char *tab[] = { "\t", "\t\t", "\t\t\t", "\t\t\t\t", "\t\t\t\t\t", "\t\t\t\t\t\t", "\t\t\t\t\t\t\t", "\t\t\t\t\t\t\t" };
int tabs;

extern char* convert_ast_to_c() {
	char* ptr = NULL;
	char* code = NULL;
	char* code1 = NULL;
	char* code2 = NULL;

	int i, idx = 0;

	tabs = 0;
	use_elasticpl_init = false;
	use_elasticpl_math = false;

	// Check For "init_once", If Found Move Logic To Seperate Function
	if (vm_ast[0]->type == NODE_INIT_ONCE) {
		idx = 1;
		use_elasticpl_init = true;
		code1 = append_strings(code1, convert(vm_ast[0]->left));
	}

	for (i = idx; i < vm_ast_cnt; i++) {
		code2 = append_strings(code2, convert(vm_ast[i]));
	}

	if (!code2)
		return NULL;

	// Create C / OpenCL Code
	if (!opt_opencl) {

		char code2_end[] = "\n\treturn bounty_found;\n}\n\n";
		char code1_begin[] = "static void init_once() {\n\n";
		char code1_end[] = "}\n";

		// Check For Init Function
		if (code1) {
			code = malloc(strlen(code1) + strlen(code2) + strlen(code2_end) + strlen(code1_begin) + strlen(code1_end) + 1);
			ptr = &code[0];
			memcpy(ptr, code2, strlen(code2));
			ptr += strlen(code2);
			memcpy(ptr, &code2_end[0], strlen(code2_end));
			ptr += strlen(code2_end);
			memcpy(ptr, &code1_begin[0], strlen(code1_begin));
			ptr += strlen(code1_begin);
			memcpy(ptr, code1, strlen(code1));
			ptr += strlen(code1);
			memcpy(ptr, &code1_end[0], strlen(code1_end));
			ptr += strlen(code1_end);
			ptr[0] = 0; // Terminate String

			free(code1);
			free(code2);
		}
		else {
			code = malloc(strlen(code2) + strlen(code2_end) + strlen(code1_begin) + strlen(code1_end) + 1);
			ptr = &code[0];
			memcpy(ptr, code2, strlen(code2));
			ptr += strlen(code2);
			memcpy(ptr, &code2_end[0], strlen(code2_end));
			ptr += strlen(code2_end);
			memcpy(ptr, &code1_begin[0], strlen(code1_begin));
			ptr += strlen(code1_begin);
			memcpy(ptr, &code1_end[0], strlen(code1_end));
			ptr += strlen(code1_end);
			ptr[0] = 0; // Terminate String
			free(code2);
		}
	}

	// Create OpenCL Code
	else {

		char code2_end[] = "\n";
		char code1_begin[] = "if (1) {\n";
		char code1_end[] = "}\n\n";

		// Check For Init Function
		if (code1) {
			code = malloc(strlen(code1) + strlen(code2) + strlen(code2_end) + strlen(code1_begin) + strlen(code1_end) + 1);
			ptr = &code[0];
			memcpy(ptr, &code1_begin[0], strlen(code1_begin));
			ptr += strlen(code1_begin);
			memcpy(ptr, code1, strlen(code1));
			ptr += strlen(code1);
			memcpy(ptr, &code1_end[0], strlen(code1_end));
			ptr += strlen(code1_end);
			memcpy(ptr, code2, strlen(code2));
			ptr += strlen(code2);
			memcpy(ptr, &code2_end[0], strlen(code2_end));
			ptr += strlen(code2_end);
			ptr[0] = 0; // Terminate String

			free(code1);
			free(code2);
		}
		else {
			code = malloc(strlen(code2) + strlen(code2_end) + strlen(code1_begin) + strlen(code1_end) + 1);
			ptr = &code[0];
			memcpy(ptr, code2, strlen(code2));
			ptr += strlen(code2);
			memcpy(ptr, &code2_end[0], strlen(code2_end));
			ptr += strlen(code2_end);
			ptr[0] = 0; // Terminate String
			free(code2);
		}

	}
	return code;
}

// Use Post Order Traversal To Translate The Expressions In The AST to C
static char* convert(ast* exp) {
	char *lval = NULL;
	char *rval = NULL;
	char *tmp = NULL;
	char *result = NULL;

	bool l_is_float = false;
	bool r_is_float = false;

	if (exp != NULL) {

		// Determine Tab Indentations
		if (exp->type == NODE_REPEAT) {
			if (tabs < 6) tabs += 2;
		}
		else if (exp->type == NODE_IF) {
			if (tabs < 7) tabs++;
		}

		// Process Left Side Statements
		if (exp->left != NULL) {
			lval = convert(exp->left);
		}

		// Check For If Statement As Right Side Is Conditional
		if ((exp->type != NODE_IF) && (exp->type != NODE_CONDITIONAL))
			rval = convert(exp->right);

		// Check If Leafs Are Float Or Int To Determine If Casting Is Needed
		if (exp->left != NULL)
			l_is_float = (exp->left->is_float);
		if (exp->right != NULL)
			r_is_float = (exp->right->is_float);

		// Allocate Memory For Results
		result = malloc((lval ? strlen(lval) : 0) + (rval ? 3 * strlen(rval) : 0) + 256);
		result[0] = 0;

		switch (exp->type) {
		case NODE_CONSTANT:
			if (exp->data_type == DT_INT)
				sprintf(result, "%d", exp->value);
			else if (exp->data_type == DT_FLOAT)
				sprintf(result, "%f", exp->fvalue);
			else if (exp->data_type == DT_STRING)
				sprintf(result, "%s", exp->svalue);
			break;
		case NODE_VAR_CONST:
			if (exp->value < 0 || exp->value > VM_MEMORY_SIZE) {
				if (exp->data_type == DT_INT)
					sprintf(result, "m[0]");
				else if (exp->data_type == DT_FLOAT)
					sprintf(result, "f[0]");
				else
					sprintf(result, "0");
			}
			else {
				if (exp->data_type == DT_INT)
					sprintf(result, "m[%lu]", exp->value);
				else if (exp->data_type == DT_FLOAT)
					sprintf(result, "f[%lu]", exp->value);
				else
					sprintf(result, "//");
			}
			break;
		case NODE_VAR_EXP:
			if (exp->data_type == DT_INT)
				sprintf(result, "m[%s]", lval);
			else if (exp->data_type == DT_FLOAT)
				sprintf(result, "f[%s]", lval);
			else
				sprintf(result, "//");
			break;
		case NODE_ASSIGN:
			if (l_is_float && !r_is_float)
				sprintf(result, "%s = (double)(%s);\n%smangle_state((int)%s%s)", lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			else if (!l_is_float && r_is_float)
				sprintf(result, "%s = (int)(%s);\n%smangle_state(%s%s)", lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			else
				sprintf(result, "%s = %s;\n%smangle_state(%s%s%s)", lval, rval, tab[tabs], (l_is_float ? "(int)" : ""), lval, (opt_opencl ? ", &vm_state[0]" : ""));
			break;
		case NODE_IF:
			if (tabs < 1) tabs = 1;
			if (exp->right->type != NODE_ELSE) {
				rval = convert(exp->right);				// If Body (No Else Condition)
				result = realloc(result, (lval ? strlen(lval) : 0) + (rval ? strlen(rval) : 0) + 256);
				sprintf(result, "%sif( %s ) {\n%s%s%s}\n", tab[tabs - 1], lval, (rval[0] == '\t' ? "" : tab[tabs]), rval, tab[tabs - 1]);
			}
			else {
				tmp = lval;
				lval = convert(exp->right->left);		// If Body
				rval = convert(exp->right->right);		// Else Body
				result = realloc(result, (lval ? strlen(lval) : 0) + (rval ? strlen(rval) : 0) + 256);
				sprintf(result, "%sif( %s ) {\n%s%s%s}\n%selse {\n%s%s%s}\n", tab[tabs - 1], tmp, (lval[0] == '\t' ? "" : tab[tabs]), lval, tab[tabs - 1], tab[tabs - 1], (rval[0] == '\t' ? "" : tab[tabs]), rval, tab[tabs - 1]);
			}
			if (tabs) tabs--;
			break;
		case NODE_CONDITIONAL:
			tmp = lval;
			lval = convert(exp->right->left);		// If Body
			rval = convert(exp->right->right);		// Else Body
			result = realloc(result, (lval ? strlen(lval) : 0) + (rval ? strlen(rval) : 0) + 256);
			sprintf(result, "(( %s ) ? %s : %s)", tmp, lval, rval);
			break;
		case NODE_COND_ELSE:
		case NODE_ELSE:
			break;
		case NODE_REPEAT:
			if (tabs < 2) tabs = 2;
			if (exp->left->type == NODE_CONSTANT)
				sprintf(result, "%sif ( %s > 0 ) {\n%sint loop%d;\n%sfor (loop%d = 0; loop%d < ( %s ); loop%d++) {\n%s%s%s}\n%s}\n", tab[tabs - 2], lval, tab[tabs - 1], exp->token_num, tab[tabs - 1], exp->token_num, exp->token_num, lval, exp->token_num, "", rval, tab[tabs - 1], tab[tabs - 2]);
			else
				sprintf(result, "%sif ( %s > 0 ) {\n%sint loop%d;\n%sfor (loop%d = 0; loop%d < ( %s ); loop%d++) {\n%sif (loop%d >= %d) break;\n%s%s%s}\n%s}\n", tab[tabs - 2], lval, tab[tabs - 1], exp->token_num, tab[tabs - 1], exp->token_num, exp->token_num, lval, exp->token_num, tab[tabs - 1], exp->token_num, exp->value, "", rval, tab[tabs - 1], tab[tabs - 2]);
			if (tabs > 1) tabs -= 2;
			break;
		case NODE_BREAK:
			sprintf(result, "break");
			break;
		case NODE_CONTINUE:
			sprintf(result, "continue");
			break;
		case NODE_BLOCK:
			if (lval[0] != '\t')
				sprintf(result, "%s%s%s", tab[tabs], lval, (rval ? rval : ""));
			else
				sprintf(result, "%s%s", lval, (rval ? rval : ""));
			break;
		case NODE_INCREMENT_R:
			if (exp->end_stmnt) {
				sprintf(result, "++%s;\n%smangle_state(%s%s%s)", lval, tab[tabs], (l_is_float ? "(int)" : ""), lval, (opt_opencl ? ", &vm_state[0]" : ""));
			}
			else
				sprintf(result, "++%s", lval);
			exp->is_float = l_is_float;
			break;
		case NODE_INCREMENT_L:
			if (exp->end_stmnt) {
				sprintf(result, "%s++;\n%smangle_state(%s%s%s)", lval, tab[tabs], (l_is_float ? "(int)" : ""), lval, (opt_opencl ? ", &vm_state[0]" : ""));
			}
			else
				sprintf(result, "%s++", lval);
			exp->is_float = l_is_float;
			break;
		case NODE_ADD:
			if (l_is_float && !r_is_float)
				sprintf(result, "(%s + (double)(%s))", lval, rval);
			else if (!l_is_float && r_is_float)
				sprintf(result, "((double)(%s) + %s)", lval, rval);
			else
				sprintf(result, "(%s + %s)", lval, rval);
			exp->is_float = l_is_float | r_is_float;
			break;
		case NODE_ADD_ASSIGN:
			if (l_is_float && !r_is_float)
				sprintf(result, "%s += (double)(%s);\n%smangle_state((int)%s%s)", lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			else if (!l_is_float && r_is_float)
				sprintf(result, "%s += (int)(%s);\n%smangle_state(%s%s)", lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			else
				sprintf(result, "%s += %s;\n%smangle_state(%s%s%s)", lval, rval, tab[tabs], (l_is_float ? "(int)" : ""), lval, (opt_opencl ? ", &vm_state[0]" : ""));
			break;
		case NODE_SUB_ASSIGN:
			if (l_is_float && !r_is_float)
				sprintf(result, "%s -= (double)(%s);\n%smangle_state((int)%s%s)", lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			else if (!l_is_float && r_is_float)
				sprintf(result, "%s -= (int)(%s);\n%smangle_state(%s%s)", lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			else
				sprintf(result, "%s -= %s;\n%smangle_state(%s%s%s)", lval, rval, tab[tabs], (l_is_float ? "(int)" : ""), lval, (opt_opencl ? ", &vm_state[0]" : ""));
			break;
		case NODE_MUL_ASSIGN:
			if (l_is_float && !r_is_float)
				sprintf(result, "%s *= (double)(%s);\n%smangle_state((int)%s%s)", lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			else if (!l_is_float && r_is_float)
				sprintf(result, "%s *= (int)(%s);\n%smangle_state(%s%s)", lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			else
				sprintf(result, "%s *= %s;\n%smangle_state(%s%s%s)", lval, rval, tab[tabs], (l_is_float ? "(int)" : ""), lval, (opt_opencl ? ", &vm_state[0]" : ""));
			break;
		case NODE_DIV_ASSIGN:
			if (!l_is_float && !r_is_float)
				sprintf(result, "%s = ((%s != 0) ? (int)((double)(%s) / (double)(%s)) : 0);\n%smangle_state(%s%s)", lval, rval, lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			else if (l_is_float && !r_is_float)
				sprintf(result, "%s = ((%s != 0) ? %s / (double)(%s) : 0.0);\n%smangle_state((int)%s%s)", lval, rval, lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			else if (!l_is_float && r_is_float)
				sprintf(result, "%s = ((%s != 0.0) ? (int)((double)(%s) / %s) : 0);\n%smangle_state(%s%s)", lval, rval, lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			else
				sprintf(result, "%s = ((%s != 0.0) ? %s / %s : 0.0);\n%smangle_state((int)%s%s)", lval, rval, lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			break;
		case NODE_MOD_ASSIGN:
			if (!l_is_float && !r_is_float)
				sprintf(result, "%s = ((%s != 0) ? %s %% %s : 0);\n%smangle_state(%s%s)", lval, rval, lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			else if (l_is_float && !r_is_float)
				sprintf(result, "%s = (double)((%s != 0) ? (int)(%s) %% %s : 0);\n%smangle_state((int)%s%s)", lval, rval, lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			else if (!l_is_float && r_is_float)
				sprintf(result, "%s = (((int)(%s) != 0) ? %s %% (int)(%s) : 0);\n%smangle_state(%s%s)", lval, rval, lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			else
				sprintf(result, "%s = (double)(((int)(%s) != 0) ? (int)(%s) %% (int)(%s) : 0);\n%smangle_state((int)%s%s)", lval, rval, lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			break;
		case NODE_LSHFT_ASSIGN:
			if (!l_is_float && !r_is_float)
				sprintf(result, "%s = %s << %s;\n%smangle_state(%s%s)", lval, lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			else if (l_is_float && !r_is_float)
				sprintf(result, "%s = (double)((int)(%s) << %s);\n%smangle_state((int)%s%s)", lval, lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			else if (!l_is_float && r_is_float)
				sprintf(result, "%s = %s << (int)(%s);\n%smangle_state(%s%s)", lval, lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			else
				sprintf(result, "%s = (double)((int)(%s) << (int)(%s));\n%smangle_state((int)%s%s)", lval, lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			break;
		case NODE_RSHFT_ASSIGN:
			if (!l_is_float && !r_is_float)
				sprintf(result, "%s = %s >> %s;\n%smangle_state(%s%s)", lval, lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			else if (l_is_float && !r_is_float)
				sprintf(result, "%s = (double)((int)(%s) >> %s);\n%smangle_state((int)%s%s)", lval, lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			else if (!l_is_float && r_is_float)
				sprintf(result, "%s = %s >> (int)(%s);\n%smangle_state(%s%s)", lval, lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			else
				sprintf(result, "%s = (double)((int)(%s) >> (int)(%s));\n%smangle_state((int)%s%s)", lval, lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			break;
		case NODE_AND_ASSIGN:
			if (!l_is_float && !r_is_float)
				sprintf(result, "%s = %s & %s;\n%smangle_state(%s%s)", lval, lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			else if (l_is_float && !r_is_float)
				sprintf(result, "%s = (double)((int)(%s) & %s);\n%smangle_state((int)%s%s)", lval, lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			else if (!l_is_float && r_is_float)
				sprintf(result, "%s = %s & (int)(%s);\n%smangle_state(%s%s)", lval, lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			else
				sprintf(result, "%s = (double)((int)(%s) & (int)(%s));\n%smangle_state((int)%s%s)", lval, lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			break;
		case NODE_XOR_ASSIGN:
			if (!l_is_float && !r_is_float)
				sprintf(result, "%s = %s ^ %s;\n%smangle_state(%s%s)", lval, lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			else if (l_is_float && !r_is_float)
				sprintf(result, "%s = (double)((int)(%s) ^ %s);\n%smangle_state((int)%s%s)", lval, lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			else if (!l_is_float && r_is_float)
				sprintf(result, "%s = %s ^ (int)(%s);\n%smangle_state(%s%s)", lval, lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			else
				sprintf(result, "%s = (double)((int)(%s) ^ (int)(%s));\n%smangle_state((int)%s%s)", lval, lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			break;
		case NODE_OR_ASSIGN:
			if (!l_is_float && !r_is_float)
				sprintf(result, "%s = %s | %s;\n%smangle_state(%s%s)", lval, lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			else if (l_is_float && !r_is_float)
				sprintf(result, "%s = (double)((int)(%s) | %s);\n%smangle_state((int)%s%s)", lval, lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			else if (!l_is_float && r_is_float)
				sprintf(result, "%s = %s | (int)(%s);\n%smangle_state(%s%s)", lval, lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			else
				sprintf(result, "%s = (double)((int)(%s) | (int)(%s));\n%smangle_state((int)%s%s)", lval, lval, rval, tab[tabs], lval, (opt_opencl ? ", &vm_state[0]" : ""));
			break;
		case NODE_DECREMENT_R:
			if (exp->end_stmnt) {
				sprintf(result, "--%s;\n%smangle_state(%s%s%s)", lval, tab[tabs], (l_is_float ? "(int)" : ""), lval, (opt_opencl ? ", &vm_state[0]" : ""));
			}
			else
				sprintf(result, "--%s", lval);
			exp->is_float = l_is_float;
			break;
		case NODE_DECREMENT_L:
			if (exp->end_stmnt) {
				sprintf(result, "%s--;\n%smangle_state(%s%s%s)", lval, tab[tabs], (l_is_float ? "(int)" : ""), lval, (opt_opencl ? ", &vm_state[0]" : ""));
			}
			else
				sprintf(result, "%s--", lval);
			exp->is_float = l_is_float;
			break;
		case NODE_SUB:
			if (l_is_float && !r_is_float)
				sprintf(result, "(%s - (double)(%s))", lval, rval);
			else if (!l_is_float && r_is_float)
				sprintf(result, "((double)(%s) - %s)", lval, rval);
			else
				sprintf(result, "(%s - %s)", lval, rval);
			exp->is_float = l_is_float | r_is_float;
			break;
		case NODE_MUL:
			if (l_is_float && !r_is_float)
				sprintf(result, "(%s * (double)(%s))", lval, rval);
			else if (!l_is_float && r_is_float)
				sprintf(result, "((double)(%s) * %s)", lval, rval);
			else
				sprintf(result, "(%s * %s)", lval, rval);
			exp->is_float = l_is_float | r_is_float;
			break;
		case NODE_DIV:
			if (!l_is_float && !r_is_float)
				sprintf(result, "((%s != 0) ? (double)(%s) / (double)(%s) : 0.0)", rval, lval, rval);
			else if (l_is_float && !r_is_float)
				sprintf(result, "((%s != 0) ? %s / (double)(%s) : 0.0)", rval, lval, rval);
			else if (!l_is_float && r_is_float)
				sprintf(result, "((%s != 0.0) ? (double)(%s) / %s : 0.0)", rval, lval, rval);
			else
				sprintf(result, "((%s != 0.0) ? %s / %s : 0.0)", rval, lval, rval);
			exp->is_float = true;
			break;
		case NODE_MOD:
			if (l_is_float && r_is_float)
				sprintf(result, "(((int)(%s) > 0) ? (int)(%s) %% (int)(%s) : 0)", rval, lval, rval);
			else if (l_is_float && !r_is_float)
				sprintf(result, "((%s > 0) ? (int)(%s) %% %s : 0)", rval, lval, rval);
			else if (!l_is_float && r_is_float)
				sprintf(result, "(((int)(%s) > 0) ? %s %% (int)(%s) : 0)", rval, lval, rval);
			else
				sprintf(result, "((%s > 0) ? %s %% %s : 0)", rval, lval, rval);
			exp->is_float = false;
			break;
		case NODE_LSHIFT:
			if (l_is_float && r_is_float)
				sprintf(result, "((int)(%s) << (int)(%s))", lval, rval);
			else if (l_is_float && !r_is_float)
				sprintf(result, "((int)(%s) << %s)", lval, rval);
			else if (!l_is_float && r_is_float)
				sprintf(result, "(%s << (int)(%s))", lval, rval);
			else
				sprintf(result, "(%s << %s)", lval, rval);
			exp->is_float = false;
			break;
		case NODE_LROT:
			if (l_is_float && r_is_float)
				sprintf(result, "rotl32( (int)(%s), (int)(%s) %% 32)", lval, rval);
			else if (l_is_float && !r_is_float)
				sprintf(result, "rotl32( (int)(%s), %s %% 32)", lval, rval);
			else if (!l_is_float && r_is_float)
				sprintf(result, "rotl32( %s, (int)(%s) %% 32)", lval, rval);
			else
				sprintf(result, "rotl32( %s, %s %% 32)", lval, rval);
			exp->is_float = false;
			break;
		case NODE_RSHIFT:
			if (l_is_float && r_is_float)
				sprintf(result, "((int)(%s) >> (int)(%s))", lval, rval);
			else if (l_is_float && !r_is_float)
				sprintf(result, "((int)(%s) >> %s)", lval, rval);
			else if (!l_is_float && r_is_float)
				sprintf(result, "(%s >> (int)(%s))", lval, rval);
			else
				sprintf(result, "(%s >> %s)", lval, rval);
			exp->is_float = false;
			break;
		case NODE_RROT:
			if (l_is_float && r_is_float)
				sprintf(result, "rotr32( (int)(%s), (int)(%s) %% 32)", lval, rval);
			else if (l_is_float && !r_is_float)
				sprintf(result, "rotr32( (int)(%s), %s %% 32)", lval, rval);
			else if (!l_is_float && r_is_float)
				sprintf(result, "rotr32( %s, (int)(%s) %% 32)", lval, rval);
			else
				sprintf(result, "rotr32( %s, %s %% 32)", lval, rval);
			exp->is_float = false;
			break;
		case NODE_NOT:
			sprintf(result, "!(%s)", lval);
			exp->is_float = l_is_float;
			break;
		case NODE_COMPL:
			sprintf(result, "~(%s)", lval);
			exp->is_float = l_is_float;
			break;
		case NODE_AND:
			if (l_is_float && !r_is_float)
				sprintf(result, "(%s && (double)(%s))", lval, rval);
			else if (!l_is_float && r_is_float)
				sprintf(result, "(%s && (int)(%s))", lval, rval);
			else
				sprintf(result, "(%s && %s)", lval, rval);
			exp->is_float = false;
			break;
		case NODE_OR:
			if (l_is_float && !r_is_float)
				sprintf(result, "(%s || (double)(%s))", lval, rval);
			else if (!l_is_float && r_is_float)
				sprintf(result, "(%s || (int)(%s))", lval, rval);
			else
				sprintf(result, "(%s || %s)", lval, rval);
			exp->is_float = false;
			break;
		case NODE_BITWISE_AND:
			if (l_is_float && r_is_float)
				sprintf(result, "((int)(%s) & (int)(%s))", lval, rval);
			else if (l_is_float && !r_is_float)
				sprintf(result, "((int)(%s) & %s)", lval, rval);
			else if (!l_is_float && r_is_float)
				sprintf(result, "(%s & (int)(%s))", lval, rval);
			else
				sprintf(result, "(%s & %s)", lval, rval);
			exp->is_float = false;
			break;
		case NODE_BITWISE_XOR:
			if (l_is_float && r_is_float)
				sprintf(result, "((int)(%s) ^ (int)(%s))", lval, rval);
			else if (l_is_float && !r_is_float)
				sprintf(result, "((int)(%s) ^ %s)", lval, rval);
			else if (!l_is_float && r_is_float)
				sprintf(result, "(%s ^ (int)(%s))", lval, rval);
			else
				sprintf(result, "(%s ^ %s)", lval, rval);
			exp->is_float = false;
			break;
		case NODE_BITWISE_OR:
			if (l_is_float && r_is_float)
				sprintf(result, "((int)(%s) | (int)(%s))", lval, rval);
			else if (l_is_float && !r_is_float)
				sprintf(result, "((int)(%s) | %s)", lval, rval);
			else if (!l_is_float && r_is_float)
				sprintf(result, "(%s | (int)(%s))", lval, rval);
			else
				sprintf(result, "(%s | %s)", lval, rval);
			exp->is_float = false;
			break;
		case NODE_EQ:
			if (l_is_float && !r_is_float)
				sprintf(result, "(%s == (double)(%s))", lval, rval);
			else if (!l_is_float && r_is_float)
				sprintf(result, "((double)(%s) == %s)", lval, rval);
			else
				sprintf(result, "(%s == %s)", lval, rval);
			exp->is_float = false;
			break;
		case NODE_NE:
			if (l_is_float && !r_is_float)
				sprintf(result, "(%s != (double)(%s))", lval, rval);
			else if (!l_is_float && r_is_float)
				sprintf(result, "((double)(%s) != %s)", lval, rval);
			else
				sprintf(result, "(%s != %s)", lval, rval);
			exp->is_float = false;
			break;
		case NODE_GT:
			if (l_is_float && !r_is_float)
				sprintf(result, "(%s > (double)(%s))", lval, rval);
			else if (!l_is_float && r_is_float)
				sprintf(result, "((double)(%s) > %s)", lval, rval);
			else
				sprintf(result, "(%s > %s)", lval, rval);
			exp->is_float = false;
			break;
		case NODE_LT:
			if (l_is_float && !r_is_float)
				sprintf(result, "(%s < (double)(%s))", lval, rval);
			else if (!l_is_float && r_is_float)
				sprintf(result, "((double)(%s) < %s)", lval, rval);
			else
				sprintf(result, "(%s < %s)", lval, rval);
			exp->is_float = false;
			break;
		case NODE_GE:
			if (l_is_float && !r_is_float)
				sprintf(result, "(%s >= (double)(%s))", lval, rval);
			else if (!l_is_float && r_is_float)
				sprintf(result, "((double)(%s) >= %s)", lval, rval);
			else
				sprintf(result, "(%s >= %s)", lval, rval);
			exp->is_float = false;
			break;
		case NODE_LE:
			if (l_is_float && !r_is_float)
				sprintf(result, "(%s <= (double)(%s))", lval, rval);
			else if (!l_is_float && r_is_float)
				sprintf(result, "((double)(%s) <= %s)", lval, rval);
			else
				sprintf(result, "(%s <= %s)", lval, rval);
			exp->is_float = false;
			break;
		case NODE_NEG:
			sprintf(result, "-(%s)", lval);
			exp->is_float = l_is_float;
			break;
		case NODE_VERIFY:
			sprintf(result, "\n\tbounty_found = (%s != 0 ? 1 : 0)", lval);
			break;
		case NODE_PARAM:
			if (rval)
				sprintf(result, "%s, %s", lval, rval);
			else
				sprintf(result, "%s", lval);
			break;
		case NODE_SIN:
			sprintf(result, "sin( %s )", rval);
			exp->is_float = true;
			use_elasticpl_math = true;
			break;
		case NODE_COS:
			sprintf(result, "cos( %s )", rval);
			exp->is_float = true;
			use_elasticpl_math = true;
			break;
		case NODE_TAN:
			sprintf(result, "tan( %s )", rval);
			exp->is_float = true;
			use_elasticpl_math = true;
			break;
		case NODE_SINH:
			sprintf(result, "(((%s >= -1.0) && (%s <= 1.0)) ? sinh( %s ) : 0.0)", rval, rval, rval);
			exp->is_float = true;
			use_elasticpl_math = true;
			break;
		case NODE_COSH:
			sprintf(result, "(((%s >= -1.0) && (%s <= 1.0)) ? cosh( %s ) : 0.0)", rval, rval, rval);
			exp->is_float = true;
			use_elasticpl_math = true;
			break;
		case NODE_TANH:
			sprintf(result, "tanh( %s )", rval);
			exp->is_float = true;
			use_elasticpl_math = true;
			break;
		case NODE_ASIN:
			sprintf(result, "(((%s >= -1.0) && (%s <= 1.0)) ? asin( %s ) : 0.0)", rval, rval, rval);
			exp->is_float = true;
			use_elasticpl_math = true;
			break;
		case NODE_ACOS:
			sprintf(result, "(((%s >= -1.0) && (%s <= 1.0)) ? acos( %s ) : 0.0)", rval, rval, rval);
			exp->is_float = true;
			use_elasticpl_math = true;
			break;
		case NODE_ATAN:
			sprintf(result, "atan( %s )", rval);
			exp->is_float = true;
			use_elasticpl_math = true;
			break;
		case NODE_ATAN2:
			tmp = strstr(rval, ",");	// Point To Second Argurment
			sprintf(result, "((%s != 0) ? atan2( %s ) : 0.0)", tmp + 1, rval);
			tmp = NULL;
			exp->is_float = true;
			use_elasticpl_math = true;
			break;
		case NODE_EXPNT:
			sprintf(result, "((((%s) >= -708.0) && ((%s) <= 709.0)) ? exp( %s ) : 0.0)", rval, rval, rval);
			exp->is_float = true;
			use_elasticpl_math = true;
			break;
		case NODE_LOG:
			sprintf(result, "((%s > 0) ? log( %s ) : 0.0)", rval, rval);
			exp->is_float = true;
			use_elasticpl_math = true;
			break;
		case NODE_LOG10:
			sprintf(result, "((%s > 0) ? log10( %s ) : 0.0)", rval, rval);
			exp->is_float = true;
			use_elasticpl_math = true;
			break;
		case NODE_POW:
			sprintf(result, "pow( %s )", rval);
			exp->is_float = true;
			use_elasticpl_math = true;
			break;
		case NODE_SQRT:
			sprintf(result, "((%s > 0) ? sqrt( %s ) : 0.0)", rval, rval);
			exp->is_float = true;
			use_elasticpl_math = true;
			break;
		case NODE_CEIL:
			sprintf(result, "ceil( %s )", rval);
			exp->is_float = true;
			use_elasticpl_math = true;
			break;
		case NODE_FLOOR:
			sprintf(result, "floor( %s )", rval);
			exp->is_float = true;
			use_elasticpl_math = true;
			break;
		case NODE_ABS:
			sprintf(result, "abs( %s )", rval);
			exp->is_float = false;
			use_elasticpl_math = true;
			break;
		case NODE_FABS:
			sprintf(result, "fabs( %s )", rval);
			exp->is_float = true;
			use_elasticpl_math = true;
			break;
		case NODE_FMOD:
			tmp = strstr(rval, ",");	// Point To Second Argurment
			sprintf(result, "((%s != 0) ? fmod( %s ) : 0.0)", tmp + 1, rval);
			tmp = NULL;
			exp->is_float = true;
			use_elasticpl_math = true;
			break;
		case NODE_GCD:
			sprintf(result, "gcd( %s )", rval);
			exp->is_float = true;
			use_elasticpl_math = true;
			break;
		default:
			sprintf(result, "fprintf(stderr, \"ERROR: VM Runtime - Unsupported Operation (%d)\");\n", exp->type);
		}

		if (lval) free(lval);
		if (rval) free(rval);
		if (tmp) free(tmp);

		// Terminate Statements
		if (exp->end_stmnt && (exp->type != NODE_IF) && (exp->type != NODE_REPEAT) && (exp->type != NODE_BLOCK)) {
			tmp = malloc(strlen(result) + 20);
			sprintf(tmp, "%s%s;\n", tab[tabs], result);
			free(result);
			result = tmp;
		}
	}

	return result;
}

static char* append_strings(char * old, char * new) {

	char* out = NULL;

	if (new == NULL && old != NULL) {
		out = calloc(strlen(old) + 1, sizeof(char));
		strcpy(out, old);
	}
	else if (old == NULL && new != NULL) {
		out = calloc(strlen(new) + 1, sizeof(char));
		strcpy(out, new);
	}
	else if (new == NULL && old == NULL) {
		// pass
	}
	else {
		// find the size of the string to allocate
		const size_t old_len = strlen(old), new_len = strlen(new);
		const size_t out_len = old_len + new_len + 1;
		// allocate a pointer to the new string
		out = malloc(out_len);
		// concat both strings and return
		memcpy(out, old, old_len);
		strcpy(out + old_len, new);
	}

	// Free here
	if (old != NULL) {
		free(old);
		old = NULL;
	}
	if (new != NULL) {
		free(new);
		new = NULL;
	}

	return out;
}
