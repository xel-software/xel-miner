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

uint32_t max_vm_ints = 0;
uint32_t max_vm_uints = 0;
uint32_t max_vm_longs = 0;
uint32_t max_vm_ulongs = 0;
uint32_t max_vm_floats = 0;
uint32_t max_vm_doubles = 0;


/*
#define NAV_LEFT_DOWN	0
#define NAV_RIGHT_DOWN	1
#define NAV_LEFT_UP		2
#define NAV_RIGHT_UP	3
#define AST_TREE_LEFT	0
#define AST_TREE_RIGHT	1
#define AST_NAV_UP		0
#define AST_NAV_DOWN	1

//#define NAV_UP		1
//#define NAV_LEFT	2
//#define NAV_RIGHT	3

static void get_next_node(ast *node, int *side, int *direction) {
	ast *node_tmp = NULL;

	if (!node)
		return NULL;

	node_tmp = node;

	// Navigate Down The Tree
	if (*direction == AST_NAV_DOWN) {

		// Navigate To Lowest Left Parent Node
		while (node_tmp->left) {
			node_tmp = node_tmp->left;
		}

		if (node_tmp != node) {
			return node_tmp;
		}

		// Switch To Right Node
		if (node->right) {
			node_tmp = node->right;
			while (node_tmp->left) {
				node_tmp = node_tmp->left;
			}

			if (node_tmp != node->right) {
				return node_tmp;
			}

			return node_tmp;
		}
		else {
			// Print Right Node & Navigate Back Up The Tree
			*direction == DIR_UP;
			return node_tmp->parent;
		}
	}

	// Navigate Back Up The Tree
	else {
		// Check If We Need To Navigate Back Down A Right Branch
		if ((node_tmp == node->parent->left) && (node->parent->right)) {
			*direction == DIR_DOWN;
			return node->parent->right;
		}
		else {
			return node->parent;
		}
	}
	return NULL;
}


extern void dump_vm_ast2(ast* root) {

	if (!root)
		return;

	ast *node = root;
	int direction = DIR_DOWN;

	while (node) {

		node = get_next_node(node, &direction);

		if (!node)
			break;

		// Print Bottom Nodes
		if (direction == DIR_DOWN) {
			if (node->left && !node->left->left && !node->left->right)
				print_node(node->left);
		}
		if (direction == DIR_UP) {
			if (node->right && !node->right->left && !node->right->right)
				print_node(node->right);
			print_node(node);
		}
	}
}

*/


extern bool create_epl_vm(char *source) {
	int i;
	SOURCE_TOKEN_LIST token_list;

	if (!source) {
		applog(LOG_ERR, "ERROR: Missing ElasticPL Source Code!");
		return false;
	}

	stack_op_idx = -1;
	stack_exp_idx = -1;
	top_op = -1;
	stack_op = calloc(PARSE_STACK_SIZE * 2, sizeof(int));
	stack_exp = calloc(PARSE_STACK_SIZE, sizeof(ast));

	if (!stack_op || !stack_exp) {
		applog(LOG_ERR, "ERROR: Unable To Allocate VM Parser Stack!");
		return false;
	}

	max_vm_ints = 0;
	max_vm_uints = 0;
	max_vm_longs = 0;
	max_vm_ulongs = 0;
	max_vm_floats = 0;
	max_vm_doubles = 0;

	if (!init_token_list(&token_list, TOKEN_LIST_SIZE)) {
		applog(LOG_ERR, "ERROR: Unable To Allocate Token List For Parser!");
		return false;
	}

	// Parse EPL Source Code Into Tokens
	if (!get_token_list(source, &token_list)) {
		return false;
	}

	// Parse Tokens Into AST
	if (!parse_token_list(&token_list)) {
		applog(LOG_ERR, "ERROR: Unable To Parse ElasticPL Tokens!");
		return false;
	}

	// Free VM Memory
	if (vm_ast)
		delete_epl_vm();

	// Copy Parsed Statements Into VM Array
	vm_ast_cnt = stack_exp_idx + 1;

	vm_ast = calloc(vm_ast_cnt, sizeof(ast*));
	memcpy(vm_ast, stack_exp, vm_ast_cnt * sizeof(ast*));

	if (opt_debug_epl) {
		fprintf(stdout, "--------------------------------\n");
		for (i = 0; i<vm_ast_cnt; i++) {
			dump_vm_ast(vm_ast[i]);
			fprintf(stdout, "--------------------------------\n");
		}
	}

	// Cleanup Stack Memory
	for (i = 0; i < vm_ast_cnt; i++) {
		if (stack_exp[i]->svalue)
			free(stack_exp[i]->svalue);
	}
	free(stack_exp);
	free(stack_op);

	if (!vm_ast) {
		applog(LOG_ERR, "ERROR: ElasticPL Parser Failed!");
		return false;
	}

	return true;
}

static bool delete_epl_vm() {
	int i;

	if (!vm_ast)
		return true;

	for (i = 0; i < vm_ast_cnt; i++) {
		if (vm_ast[i]->svalue)
			free(vm_ast[i]->svalue);
	}
	free(vm_ast);

	return true;
}

// Temporary - For Debugging Only
extern void dump_vm_ast(ast* root) {
	bool downward = true;
	ast *new_ptr = NULL;
	ast *old_ptr = NULL;

	if (!root)
		return;
	new_ptr = root;

	while (new_ptr) {
		old_ptr = new_ptr;

		// Navigate Down The Tree
		if (downward) {
			// Navigate To Lowest Left Parent Node
			while (new_ptr->left) {
				if (!new_ptr->left->left)
					break;
				new_ptr = new_ptr->left;
			}

			// Print Left Node
			if (new_ptr->left)
				print_node(new_ptr->left);

			// Switch To Right Node
			if (new_ptr->right) {
				new_ptr = new_ptr->right;
			}
			else {
				// Print Right Node & Navigate Back Up The Tree
				if (old_ptr != root) {
					print_node(new_ptr);
					new_ptr = old_ptr->parent;
				}
				downward = false;
			}
		}

		// Navigate Back Up The Tree
		else {
			if (new_ptr == root)
				break;

			// Print Parent Node
			print_node(new_ptr);

			// Check If We Need To Navigate Back Down A Right Branch
			if ((new_ptr == new_ptr->parent->left) && (new_ptr->parent->right)) {
				new_ptr = new_ptr->parent->right;
				downward = true;
			}
			else {
				new_ptr = old_ptr->parent;
			}
		}
	}

	// Display Root Node
	print_node(new_ptr);
}

static void print_node(ast* node) {
	char val[18];
	val[0] = 0;

	switch (node->type) {
	case NODE_CONSTANT:
		if (node->is_float) {
			printf("Type: %d,\t%f\n", node->type, node->fvalue);
		}
		else {
			if (node->is_signed)
				printf("Type: %d,\t%lld\n", node->type, node->ivalue);
			else
				printf("Type: %d,\t%llu\n", node->type, node->uvalue);
		}
		break;
	case NODE_VAR_CONST:
		if (node->is_float) {
			if (node->is_64bit)
				printf("Type: %d,\td[%llu]\n", node->type, node->uvalue);
			else
				printf("Type: %d,\tf[%llu]\n", node->type, node->uvalue);
		}
		else {
			if (node->is_64bit) {
				if (node->is_signed)
					printf("Type: %d,\tl[%llu]\n", node->type, node->uvalue);
				else
					printf("Type: %d,\tul[%llu]\n", node->type, node->uvalue);
			}
			else {
				if (node->is_signed)
					printf("Type: %d,\ti[%llu]\n", node->type, node->uvalue);
				else
					printf("Type: %d,\tu[%llu]\n", node->type, node->uvalue);
			}
		}
		break;
	case NODE_VAR_EXP:
		if (node->is_float) {
			if (node->is_64bit)
				printf("Type: %d,\tf[x]\n", node->type);
			else
				printf("Type: %d,\td[x]\n", node->type);
		}
		else {
			if (node->is_64bit) {
				if (node->is_signed)
					printf("Type: %d,\ti[x]\n", node->type);
				else
					printf("Type: %d,\tu[x]\n", node->type);
			}
			else {
				if (node->is_signed)
					printf("Type: %d,\tl[x]\n", node->type);
				else
					printf("Type: %d,\tul[x]\n", node->type);
			}
		}
		break;
	case NODE_FUNCTION:
		printf("Type: %d,\t%s %s()\n", node->type, get_node_str(node->type), node->svalue);
		break;
	default:
		printf("Type: %d,\t%s\n", node->type, get_node_str(node->type));
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
	case NODE_CONSTANT:		return "";
	case NODE_VAR_CONST:	return "array[]";
	case NODE_VAR_EXP:		return "array[x]";
	case NODE_FUNCTION:		return "function";
	case NODE_VERIFY:		return "verify";
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
	case NODE_RROT:			return "<<<";
	case NODE_LROT:			return ">>>";
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
	case NODE_PARAM:		return "param";
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