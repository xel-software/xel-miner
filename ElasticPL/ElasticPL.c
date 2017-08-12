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

uint32_t ast_vm_ints = 0;
uint32_t ast_vm_uints = 0;
uint32_t ast_vm_longs = 0;
uint32_t ast_vm_ulongs = 0;
uint32_t ast_vm_floats = 0;
uint32_t ast_vm_doubles = 0;

uint32_t ast_storage_sz = 0;
uint32_t ast_storage_idx = 0;


extern bool create_epl_ast(char *source) {
	int i;
	SOURCE_TOKEN_LIST token_list;

	if (!source) {
		applog(LOG_ERR, "ERROR: Missing ElasticPL Source Code!");
		return false;
	}

	// Allocate Operator Stack
	if (!stack_op)
		stack_op = calloc(PARSE_STACK_SIZE * 2, sizeof(int));
	else
		memset(stack_op, 0, sizeof(stack_op));

	// Allocate Expression Stack
	if (!stack_exp)
		stack_exp = calloc(PARSE_STACK_SIZE, sizeof(ast));
	else {
		// Free Memory For Old String Values
		for (i = 0; i <= stack_exp_idx; i++) {
			if (stack_exp[i]->svalue)
				free(stack_exp[i]->svalue);
		}
		memset(stack_exp, 0, sizeof(stack_exp));
	}

	if (!stack_op || !stack_exp) {
		applog(LOG_ERR, "ERROR: Unable To Allocate VM Parser Stack!");
		return false;
	}

// TODO - Fix Token List Allocation

	if (!init_token_list(&token_list, TOKEN_LIST_SIZE)) {
		applog(LOG_ERR, "ERROR: Unable To Allocate Token List For Parser!");
		return false;
	}

	// Reset Stack Counters
	stack_op_idx = -1;
	stack_exp_idx = -1;
	top_op = -1;

	// Reset Global Variable Array Size
	ast_vm_ints = 0;
	ast_vm_uints = 0;
	ast_vm_longs = 0;
	ast_vm_ulongs = 0;
	ast_vm_floats = 0;
	ast_vm_doubles = 0;

	// Reset Storage Variables
	ast_storage_sz = 0;
	ast_storage_idx = 0;
	
	// Parse EPL Source Code Into Tokens
	if (!get_token_list(source, &token_list)) {
		return false;
	}

	// Parse Tokens Into AST
	if (!parse_token_list(&token_list)) {
		applog(LOG_ERR, "ERROR: Unable To Parse ElasticPL Tokens!");
		return false;
	}

	if (opt_debug_epl) {
		fprintf(stdout, "\n*********************************************************\n");
		fprintf(stdout, "AST Dump\n");
		fprintf(stdout, "*********************************************************\n");
		for (i = 0; i <= stack_exp_idx; i++) {
			dump_vm_ast(stack_exp[i]);
			fprintf(stdout, "---------------------------------------------------------\n");
		}
	}

	return true;
}

// Temporary - For Debugging Only
extern void dump_vm_ast(ast* root) {
	bool downward = true;
	ast *ast_ptr = NULL;

	if (!root)
		return;

	ast_ptr = root;

	if (root->type == NODE_FUNCTION) {
		printf("FUNCTION '%s'\n", root->svalue);
		printf("---------------------------------------------------------\n");
		ast_ptr = root->right;
	}

	while (ast_ptr) {

		// Navigate Down The Tree
		if (downward) {

			// Navigate To Lowest Left Node
			while (ast_ptr->left) {

				// Print Left Node
				if (!ast_ptr->right)
					print_node(ast_ptr);

				ast_ptr = ast_ptr->left;
			}

			// Print Build In Functions
			if (ast_ptr->exp == EXP_FUNCTION)
				print_node(ast_ptr);

			// If There Is A Right Node, Switch To It
			if (ast_ptr->right) {
				ast_ptr = ast_ptr->right;
			}
			// Otherwise, Print Current Node & Navigate Back Up The Tree
			else {
				print_node(ast_ptr);
				downward = false;
			}
		}

		// Navigate Back Up The Tree
		else {
			if (ast_ptr == root)
				break;

			// Check If We Need To Navigate Back Down A Right Branch
			if ((ast_ptr == ast_ptr->parent->left) && (ast_ptr->parent->right)) {
				print_node(ast_ptr->parent);
				ast_ptr = ast_ptr->parent->right;
				downward = true;
			}
			else {
				ast_ptr = ast_ptr->parent;
			}
		}
	}
}

static void print_node(ast* node) {
	char val[18];
	val[0] = 0;

	if (node->type == NODE_PARAM)
		return;

	switch (node->type) {
	case NODE_CONSTANT:
		if (node->is_float) {
			printf("\tType: %d,\t%f\t\t\t", node->type, node->fvalue);
		}
		else {
			if (node->is_signed)
				printf("\tType: %d,\t%lld\t\t\t", node->type, node->ivalue);
			else
				printf("\tType: %d,\t%llu\t\t\t", node->type, node->uvalue);
		}
		break;
	case NODE_VAR_CONST:
		if (node->is_float) {
			if (node->is_64bit)
				printf("\tType: %d,\td[%llu]\t\t\t", node->type, node->uvalue);
			else
				printf("\tType: %d,\tf[%llu]\t\t\t", node->type, node->uvalue);
		}
		else {
			if (node->is_64bit) {
				if (node->is_signed)
					printf("\tType: %d,\tl[%llu]\t\t\t", node->type, node->uvalue);
				else
					printf("\tType: %d,\tul[%llu]\t\t\t", node->type, node->uvalue);
			}
			else {
				if (node->is_signed) {
					printf("\tType: %d,\ti[%llu]\t\t\t", node->type, node->uvalue);
				}
				else {
					if (node->is_vm_mem)
						printf("\tType: %d,\tm[%llu]\t\t\t", node->type, node->uvalue);
					else
						printf("\tType: %d,\tu[%llu]\t\t\t", node->type, node->uvalue);
				}
			}
		}
		break;
	case NODE_VAR_EXP:
		if (node->is_float) {
			if (node->is_64bit)
				printf("\tType: %d,\td[x]\t\t\t", node->type);
			else
				printf("\tType: %d,\tf[x]\t\t\t", node->type);
		}
		else {
			if (node->is_64bit) {
				if (node->is_signed)
					printf("\tType: %d,\tl[x]\t\t\t", node->type);
				else
					printf("\tType: %d,\tul[x]\t\t\t", node->type);
			}
			else {
				if (node->is_signed) {
					printf("\tType: %d,\ti[x]\t\t\t", node->type);
				}
				else {
					if (node->is_vm_mem)
						printf("\tType: %d,\tm[x]\t\t\t", node->type);
					else
						printf("\tType: %d,\tu[x]\t\t\t", node->type);

				}
			}
		}
		break;
	case NODE_FUNCTION:
		printf("\tType: %d,\t%s %s\t\t", node->type, get_node_str(node->type), node->svalue);
		break;
	case NODE_CALL_FUNCTION:
		printf("\tType: %d,\t%s()\t\t", node->type, node->svalue);
		break;
	case NODE_REPEAT:
		printf("\tType: %d,\t%s (u[%llu], %lld)\t", node->type, get_node_str(node->type), node->uvalue, node->ivalue);
		break;
	case NODE_BLOCK:
		if (node->parent->type != NODE_FUNCTION)
			printf("\t-------------------------------------------------\n");
		return;
	case NODE_ARRAY_INT:
	case NODE_ARRAY_UINT:
	case NODE_ARRAY_LONG:
	case NODE_ARRAY_ULONG:
	case NODE_ARRAY_FLOAT:
	case NODE_ARRAY_DOUBLE:
		printf("\tType: %d,\t%s\t\t", node->type, get_node_str(node->type));
		break;
	default:
		printf("\tType: %d,\t%s\t\t\t", node->type, get_node_str(node->type));
		break;
	}

	switch (node->data_type) {
	case DT_INT:
		printf("(int)\n");
		break;
	case DT_UINT:
		printf("(uint)\n");
		break;
	case DT_LONG:
		printf("(long)\n");
		break;
	case DT_ULONG:
		printf("(ulong)\n");
		break;
	case DT_FLOAT:
		printf("(float)\n");
		break;
	case DT_DOUBLE:
		printf("(double)\n");
		break;
	case DT_STRING:
		printf("(string)\n");
		break;
	default:
		printf("(N/A)\n");
		break;
	}
}

extern char* get_node_str(NODE_TYPE node_type) {
	switch (node_type) {
	case NODE_ARRAY_INT:	return "array_int";
	case NODE_ARRAY_UINT:	return "array_uint";
	case NODE_ARRAY_LONG:	return "array_long";
	case NODE_ARRAY_ULONG:	return "array_ulong";
	case NODE_ARRAY_FLOAT:	return "array_float";
	case NODE_ARRAY_DOUBLE:	return "array_double";
	case NODE_STORAGE_SZ:	return "storage_sz";
	case NODE_STORAGE_IDX:	return "storage_idx";
	case NODE_CONSTANT:		return "";
	case NODE_VAR_CONST:	return "array[]";
	case NODE_VAR_EXP:		return "array[x]";
	case NODE_FUNCTION:		return "function";
	case NODE_CALL_FUNCTION:return "";
	case NODE_VERIFY_BTY:	return "verify_bty";
	case NODE_VERIFY_POW:	return "verify_pow";
	case NODE_ASSIGN:		return "=";
	case NODE_OR:			return "||";
	case NODE_AND:			return "&&";
	case NODE_BITWISE_OR:	return "|";
	case NODE_BITWISE_XOR:	return "^";
	case NODE_BITWISE_AND:	return "&";
	case NODE_EQ:			return "==";
	case NODE_NE:			return "!=";
	case NODE_LT:			return "<";
	case NODE_GT:			return ">";
	case NODE_LE:			return "<=";
	case NODE_GE:			return ">=";
	case NODE_INCREMENT_R:	return "++";
	case NODE_INCREMENT_L:	return "++";
	case NODE_ADD_ASSIGN:	return "+=";
	case NODE_SUB_ASSIGN:	return "-=";
	case NODE_MUL_ASSIGN:	return "*=";
	case NODE_DIV_ASSIGN:	return "/=";
	case NODE_MOD_ASSIGN:	return "%=";
	case NODE_LSHFT_ASSIGN:	return "<<=";
	case NODE_RSHFT_ASSIGN:	return ">>=";
	case NODE_AND_ASSIGN:	return "&=";
	case NODE_XOR_ASSIGN:	return "^=";
	case NODE_OR_ASSIGN:	return "|=";
	case NODE_ADD:			return "+";
	case NODE_DECREMENT_R:	return "--";
	case NODE_DECREMENT_L:	return "--";
	case NODE_SUB:			return "-";
	case NODE_NEG:			return "'-'";
	case NODE_MUL:			return "*";
	case NODE_DIV:			return "/";
	case NODE_MOD:			return "%";
	case NODE_RSHIFT:		return ">>";
	case NODE_LSHIFT:		return "<<";
	case NODE_RROT:			return ">>>";
	case NODE_LROT:			return "<<<";
	case NODE_COMPL:		return "~";
	case NODE_NOT:			return "!";
	case NODE_TRUE:			return "true";
	case NODE_FALSE:		return "false";
	case NODE_BLOCK:		return "{}";
	case NODE_IF:			return "if";
	case NODE_ELSE:			return "else";
	case NODE_REPEAT:		return "repeat";
	case NODE_BREAK:		return "break";
	case NODE_CONTINUE:		return "continue";
	case NODE_SIN:			return "sin";
	case NODE_COS:			return "cos";
	case NODE_TAN:			return "tan";
	case NODE_SINH:			return "sinh";
	case NODE_COSH:			return "cosh";
	case NODE_TANH:			return "tanh";
	case NODE_ASIN:			return "asin";
	case NODE_ACOS:			return "acos";
	case NODE_ATAN:			return "atan";
	case NODE_ATAN2:		return "atan2";
	case NODE_EXPNT:		return "exp";
	case NODE_LOG:			return "log";
	case NODE_LOG10:		return "log10";
	case NODE_POW:			return "pow";
	case NODE_SQRT:			return "sqrt";
	case NODE_CEIL:			return "ceil";
	case NODE_FLOOR:		return "floor";
	case NODE_ABS:			return "abs";
	case NODE_FABS:			return "fabs";
	case NODE_FMOD:			return "fmod";
	case NODE_GCD:			return "gcd";
	default: return "Unknown";
	}
}