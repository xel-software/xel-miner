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
#include "../miner.h"

uint64_t overflow_safe_mul(uint64_t lhs, uint64_t rhs)
{
        const uint64_t HALFSIZE_MAX = (1ul << LONG_BIT/2) - 1ul;
        uint64_t lhs_high = lhs >> LONG_BIT/2;
        uint64_t lhs_low  = lhs & HALFSIZE_MAX;
        uint64_t rhs_high = rhs >> LONG_BIT/2;
        uint64_t rhs_low  = rhs & HALFSIZE_MAX;
		uint64_t result = 0;
        uint64_t bot_bits = lhs_low * rhs_low;
        if (!(lhs_high || rhs_high)) {
            return bot_bits;
        }
        bool overflowed = lhs_high && rhs_high;
        unsigned long mid_bits1 = lhs_low * rhs_high;
        unsigned long mid_bits2 = lhs_high * rhs_low;

        result = bot_bits + ((mid_bits1+mid_bits2) << LONG_BIT/2);
        if(overflowed || result < bot_bits
            || (mid_bits1 >> LONG_BIT/2) != 0
            || (mid_bits2 >> LONG_BIT/2) != 0){
				applog(LOG_ERR, "ERROR: WCET overflowed, your program is way to complex. Exiting.");
				exit(EXIT_FAILURE);
		}

		// no overflow
		return result;
}


uint64_t overflow_safe_add (uint64_t a, uint64_t b)
{
    if (b > UINT64_MAX - a) {
        applog(LOG_ERR, "ERROR: WCET overflowed, your program is way to complex. Exiting.");
		exit(EXIT_FAILURE);
    }

    // no underflow
    return a+b;
}

extern uint64_t get_verify_wcet() {
	return stack_exp[ast_verify_idx]->wcet_value;
}
extern uint64_t get_main_wcet() {
	return stack_exp[ast_main_idx]->wcet_value;
}

extern uint64_t calc_wcet() {
	int i, call_depth = 0;
	uint32_t ast_depth;
	uint64_t wcet;

	// Get Max Function Call Depth
	for (i = ast_func_idx; i <= stack_exp_idx; i++) {
		if (stack_exp[i]->uvalue > call_depth)
			call_depth = (int)stack_exp[i]->uvalue;
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

				// Store WCET Value In Function's 'wcet_value' Field
				stack_exp[i]->wcet_value = wcet;
			}
		}
		call_depth--;
	}

	applog(LOG_DEBUG, "DEBUG: Total WCET = %lu", stack_exp[ast_main_idx]->wcet_value);

	return (uint64_t)stack_exp[ast_main_idx]->wcet_value;
}

static uint64_t calc_function_weight(ast* root, uint32_t *ast_depth) {
	uint32_t depth = 0;
	uint64_t  weight = 0, total_weight = 0;
	uint64_t block_weight[REPEAT_STACK_SIZE];
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

				if ((ast_ptr->type == NODE_IF) || (ast_ptr->type == NODE_ELSE))
					weight = get_node_weight(ast_ptr);

				// Check For Built In Function
				if ((ast_ptr->exp == EXP_FUNCTION))
					weight = get_node_weight(ast_ptr);

				// Check For "Repeat" Blocks
				if (ast_ptr->type == NODE_REPEAT) {
					weight = get_node_weight(ast_ptr);
					weight = overflow_safe_add(weight, get_node_weight(ast_ptr->left));
					block_level++;
					block_weight[block_level] = 0;
					break;
				}

				if (++depth > *ast_depth) *ast_depth = depth;
			}

			// If There Is A Right Node, Switch To It
			if (ast_ptr->right) {
				ast_ptr = ast_ptr->right;
				if (++depth > *ast_depth) *ast_depth = depth;
			}
			// Otherwise, Get Weight Of Current Node & Navigate Back Up The Tree
			else {
				weight = get_node_weight(ast_ptr);
				downward = false;
			}
		}

		// Navigate Back Up The Tree
		else {
			if (ast_ptr == root)
				break;

			// Check If We Need To Navigate Back Down A Right Branch
			if ((ast_ptr == ast_ptr->parent->left) && (ast_ptr->parent->right)) {

				ast_ptr = ast_ptr->parent->right;
				downward = true;

				if ((ast_ptr->type == NODE_IF) || (ast_ptr->type == NODE_ELSE))
					weight = get_node_weight(ast_ptr);

				// Check For Built In Function
				if ((ast_ptr->exp == EXP_FUNCTION))
					weight = get_node_weight(ast_ptr);

				// Check For "Repeat" Blocks
				if (ast_ptr->type == NODE_REPEAT) {
					weight = get_node_weight(ast_ptr);
					weight = overflow_safe_add(weight, get_node_weight(ast_ptr->left));
					block_level++;
					block_weight[block_level] = 0;
					ast_ptr = ast_ptr->right;
				}
				else {
					weight = get_node_weight(ast_ptr->parent);
					depth--;
				}
			}
			else {
				if (((ast_ptr->type == NODE_IF) && (ast_ptr->right->type != NODE_ELSE) ) || (ast_ptr->type == NODE_ELSE))
					weight = get_node_weight(ast_ptr);
				ast_ptr = ast_ptr->parent;
			}
		}

		if ((block_level >= 0) && (ast_ptr->parent->type != NODE_REPEAT))
			block_weight[block_level] = overflow_safe_add(block_weight[block_level], weight);
		else
			total_weight = overflow_safe_add(total_weight, (total_weight < (0xFFFFFFFF - weight) ? weight : 0)); // TODO, what is that???

		// Get Total weight For The "Repeat" Block
		if ((!downward) && (block_level >= 0) && (ast_ptr->type == NODE_REPEAT)) {
			if (block_level == 0)
				total_weight = overflow_safe_add(total_weight, overflow_safe_mul((uint64_t)ast_ptr->ivalue, block_weight[block_level]));
			else
				block_weight[block_level - 1] = overflow_safe_add(block_weight[block_level - 1], overflow_safe_mul((uint64_t)ast_ptr->ivalue, block_weight[block_level])); // TODO: what is ivalue?
			block_level--;
		}
	}

	return total_weight;
}

static uint64_t get_node_weight(ast* node) {
	uint64_t weight = 1;

	if (!node)
		return 0;

	// Increase Weight For 64bit Operations
	if (node->is_64bit)
		weight = 2;

	switch (node->type) {
		case NODE_IF:
		case NODE_ELSE:
		case NODE_COND_ELSE:
			return weight * 4;

		case NODE_REPEAT:
			return weight * 10;

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
			return 4 + (uint64_t)stack_exp[node->uvalue]->wcet_value;

		case NODE_BLOCK:
		case NODE_PARAM:
			break;

		default:
			break;
	}

	return 0;
}
