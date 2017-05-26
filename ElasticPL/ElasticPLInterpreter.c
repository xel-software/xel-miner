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
#include <math.h>

#include "ElasticPL.h"
#include "ElasticPLFunctions.h"
#include "../miner.h"

extern uint32_t calc_wcet() {
	int i, call_depth = 0;
	uint32_t ast_depth, wcet;

	// Get Max Function Call Depth
	for (i = ast_func_idx; i <= stack_exp_idx; i++) {
		if (stack_exp[i]->uvalue > call_depth)
			call_depth = stack_exp[i]->uvalue;
	}

	// Calculate WCET For Each Function Beginning With The Lowest One In Call Stack
	while (call_depth >= 0) {
		for (i = ast_func_idx; i <= stack_exp_idx; i++) {
			if (stack_exp[i]->uvalue == call_depth) {
				ast_depth = 0;
				wcet = calc_function_weight(stack_exp[i], &ast_depth);
				applog(LOG_DEBUG, "DEBUG: Function '%s' WCET = %lu,\tDepth = %lu", stack_exp[i]->svalue, wcet, ast_depth);

				if (ast_depth > MAX_AST_DEPTH) {
					applog(LOG_ERR, "ERROR: Max allowed AST depth exceeded (%lu)", ast_depth);
					return 0;
				}

				// Store WCET Value In Function's 'fvalue' Field
				stack_exp[i]->fvalue = wcet;
			}
		}
		call_depth--;
	}

	applog(LOG_DEBUG, "DEBUG: Total WCET = %lu", (uint32_t)stack_exp[ast_main_idx]->fvalue);

	return (uint32_t)stack_exp[ast_main_idx]->fvalue;
}

static uint32_t calc_function_weight(ast* root, uint32_t *ast_depth) {
	uint32_t depth = 0, weight = 0, total_weight = 0;
	uint32_t block_weight[REPEAT_STACK_SIZE];
	int block_level = -1;
	bool downward = true;
	ast *ast_ptr = NULL;

	if (!root)
		return 0;

	ast_ptr = root;
	depth = 1;

	while (ast_ptr) {
		weight = 0;

		// Navigate Down The Tree
		if (downward) {

			// Navigate To Lowest Left Node
			while (ast_ptr->left) {
				ast_ptr = ast_ptr->left;

				// Check For "Repeat" Blocks
				if (ast_ptr->type == NODE_REPEAT) {
					total_weight += weight;
					block_level++;
					block_weight[block_level] = 0;
					break;
				}

				if (++depth > *ast_depth) *ast_depth = depth;
			}

			// If There Is A Right Node, Switch To It
			if (ast_ptr->right) {
				ast_ptr = ast_ptr->right;
			}
			// Otherwise, Get Weight Of Current Node & Navigate Back Up The Tree
			else {
				weight = get_node_weight(ast_ptr);
				downward = false;
			}

			//// Check For "Repeat" Blocks
			//if (ast_ptr->type == NODE_REPEAT) {
			//	total_weight += weight;
			//	block_level++;
			//	block_weight[block_level] = 0;
			//}

			//// Switch To Right Node
			//if (new_ptr->right) {
			//	new_ptr = new_ptr->right;
			//	if (++depth > *ast_depth) *ast_depth = depth;
			//}
			//else {
			//	// Get Weight Of Right Node & Navigate Back Up The Tree
			//	if (old_ptr != root) {
			//		weight = get_node_weight(new_ptr);
			//		new_ptr = old_ptr->parent;
			//		depth--;
			//	}
			//	downward = false;
			//}
		}

		// Navigate Back Up The Tree
		else {
			if (ast_ptr == root)
				break;

			// Check If We Need To Navigate Back Down A Right Branch
			if ((ast_ptr == ast_ptr->parent->left) && (ast_ptr->parent->right)) {
				weight = get_node_weight(ast_ptr->parent);
				ast_ptr = ast_ptr->parent->right;
				downward = true;
			}
			else {
				ast_ptr = ast_ptr->parent;
				depth--;
			}
		}

		if (block_level >= 0)
			block_weight[block_level] += weight;
		else
			total_weight += (total_weight < (0xFFFFFFFF - weight) ? weight : 0);

		// Get Total weight For The "Repeat" Block
		if ((block_level >= 0) && (ast_ptr->type == NODE_REPEAT)) {
			if (block_level == 0)
				total_weight += ((uint32_t)ast_ptr->ivalue * block_weight[block_level]);
			else
				block_weight[block_level - 1] += ((uint32_t)ast_ptr->ivalue * block_weight[block_level]);
			block_level--;
		}
	}

	// Get weight Of Root Node
	weight = get_node_weight(ast_ptr);
	total_weight += (total_weight < (0xFFFFFFFF - weight) ? weight : 0);

	return total_weight;
}

static uint32_t get_node_weight(ast* node) {
	uint32_t weight = 1;

	bool l_is_float = false;
	bool r_is_float = false;

	if (!node)
		return 0;

	// Check If Leafs Are Float Or Int To Determine Weight
	if (node->left != NULL)
		l_is_float = (node->left->is_float);
	if (node->right != NULL)
		r_is_float = (node->right->is_float);

	node->is_float = node->is_float | l_is_float | r_is_float;

	// Increase Weight For Double Operations
	if (node->is_float)
		weight = 2;

	switch (node->type) {
		case NODE_IF:
		case NODE_ELSE:
		case NODE_REPEAT:
		case NODE_COND_ELSE:
			return weight * 4;

		case NODE_BREAK:
		case NODE_CONTINUE:
			return weight;

		// Variable / Constants (Weight x 1)
		case NODE_CONSTANT:
		case NODE_VAR_CONST:
		case NODE_VAR_EXP:
			return weight;

		// Assignments (Weight x 1)
		case NODE_ASSIGN:
		case NODE_ADD_ASSIGN:
		case NODE_SUB_ASSIGN:
		case NODE_MUL_ASSIGN:
		case NODE_DIV_ASSIGN:
		case NODE_MOD_ASSIGN:
		case NODE_LSHFT_ASSIGN:
		case NODE_RSHFT_ASSIGN:
		case NODE_AND_ASSIGN:
		case NODE_XOR_ASSIGN:
		case NODE_OR_ASSIGN:
			return weight;

		// Simple Operations (Weight x 1)
		case NODE_AND:
		case NODE_OR:
		case NODE_BITWISE_AND:
		case NODE_BITWISE_XOR:
		case NODE_BITWISE_OR:
		case NODE_EQ:
		case NODE_NE:
		case NODE_GT:
		case NODE_LT:
		case NODE_GE:
		case NODE_LE:
			return weight;

		case NODE_NOT:
		case NODE_COMPL:
		case NODE_NEG:
		case NODE_INCREMENT_R:
		case NODE_INCREMENT_L:
		case NODE_DECREMENT_R:
		case NODE_DECREMENT_L:
			return weight;

		// Medium Operations (Weight x 2)
		case NODE_ADD:
		case NODE_SUB:
		case NODE_LSHIFT:
		case NODE_RSHIFT:
		case NODE_VERIFY:
		case NODE_CONDITIONAL:
			return weight * 2;

		// Complex Operations (Weight x 3)
		case NODE_MUL:
		case NODE_DIV:
		case NODE_MOD:
		case NODE_LROT:
		case NODE_RROT:
			return weight * 3;

		// Complex Operations (Weight x 2)
		case NODE_ABS:
		case NODE_CEIL:
		case NODE_FLOOR:
		case NODE_FABS:
			return weight * 2;

		// Medium Functions (Weight x 4)
		case NODE_SIN:
		case NODE_COS:
		case NODE_TAN:
		case NODE_SINH:
		case NODE_COSH:
		case NODE_TANH:
		case NODE_ASIN:
		case NODE_ACOS:
		case NODE_ATAN:
		case NODE_FMOD:
			return weight * 4;

		// Complex Functions (Weight x 6)
		case NODE_EXPNT:
		case NODE_LOG:
		case NODE_LOG10:
		case NODE_SQRT:
		case NODE_ATAN2:
		case NODE_POW:
		case NODE_GCD:
			return weight * 6;

		// Function Calls (4 + Weight Of Called Function)
		case NODE_CALL_FUNCTION:
			return 4 + (uint32_t)stack_exp[node->uvalue]->fvalue;

		case NODE_BLOCK:
		case NODE_PARAM:
			break;

		default:
			break;
	}

	return 0;
}

extern int interpret_ast(bool first_run) {
	int i, idx = 0;

	//vm_bounty = false;
	//vm_break = false;
	//vm_continue = false;

	//for (i = idx; i < vm_ast_cnt; i++) {
	//	if (!interpret(vm_ast[i]) && (vm_ast[i]->type != NODE_VAR_CONST) && (vm_ast[i]->type != NODE_VAR_EXP) && (vm_ast[i]->type != NODE_CONSTANT))
	//			return 0;
	//}

	return vm_bounty;
}

static const unsigned int mask32 = (CHAR_BIT * sizeof(uint32_t) - 1);

#ifdef _MSC_VER
static uint32_t rotl32(uint32_t x, int n) {
#else
static uint32_t rotl32(uint32_t x, unsigned int n) {
#endif
n &= mask32;  // avoid undef behaviour with NDEBUG.  0 overhead for most types / compilers
	return (x << n) | (x >> ((-n)&mask32));
}

#ifdef _MSC_VER
static uint32_t rotr32(uint32_t x, int n) {
#else
static uint32_t rotr32(uint32_t x, unsigned int n) {
#endif
	n &= mask32;  // avoid undef behaviour with NDEBUG.  0 overhead for most types / compilers
	return (x >> n) | (x << ((-n)&mask32));
}

static void mangle_state(int x) {
	int mod = x % 32;
	int leaf = mod % 4;
	if (leaf == 0) {
		vm_state[0] = rotl32(vm_state[0], mod);
		vm_state[0] = vm_state[0] ^ x;
	}
	else if (leaf == 1) {
		vm_state[1] = rotl32(vm_state[1], mod);
		vm_state[1] = vm_state[1] ^ x;
	}
	else if (leaf == 2) {
		vm_state[2] = rotl32(vm_state[2], mod);
		vm_state[2] = vm_state[2] ^ x;
	}
	else {
		vm_state[3] = rotl32(vm_state[3], mod);
		vm_state[3] = vm_state[3] ^ x;
	}
}

static double interpret(ast* node) {
	//double lfval, rfval;
	//int32_t lval, rval;

	if (node == NULL)
		return 0;

	if (vm_break || vm_continue)
		return 1;
/*
	switch (node->type) {
	case NODE_CONSTANT:
		if (node->data_type == DT_FLOAT)
			return node->fvalue;
		else
			return node->value;
	case NODE_VAR_CONST:
		if (node->value < 0 || node->value > VM_MEMORY_SIZE)
			return 0;
		if (node->data_type == DT_FLOAT)
			return vm_f[node->value];
		return vm_m[node->value];
	case NODE_VAR_EXP:
		lval = (int32_t)interpret(node->left);
		if (lval < 0 || lval > VM_MEMORY_SIZE)
			return 0;
		if (node->data_type == DT_FLOAT)
			return vm_f[lval];
		return vm_m[lval];
	case NODE_ASSIGN:
		if (node->left->type == NODE_VAR_CONST)
			lval = node->left->value;
		else
			lval = (int32_t)interpret(node->left->left);
		if (lval < 0 || lval > VM_MEMORY_SIZE)
			return 0;

		rfval = interpret(node->right);

		if (node->left->is_float) {
			vm_f[lval] = rfval;
			mangle_state((int32_t)vm_f[lval]);
		}
		else {
			vm_m[lval] = (int32_t)rfval;
			mangle_state(vm_m[lval]);
		}
		return 1;
	case NODE_IF:
		if (node->right->type != NODE_ELSE) {
			if (interpret(node->left))
				interpret(node->right);					// If Body (No Else Condition)
		}
		else {
			if (interpret(node->left))
				interpret(node->right->left);			// If Body
			else
				interpret(node->right->right);			// Else Body
		}
		return 1;
	case NODE_CONDITIONAL:
		if (interpret(node->left))
			return interpret(node->right->left);
		else
			return interpret(node->right->right);
	case NODE_REPEAT:
		vm_break = false;
		lval = (int32_t)interpret(node->left);
		if (lval > node->value)
			lval = node->value;
		if (lval > 0) {
			int i;
			for (i = 0; i < lval; i++) {
				vm_continue = false;
				if (vm_break)
					break;
				else if (vm_continue)
					continue;
				else
					interpret(node->right);					// Repeat Body
			}
		}
		vm_break = false;
		vm_continue = false;
		return 1;
	case NODE_BLOCK:
		interpret(node->left);
		if (node->right)
			interpret(node->right);
		return 1;
	case NODE_BREAK:
		vm_break = true;
		break;
	case NODE_CONTINUE:
		vm_continue = true;
		break;
	case NODE_ADD_ASSIGN:
		if (node->left->type == NODE_VAR_CONST)
			lval = node->left->value;
		else
			lval = (int32_t)interpret(node->left->left);

		if (lval < 0 || lval > VM_MEMORY_SIZE)
			return 0;

		rfval = interpret(node->right);

		if (node->left->is_float) {
			vm_f[lval] += rfval;
			mangle_state((int32_t)vm_f[lval]);
		}
		else {
			vm_m[lval] += (int32_t)rfval;
			mangle_state(vm_m[lval]);
		}
		return 1;
	case NODE_SUB_ASSIGN:
		if (node->left->type == NODE_VAR_CONST)
			lval = node->left->value;
		else
			lval = (int32_t)interpret(node->left->left);

		if (lval < 0 || lval > VM_MEMORY_SIZE)
			return 0;

		rfval = interpret(node->right);

		if (node->left->is_float) {
			vm_f[lval] -= rfval;
			mangle_state((int32_t)vm_f[lval]);
		}
		else {
			vm_m[lval] -= (int32_t)rfval;
			mangle_state(vm_m[lval]);
		}
		return 1;
	case NODE_MUL_ASSIGN:
		if (node->left->type == NODE_VAR_CONST)
			lval = node->left->value;
		else
			lval = (int32_t)interpret(node->left->left);

		if (lval < 0 || lval > VM_MEMORY_SIZE)
			return 0;

		rfval = interpret(node->right);

		if (node->left->is_float) {
			vm_f[lval] *= rfval;
			mangle_state((int32_t)vm_f[lval]);
		}
		else {
			vm_m[lval] *= (int32_t)rfval;
			mangle_state(vm_m[lval]);
		}
		return 1;
	case NODE_DIV_ASSIGN:
		if (node->left->type == NODE_VAR_CONST)
			lval = node->left->value;
		else
			lval = (int32_t)interpret(node->left->left);

		if (lval < 0 || lval > VM_MEMORY_SIZE)
			return 0;

		rfval = interpret(node->right);
		if (rfval == 0.0)
			return 0;

		if (node->left->is_float) {
			vm_f[lval] /= rfval;
			mangle_state((int32_t)vm_f[lval]);
		}
		else {
			vm_m[lval] = (int32_t)(vm_m[lval] / rfval);
			mangle_state(vm_m[lval]);
		}
		return 1;
	case NODE_MOD_ASSIGN:
		if (node->left->type == NODE_VAR_CONST)
			lval = node->left->value;
		else
			lval = (int32_t)interpret(node->left->left);

		if (lval < 0 || lval > VM_MEMORY_SIZE)
			return 0;

		rval = (int32_t)interpret(node->right);
		if (rval == 0)
			return 0;

		if (node->left->is_float) {
			vm_f[lval] = (double)((int32_t)vm_f[lval] % rval);
			mangle_state((int32_t)vm_f[lval]);
		}
		else {
			vm_m[lval] %= rval;
			mangle_state(vm_m[lval]);
		}
		return 1;
	case NODE_LSHFT_ASSIGN:
		if (node->left->type == NODE_VAR_CONST)
			lval = node->left->value;
		else
			lval = (int32_t)interpret(node->left->left);

		if (lval < 0 || lval > VM_MEMORY_SIZE)
			return 0;

		rval = (int32_t)interpret(node->right);

		if (node->left->is_float) {
			vm_f[lval] = (double)((int32_t)vm_f[lval] << rval);
			mangle_state((int32_t)vm_f[lval]);
		}
		else {
			vm_m[lval] <<= rval;
			mangle_state(vm_m[lval]);
		}
		return 1;
	case NODE_RSHFT_ASSIGN:
		if (node->left->type == NODE_VAR_CONST)
			lval = node->left->value;
		else
			lval = (int32_t)interpret(node->left->left);

		if (lval < 0 || lval > VM_MEMORY_SIZE)
			return 0;

		rval = (int32_t)interpret(node->right);

		if (node->left->is_float) {
			vm_f[lval] = (double)((int32_t)vm_f[lval] >> rval);
			mangle_state((int32_t)vm_f[lval]);
		}
		else {
			vm_m[lval] >>= rval;
			mangle_state(vm_m[lval]);
		}
		return 1;
	case NODE_AND_ASSIGN:
		if (node->left->type == NODE_VAR_CONST)
			lval = node->left->value;
		else
			lval = (int32_t)interpret(node->left->left);

		if (lval < 0 || lval > VM_MEMORY_SIZE)
			return 0;

		rval = (int32_t)interpret(node->right);

		if (node->left->is_float) {
			vm_f[lval] = (double)((int32_t)vm_f[lval] & rval);
			mangle_state((int32_t)vm_f[lval]);
		}
		else {
			vm_m[lval] &= rval;
			mangle_state(vm_m[lval]);
		}
		return 1;
	case NODE_XOR_ASSIGN:
		if (node->left->type == NODE_VAR_CONST)
			lval = node->left->value;
		else
			lval = (int32_t)interpret(node->left->left);

		if (lval < 0 || lval > VM_MEMORY_SIZE)
			return 0;

		rval = (int32_t)interpret(node->right);

		if (node->left->is_float) {
			vm_f[lval] = (double)((int32_t)vm_f[lval] ^ rval);
			mangle_state((int32_t)vm_f[lval]);
		}
		else {
			vm_m[lval] ^= rval;
			mangle_state(vm_m[lval]);
		}
		return 1;
	case NODE_OR_ASSIGN:
		if (node->left->type == NODE_VAR_CONST)
			lval = node->left->value;
		else
			lval = (int32_t)interpret(node->left->left);

		if (lval < 0 || lval > VM_MEMORY_SIZE)
			return 0;

		rval = (int32_t)interpret(node->right);

		if (node->left->is_float) {
			vm_f[lval] = (double)((int32_t)vm_f[lval] | rval);
			mangle_state((int32_t)vm_f[lval]);
		}
		else {
			vm_m[lval] |= rval;
			mangle_state(vm_m[lval]);
		}
		return 1;
	case NODE_INCREMENT_R:
		if (node->left->type == NODE_VAR_CONST)
			lval = node->left->value;
		else
			lval = (int32_t)interpret(node->left->left);

		if (lval < 0 || lval > VM_MEMORY_SIZE)
			return 0;

		if (node->end_stmnt) {
			mangle_state((node->left->is_float) ? ++vm_f[lval] : ++vm_m[lval]);
			return 1;
		}
		else {
			return (node->left->is_float) ? ++vm_f[lval] : ++vm_m[lval];
		}
	case NODE_INCREMENT_L:
		if (node->left->type == NODE_VAR_CONST)
			lval = node->left->value;
		else
			lval = (int32_t)interpret(node->left->left);

		if (lval < 0 || lval > VM_MEMORY_SIZE)
			return 0;

		if (node->end_stmnt) {
			mangle_state((node->left->is_float) ? ++vm_f[lval] : ++vm_m[lval]);
			return 1;
		}
		else {
			if (node->left->is_float) {
				rfval = vm_f[lval]++;
				return rfval;
			}
			else {
				rval = vm_m[lval]++;
				return rval;
			}
		}
	case NODE_DECREMENT_R:
		if (node->left->type == NODE_VAR_CONST)
			lval = node->left->value;
		else
			lval = (int32_t)interpret(node->left->left);

		if (lval < 0 || lval > VM_MEMORY_SIZE)
			return 0;

		if (node->end_stmnt) {
			mangle_state((node->left->is_float) ? --vm_f[lval] : --vm_m[lval]);
			return 1;
		}
		else {
			return (node->left->is_float) ? --vm_f[lval] : --vm_m[lval];
		}
	case NODE_DECREMENT_L:
		if (node->left->type == NODE_VAR_CONST)
			lval = node->left->value;
		else
			lval = (int32_t)interpret(node->left->left);

		if (lval < 0 || lval > VM_MEMORY_SIZE)
			return 0;

		if (node->end_stmnt) {
			mangle_state((node->left->is_float) ? --vm_f[lval] : --vm_m[lval]);
			return 1;
		}
		else {
			if (node->left->is_float) {
				rfval = vm_f[lval]--;
				return rfval;
			}
			else {
				rval = vm_m[lval]--;
				return rval;
			}
		}
	case NODE_ADD:
		lfval = interpret(node->left);
		rfval = interpret(node->right);
		return (lfval + rfval);
	case NODE_SUB:
		lfval = interpret(node->left);
		rfval = interpret(node->right);
		return (lfval - rfval);
	case NODE_MUL:
		lfval = interpret(node->left);
		rfval = interpret(node->right);
		return (lfval * rfval);
	case NODE_DIV:
		lfval = interpret(node->left);
		rfval = interpret(node->right);
		if (rfval != 0.0)
			return (lfval / rfval);
		else
			return 0;
	case NODE_MOD:
		lval = (int32_t)interpret(node->left);
		rval = (int32_t)interpret(node->right);
		if (rval > 0)
			return (lval % rval);
		else
			return 0;
	case NODE_LSHIFT:
		lval = (int32_t)interpret(node->left);
		rval = (int32_t)interpret(node->right);
		return (lval << rval);
	case NODE_LROT:
		lval = (int32_t)interpret(node->left);
		rval = (int32_t)interpret(node->right);
		return rotl32(lval, rval % 32);
	case NODE_RSHIFT:
		lval = (int32_t)interpret(node->left);
		rval = (int32_t)interpret(node->right);
		return (lval >> rval);
	case NODE_RROT:
		lval = (int32_t)interpret(node->left);
		rval = (int32_t)interpret(node->right);
		return rotr32(lval, rval % 32);
	case NODE_NOT:
		lfval = interpret(node->left);
		return !lfval;
	case NODE_COMPL:
		lval = (int32_t)interpret(node->left);
		return ~lval;
	case NODE_AND:
		lfval = interpret(node->left);
		rfval = interpret(node->right);
		return (lfval && rfval);
	case NODE_OR:
		lfval = interpret(node->left);
		rfval = interpret(node->right);
		return (lfval || rfval);
	case NODE_BITWISE_AND:
		lval = (int32_t)interpret(node->left);
		rval = (int32_t)interpret(node->right);
		return (lval & rval);
	case NODE_BITWISE_XOR:
		lval = (int32_t)interpret(node->left);
		rval = (int32_t)interpret(node->right);
		return (lval ^ rval);
	case NODE_BITWISE_OR:
		lval = (int32_t)interpret(node->left);
		rval = (int32_t)interpret(node->right);
		return (lval | rval);
	case NODE_EQ:
		lfval = interpret(node->left);
		rfval = interpret(node->right);
		return (lfval == rfval);
	case NODE_NE:
		lfval = interpret(node->left);
		rfval = interpret(node->right);
		return (lfval != rfval);
	case NODE_GT:
		lfval = interpret(node->left);
		rfval = interpret(node->right);
		return (lfval > rfval);
	case NODE_LT:
		lfval = interpret(node->left);
		rfval = interpret(node->right);
		return (lfval < rfval);
	case NODE_GE:
		lfval = interpret(node->left);
		rfval = interpret(node->right);
		return (lfval >= rfval);
	case NODE_LE:
		lfval = interpret(node->left);
		rfval = interpret(node->right);
		return (lfval <= rfval);
	case NODE_NEG:
		lfval = interpret(node->left);
		return -lfval;
	case NODE_PARAM:
		vm_param_val[vm_param_num] = interpret(node->left);
		vm_param_idx[vm_param_num++] = node->left->value;
		rval = (int32_t)interpret(node->right);
		vm_param_num = 0;
		return 1;
	case NODE_SIN:
		vm_param_num = 0;
		interpret(node->right);
		return sin(vm_param_val[0]);
	case NODE_COS:
		vm_param_num = 0;
		interpret(node->right);
		return cos(vm_param_val[0]);
	case NODE_TAN:
		vm_param_num = 0;
		interpret(node->right);
		return tan(vm_param_val[0]);
	case NODE_SINH:
		vm_param_num = 0;
		interpret(node->right);
		return sinh(vm_param_val[0]);
	case NODE_COSH:
		vm_param_num = 0;
		interpret(node->right);
		return cosh(vm_param_val[0]);
	case NODE_TANH:
		vm_param_num = 0;
		interpret(node->right);
		return tanh(vm_param_val[0]);
	case NODE_ASIN:
		vm_param_num = 0;
		interpret(node->right);
		return asin(vm_param_val[0]);
	case NODE_ACOS:
		vm_param_num = 0;
		interpret(node->right);
		return acos(vm_param_val[0]);
	case NODE_ATAN:
		vm_param_num = 0;
		interpret(node->right);
		return atan(vm_param_val[0]);
	case NODE_ATAN2:
		vm_param_num = 0;
		interpret(node->right);
		if (vm_param_val[1] == 0.0)
			return 0;
		return atan2(vm_param_val[0], vm_param_val[1]);
	case NODE_EXPNT:
		vm_param_num = 0;
		interpret(node->right);
		if ((vm_param_val[0] < -708.0) || (vm_param_val[0] > 709.0))
			return 0;
		return exp(vm_param_val[0]);
	case NODE_LOG:
		vm_param_num = 0;
		interpret(node->right);
		if (vm_param_val[0] <= 0.0)
			return 0;
		return log(vm_param_val[0]);
	case NODE_LOG10:
		vm_param_num = 0;
		interpret(node->right);
		if (vm_param_val[0] <= 0.0)
			return 0;
		return log10(vm_param_val[0]);
	case NODE_POW:
		vm_param_num = 0;
		interpret(node->right);
		return pow(vm_param_val[0], vm_param_val[1]);
	case NODE_SQRT:
		vm_param_num = 0;
		interpret(node->right);
		if (vm_param_val[0] <= 0.0)
			return 0;
		return sqrt(vm_param_val[0]);
	case NODE_CEIL:
		vm_param_num = 0;
		interpret(node->right);
		return ceil(vm_param_val[0]);
	case NODE_FLOOR:
		vm_param_num = 0;
		interpret(node->right);
		return floor(vm_param_val[0]);
	case NODE_ABS:
		vm_param_num = 0;
		interpret(node->right);
		return abs((int32_t)vm_param_val[0]);
	case NODE_FABS:
		vm_param_num = 0;
		interpret(node->right);
		return fabs(vm_param_val[0]);
	case NODE_FMOD:
		vm_param_num = 0;
		interpret(node->right);
		if (vm_param_val[1] == 0.0)
			return 0;
		return fmod(vm_param_val[0], vm_param_val[1]);
	case NODE_GCD:
		vm_param_num = 0;
		interpret(node->right);
		return gcd((int32_t)vm_param_val[0], (int32_t)vm_param_val[1]);
	case NODE_VERIFY:
		lval = (int32_t)interpret(node->left);
		vm_bounty = (lval != 0);
		return vm_bounty;
	default:
		applog(LOG_ERR, "ERROR: VM Runtime - Unsupported Operation (%d)", node->type);
		return 0;
	}
*/
	return 1;
}
