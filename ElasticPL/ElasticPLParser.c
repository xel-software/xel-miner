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
#include <float.h>

#include "ElasticPL.h"
#include "../miner.h"

int num_exp = 0;

int d_stack_op[10];	// DEBUG
ast *d_stack_exp[10];	// DEBUG


static ast* add_exp(NODE_TYPE node_type, EXP_TYPE exp_type, bool is_vm_mem, int64_t val_int64, uint64_t val_uint64, double val_double, unsigned char *svalue, int token_num, int line_num, DATA_TYPE data_type, ast* left, ast* right) {
	DATA_TYPE dt_l, dt_r;
	ast* e = calloc(1, sizeof(ast));

	if (e) {
		e->type = node_type;
		e->exp = exp_type;
		e->is_vm_mem = is_vm_mem;
		e->ivalue = val_int64;
		e->uvalue = val_uint64;
		e->fvalue = val_double;
		e->svalue = &svalue[0];
		e->token_num = token_num;
		e->line_num = line_num;
		e->end_stmnt = false;
		e->data_type = data_type;
		e->left = left;
		e->right = right;

		// ElasticPL Operator Nodes Inherit Data Type From Child Nodes
		// Precedence Is Based On C99 Standard:
		// double <- float <- uint64_t <- int64_t <- uint32_t <- int32_t
		if ((data_type != DT_NONE) && (node_type != NODE_VAR_CONST) && (node_type != NODE_VAR_EXP) && (node_type != NODE_CONSTANT)) {
			dt_l = left ? left->data_type : DT_NONE;
			dt_r = right ? right->data_type : DT_NONE;

			if ((dt_l == DT_DOUBLE) || (dt_r == DT_DOUBLE))
				e->data_type = DT_DOUBLE;
			else if ((dt_l == DT_FLOAT) || (dt_r == DT_FLOAT))
				e->data_type = DT_FLOAT;
			else if ((dt_l == DT_ULONG) || (dt_r == DT_ULONG))
				e->data_type = DT_ULONG;
			else if ((dt_l == DT_LONG) || (dt_r == DT_LONG))
				e->data_type = DT_LONG;
			else if ((dt_l == DT_UINT) || (dt_r == DT_UINT))
				e->data_type = DT_UINT;
			else
				e->data_type = DT_INT;
		}

		// Set Indicators Based On Data Type
		switch (e->data_type) {
		case DT_INT:
			e->is_64bit = false;
			e->is_signed = true;
			e->is_float = false;
			break;
		case DT_UINT:
			e->is_64bit = false;
			e->is_signed = false;
			e->is_float = false;
			break;
		case DT_LONG:
			e->is_64bit = true;
			e->is_signed = true;
			e->is_float = false;
			break;
		case DT_ULONG:
			e->is_64bit = true;
			e->is_signed = false;
			e->is_float = false;
			break;
		case DT_FLOAT:
			e->is_64bit = false;
			e->is_signed = true;
			e->is_float = true;
			break;
		case DT_DOUBLE:
			e->is_64bit = true;
			e->is_signed = true;
			e->is_float = true;
			break;
		default:
			e->is_64bit = false;
			e->is_signed = false;
			e->is_float = false;
		}

		if (left)
			e->left->parent = e;
		if (right)
			e->right->parent = e;
	}
	return e;
}

/*
static void push_op(int token_id) {
	stack_op[++stack_op_idx] = token_id;
	top_op = token_id;
}

static int pop_op() {
	int op = -1;
	if (stack_op_idx >= 0) {
		op = stack_op[stack_op_idx];
		stack_op[stack_op_idx--] = -1;
	}

	if (stack_op_idx >= 0)
		top_op = stack_op[stack_op_idx];
	else
		top_op = -1;

	return op;
}

static void push_exp(ast* exp) {
	stack_exp[++stack_exp_idx] = exp;
	if (!exp->end_stmnt)
		num_exp++;
}

static ast* pop_exp() {
	ast *exp = NULL;

	if (stack_exp_idx >= 0) {
		exp = stack_exp[stack_exp_idx];
		stack_exp[stack_exp_idx--] = NULL;
		if (!exp->end_stmnt)
			num_exp--;
	}

	return exp;
}
*/

static void push_op(int token_id) {
	stack_op[++stack_op_idx] = token_id;
	top_op = token_id;


	// DEBUG
	int i;
	for (i = 0; i < 10; i++)
		d_stack_op[i] = stack_op[i];


}

static int pop_op() {
	int op = -1;
	if (stack_op_idx >= 0) {
		op = stack_op[stack_op_idx];
		stack_op[stack_op_idx--] = -1;
	}

	if (stack_op_idx >= 0)
		top_op = stack_op[stack_op_idx];
	else
		top_op = -1;





	// DEBUG
	int i;
	for (i = 0; i < 10; i++)
		d_stack_op[i] = stack_op[i];




	return op;
}

static void push_exp(ast* exp) {
	stack_exp[++stack_exp_idx] = exp;
	if (!exp->end_stmnt)
		num_exp++;


	// DEBUG
	int i;
	for (i = 0; i < 10; i++)
		d_stack_exp[i] = stack_exp[i];


}

static ast* pop_exp() {
	ast *exp = NULL;

	if (stack_exp_idx >= 0) {
		exp = stack_exp[stack_exp_idx];
		stack_exp[stack_exp_idx--] = NULL;
		if (!exp->end_stmnt)
			num_exp--;
	}



	// DEBUG
	int i;
	for (i = 0; i < 10; i++)
		d_stack_exp[i] = stack_exp[i];


	return exp;
}

static bool validate_inputs(SOURCE_TOKEN *token, int token_num, NODE_TYPE node_type) {

	if ((token->inputs == 0) || (node_type == NODE_BLOCK))
		return true;

	// Validate That There Are Enough Expressions / Statements On The Stack
	if (node_type == NODE_FUNCTION) {
		if (stack_exp_idx < 0) {
				applog(LOG_ERR, "Syntax Error: Line: %d - Invalid number of inputs ", token->line_num);
			return false;
		}
	}
	else if ((node_type == NODE_IF) || (node_type == NODE_ELSE) || (node_type == NODE_REPEAT)) {
		if (stack_exp_idx < 1) {
			applog(LOG_ERR, "Syntax Error: Line: %d - Invalid number of inputs ", token->line_num);
			return false;
		}
	}
	else if (num_exp < token->inputs) {
		applog(LOG_ERR, "Syntax Error: Line: %d - Invalid number of inputs ", token->line_num);
		return false;
	}

	// Validate The Inputs For Each Node Type Are The Correct Type
	switch (node_type) {

	// VM Memory Declarations
	case NODE_ARRAY_INT:
	case NODE_ARRAY_UINT:
	case NODE_ARRAY_LONG:
	case NODE_ARRAY_ULONG:
	case NODE_ARRAY_FLOAT:
	case NODE_ARRAY_DOUBLE:
		if ((stack_exp[stack_exp_idx]->token_num > token_num) && !stack_exp[stack_exp_idx]->is_signed && !stack_exp[stack_exp_idx]->is_float) {

			if (stack_exp[stack_exp_idx]->uvalue == 0) {
				applog(LOG_ERR, "Syntax Error: Line: %d - Array size must be greater than zero", token->line_num);
				return false;
			}

			// Check That There Is Only One Instance Of Each Data Type Array
			switch (node_type) {
			case NODE_ARRAY_INT:
				if (ast_vm_ints != 0) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Int array already declared", token->line_num);
					return false;
				}
				ast_vm_ints = (uint32_t)stack_exp[stack_exp_idx]->uvalue;
				break;
			case NODE_ARRAY_UINT:
				if (ast_vm_uints != 0) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Unsigned Int array already declared", token->line_num);
					return false;
				}
				ast_vm_uints = (uint32_t)stack_exp[stack_exp_idx]->uvalue;
				break;
			case NODE_ARRAY_LONG:
				if (ast_vm_longs != 0) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Long array already declared", token->line_num);
					return false;
				}
				ast_vm_longs = (uint32_t)stack_exp[stack_exp_idx]->uvalue;
				break;
			case NODE_ARRAY_ULONG:
				if (ast_vm_ulongs != 0) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Unsigned Long array already declared", token->line_num);
					return false;
				}
				ast_vm_ulongs = (uint32_t)stack_exp[stack_exp_idx]->uvalue;
				break;
			case NODE_ARRAY_FLOAT:
				if (ast_vm_floats != 0) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Float array already declared", token->line_num);
					return false;
				}
				ast_vm_floats = (uint32_t)stack_exp[stack_exp_idx]->uvalue;
				break;
			case NODE_ARRAY_DOUBLE:
				if (ast_vm_doubles != 0) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Double array already declared", token->line_num);
					return false;
				}
				ast_vm_doubles = (uint32_t)stack_exp[stack_exp_idx]->uvalue;
				break;
			}

			// Check If Total Allocated VM Memory Is Less Than Max Allowed
			if ((((ast_vm_ints + ast_vm_uints + ast_vm_floats) * 4) + ((ast_vm_longs + ast_vm_ulongs + ast_vm_doubles) * 8)) > ast_vm_MEMORY_SIZE) {
				applog(LOG_ERR, "Syntax Error - Requested VM Memory (%d bytes) exceeds allowable (%d bytes)", (((ast_vm_ints + ast_vm_uints + ast_vm_floats) * 4) + ((ast_vm_longs + ast_vm_ulongs + ast_vm_doubles) * 8)), ast_vm_MEMORY_SIZE);
				return false;
			}
			return true;
		}
		break;

	// VM Storage Declarations
	case NODE_STORAGE_CNT:
	case NODE_STORAGE_IDX:
		if ((stack_exp_idx > 0) &&
			(stack_exp[stack_exp_idx]->token_num > token_num) &&
			(stack_exp[stack_exp_idx]->type == NODE_CONSTANT) &&
			(stack_exp[stack_exp_idx]->data_type == DT_UINT)) {

			// Check That Global Unsigned Int Array Has Been Declared
			if (!ast_vm_uints) {
				applog(LOG_ERR, "Syntax Error: Line: %d - Unsigned Int array must be declared before 'storage' statements", token->line_num);
				return false;
			}

			// Save Storage Value
			if (node_type == NODE_STORAGE_CNT)
				ast_storage_cnt = (uint32_t)stack_exp[stack_exp_idx]->uvalue;
			else if (node_type == NODE_STORAGE_IDX)
				ast_storage_idx = (uint32_t)stack_exp[stack_exp_idx]->uvalue;

			// Check That Storage Indexes Are Within Unsigned Int Array
			if (ast_vm_uints < (ast_storage_cnt + ast_storage_idx)) {
				applog(LOG_ERR, "Syntax Error: Line: %d - 'storage_cnt' + 'storage_idx' must be within Unsigned Int array range", token->line_num);
				return false;
			}

			return true;
		}
		break;

	// CONSTANT Declaration (1 Number)
	case NODE_CONSTANT:
		if ((stack_exp[stack_exp_idx]->token_num < token_num) && (stack_exp[stack_exp_idx]->data_type != DT_NONE))
			return true;
		break;

	// Variable Declaration (1 Unsigned Int/Long)
	case NODE_VAR_CONST:
	case NODE_VAR_EXP:
		if ((stack_exp[stack_exp_idx]->token_num < token_num) &&
			((stack_exp[stack_exp_idx]->data_type == DT_UINT) || (stack_exp[stack_exp_idx]->data_type == DT_ULONG))) {

			switch (token->data_type) {
			case DT_INT:
				if (ast_vm_ints == 0) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Int array not declared", token->line_num);
					return false;
				}
				else if (stack_exp[stack_exp_idx]->uvalue >= ast_vm_ints) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Array index out of bounds", token->line_num);
					return false;
				}
				break;
			case DT_UINT:
				if (ast_vm_uints == 0) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Unsigned Int array not declared", token->line_num);
					return false;
				}
				else if (stack_exp[stack_exp_idx]->uvalue >= ast_vm_uints) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Array index out of bounds", token->line_num);
					return false;
				}
				break;
			case DT_LONG:
				if (ast_vm_longs == 0) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Long array not declared", token->line_num);
					return false;
				}
				else if (stack_exp[stack_exp_idx]->uvalue >= ast_vm_longs) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Array index out of bounds", token->line_num);
					return false;
				}
				break;
			case DT_ULONG:
				if (ast_vm_ulongs == 0) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Unsigned Long array not declared", token->line_num);
					return false;
				}
				else if (stack_exp[stack_exp_idx]->uvalue >= ast_vm_ulongs) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Array index out of bounds", token->line_num);
					return false;
				}
				break;
			case DT_FLOAT:
				if (ast_vm_floats == 0) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Float array not declared", token->line_num);
					return false;
				}
				else if (stack_exp[stack_exp_idx]->uvalue >= ast_vm_floats) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Array index out of bounds", token->line_num);
					return false;
				}
				break;
			case DT_DOUBLE:
				if (ast_vm_doubles == 0) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Double array not declared", token->line_num);
					return false;
				}
				else if (stack_exp[stack_exp_idx]->uvalue >= ast_vm_doubles) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Array index out of bounds", token->line_num);
					return false;
				}
				break;
			case DT_UINT_M: // m[]
				if (stack_exp[stack_exp_idx]->uvalue >= VM_M_ARRAY_SIZE) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Array index out of bounds", token->line_num);
					return false;
				}
				token->data_type = DT_UINT;
				break;
			case DT_UINT_S: // s[]
				if (stack_exp[stack_exp_idx]->uvalue >= ast_storage_cnt) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Array index out of bounds", token->line_num);
					return false;
				}
				token->data_type = DT_UINT;
				break;
			}
			return true;
		}
		break;

	// Function Declarations (1 Constant & 1 Block)
	case NODE_FUNCTION:
		if ((stack_exp[stack_exp_idx - 1]->type == NODE_CONSTANT) && (stack_exp[stack_exp_idx]->type == NODE_BLOCK))
			return true;
		break;

	// Function Call Declarations (1 Constant)
	case NODE_CALL_FUNCTION:
		if ((stack_exp[stack_exp_idx]->type == NODE_CONSTANT) && stack_exp[stack_exp_idx]->svalue)
			return true;
		break;

	// IF Statement (1 Number & 1 Statement)
	case NODE_IF:
		if ((stack_exp[stack_exp_idx - 1]->data_type != DT_NONE) &&
			((stack_exp[stack_exp_idx]->end_stmnt == true) || (stack_exp[stack_exp_idx]->type == NODE_IF) || (stack_exp[stack_exp_idx]->type == NODE_ELSE) || (stack_exp[stack_exp_idx]->type == NODE_REPEAT) || (stack_exp[stack_exp_idx]->type == NODE_BREAK) || (stack_exp[stack_exp_idx]->type == NODE_CONTINUE))) {

			if (stack_exp[stack_exp_idx]->type == NODE_REPEAT) {
				applog(LOG_ERR, "Syntax Error: Line: %d - A 'repeat' statement under an 'if' statement must be enclosed in {} brackets", token->line_num);
				return false;
			}

			return true;
		}
		break;

	// ELSE Statement (2 Statements)
	case NODE_ELSE:
		if ((stack_exp[stack_exp_idx - 1]->end_stmnt == true) && (stack_exp[stack_exp_idx]->end_stmnt == true)) {

			if (stack_exp[stack_exp_idx]->type == NODE_REPEAT) {
				applog(LOG_ERR, "Syntax Error: Line: %d - A 'repeat' statement under an 'else' statement must be enclosed in {} brackets", token->line_num);
				return false;
			}

			return true;
		}
		break;

	// REPEAT Statement (2 Unsigned Int & 1 Constant Unsigned Int & 1 Block)
	case NODE_REPEAT:
		if ((stack_exp_idx > 2) &&
			(stack_exp[stack_exp_idx - 3]->type == NODE_VAR_CONST) &&
			(stack_exp[stack_exp_idx - 3]->data_type == DT_UINT) &&
			((stack_exp[stack_exp_idx - 2]->type == NODE_VAR_CONST) || (stack_exp[stack_exp_idx - 2]->type == NODE_VAR_EXP) || (stack_exp[stack_exp_idx - 2]->type == NODE_CONSTANT)) &&
			(stack_exp[stack_exp_idx - 2]->data_type == DT_UINT) &&
			(stack_exp[stack_exp_idx - 1]->type == NODE_CONSTANT) &&
			(stack_exp[stack_exp_idx - 1]->data_type == DT_UINT) &&
			(stack_exp[stack_exp_idx]->type == NODE_BLOCK))
			return true;
		break;

	// Expressions w/ 1 Number (Right Operand)
	case NODE_RESULT:
	case NODE_NOT:
		if ((stack_exp[stack_exp_idx]->token_num > token_num) && (stack_exp[stack_exp_idx]->data_type != DT_NONE))
			return true;
		break;

	// Expressions w/ 1 Int/Uint/Long/ULong (Right Operand)
	case NODE_COMPL:
	case NODE_ABS:
		if ((stack_exp[stack_exp_idx]->token_num > token_num) &&
			(stack_exp[stack_exp_idx]->data_type != DT_NONE) &&
			(!stack_exp[stack_exp_idx]->is_float))
			return true;
		break;

	// Expressions w/ 1 Int/Long/Float/Double (Right Operand)
	case NODE_NEG:
		if ((stack_exp[stack_exp_idx]->token_num > token_num) &&
			(stack_exp[stack_exp_idx]->data_type != DT_NONE) &&
			(stack_exp[stack_exp_idx]->is_signed))
			return true;
		break;

	// Expressions w/ 1 Variable (Left Operand)
	case NODE_INCREMENT_R:
	case NODE_DECREMENT_R:
		if ((stack_exp[stack_exp_idx]->token_num > token_num) &&
			((stack_exp[stack_exp_idx]->type == NODE_VAR_CONST) || (stack_exp[stack_exp_idx]->type == NODE_VAR_EXP)))
			return true;
		break;

	// Expressions w/ 1 Variable (Right Operand)
	case NODE_INCREMENT_L:
	case NODE_DECREMENT_L:
		if ((stack_exp[stack_exp_idx]->token_num < token_num) &&
			((stack_exp[stack_exp_idx]->type == NODE_VAR_CONST) || (stack_exp[stack_exp_idx]->type == NODE_VAR_EXP)))
				return true;
		break;

	// Expressions w/ 1 Variable (Left Operand) & 1 Number (Right Operand)
	case NODE_ASSIGN:
	case NODE_ADD_ASSIGN:
	case NODE_SUB_ASSIGN:
	case NODE_MUL_ASSIGN:
	case NODE_DIV_ASSIGN:
		if (((stack_exp[stack_exp_idx - 1]->token_num < token_num) && (stack_exp[stack_exp_idx]->token_num > token_num)) &&
			((stack_exp[stack_exp_idx - 1]->type == NODE_VAR_CONST) || (stack_exp[stack_exp_idx - 1]->type == NODE_VAR_EXP)) &&
			(stack_exp[stack_exp_idx]->data_type != DT_NONE))
			return true;
		break;

	// Expressions w/ 1 Int/Uint/Long/Ulong Variable (Left Operand) & 1 Int/Uint/Long/Ulong (Right Operand)
	case NODE_MOD_ASSIGN:
	case NODE_LSHFT_ASSIGN:
	case NODE_RSHFT_ASSIGN:
	case NODE_AND_ASSIGN:
	case NODE_XOR_ASSIGN:
	case NODE_OR_ASSIGN:
		if (((stack_exp[stack_exp_idx - 1]->token_num < token_num) && (stack_exp[stack_exp_idx]->token_num > token_num)) &&
			((stack_exp[stack_exp_idx - 1]->type == NODE_VAR_CONST) || (stack_exp[stack_exp_idx - 1]->type == NODE_VAR_EXP)) &&
			(!stack_exp[stack_exp_idx - 1]->is_float) &&
			(stack_exp[stack_exp_idx]->data_type != DT_NONE) &&
			(!stack_exp[stack_exp_idx]->is_float))
			return true;
		break;

	// Expressions w/ 2 Numbers
	case NODE_MUL:
	case NODE_DIV:
	case NODE_MOD:
	case NODE_ADD:
	case NODE_SUB:
	case NODE_LE:
	case NODE_GE:
	case NODE_LT:
	case NODE_GT:
	case NODE_EQ:
	case NODE_NE:
	case NODE_AND:
	case NODE_OR:
	case NODE_CONDITIONAL:
	case NODE_COND_ELSE:
		if (((stack_exp[stack_exp_idx - 1]->token_num < token_num) && (stack_exp[stack_exp_idx]->token_num > token_num)) &&
			((stack_exp[stack_exp_idx - 1]->data_type != DT_NONE)) &&
			(stack_exp[stack_exp_idx]->data_type != DT_NONE))
			return true;
		break;

	// Expressions w/ 2 Ints/Uints/Longs/Ulongs
	case NODE_LROT:
	case NODE_LSHIFT:
	case NODE_RROT:
	case NODE_RSHIFT:
	case NODE_BITWISE_AND:
	case NODE_BITWISE_XOR:
	case NODE_BITWISE_OR:
		if (((stack_exp[stack_exp_idx - 1]->token_num < token_num) && (stack_exp[stack_exp_idx]->token_num > token_num)) &&
			(stack_exp[stack_exp_idx - 1]->data_type != DT_NONE) &&
			(!stack_exp[stack_exp_idx - 1]->is_float) &&
			(stack_exp[stack_exp_idx]->data_type != DT_NONE) &&
			(!stack_exp[stack_exp_idx]->is_float))
			return true;
		break;

	// Built-in Functions w/ 1 Number
	case NODE_SIN:
	case NODE_COS:
	case NODE_TAN:
	case NODE_SINH:
	case NODE_COSH:
	case NODE_TANH:
	case NODE_ASIN:
	case NODE_ACOS:
	case NODE_ATAN:
	case NODE_EXPNT:
	case NODE_LOG:
	case NODE_LOG10:
	case NODE_SQRT:
	case NODE_CEIL:
	case NODE_FLOOR:
	case NODE_FABS:
		if ((stack_exp[stack_exp_idx]->token_num > token_num) &&
			(stack_exp[stack_exp_idx]->data_type != DT_NONE))
			return true;
		break;

	// Built-in Functions w/ 2 Numbers
	case NODE_ATAN2:
	case NODE_POW:
	case NODE_FMOD:
	case NODE_GCD:
		if (((stack_exp[stack_exp_idx - 1]->token_num > token_num) && (stack_exp[stack_exp_idx]->token_num > token_num)) &&
			((stack_exp[stack_exp_idx - 1]->data_type != DT_NONE)) &&
			(stack_exp[stack_exp_idx]->data_type != DT_NONE))
			return true;
		break;

	default:
		break;
	}

	applog(LOG_ERR, "Syntax Error: Line: %d - Invalid inputs for '%s'", token->line_num, get_node_str(node_type));
	return false;
}

static NODE_TYPE get_node_type(SOURCE_TOKEN *token, int token_num) {
	NODE_TYPE node_type;

	switch (token->type) {
	case TOKEN_VAR_END:
		if (stack_exp_idx >= 0 && stack_exp[stack_exp_idx]->type == NODE_CONSTANT)
			node_type = NODE_VAR_CONST;
		else
			node_type = NODE_VAR_EXP;
		break;
	case TOKEN_INCREMENT:
		if (stack_exp_idx >= 0 && (stack_exp[stack_exp_idx]->token_num > token_num))
			node_type = NODE_INCREMENT_R;
		else
			node_type = NODE_INCREMENT_L;
		break;
	case TOKEN_DECREMENT:
		if (stack_exp_idx >= 0 && (stack_exp[stack_exp_idx]->token_num > token_num))
			node_type = NODE_DECREMENT_R;
		else
			node_type = NODE_DECREMENT_L;
		break;
	case TOKEN_COMPL:			node_type = NODE_COMPL;			break;
	case TOKEN_NOT:				node_type = NODE_NOT;			break;
	case TOKEN_NEG:				node_type = NODE_NEG;			break;
	case TOKEN_LITERAL:			node_type = NODE_CONSTANT;		break;
	case TOKEN_TRUE:			node_type = NODE_CONSTANT;		break;
	case TOKEN_FALSE:			node_type = NODE_CONSTANT;		break;
	case TOKEN_MUL:				node_type = NODE_MUL;			break;
	case TOKEN_DIV:				node_type = NODE_DIV;			break;
	case TOKEN_MOD:				node_type = NODE_MOD;			break;
	case TOKEN_ADD_ASSIGN:		node_type = NODE_ADD_ASSIGN;	break;
	case TOKEN_SUB_ASSIGN:		node_type = NODE_SUB_ASSIGN;	break;
	case TOKEN_MUL_ASSIGN:		node_type = NODE_MUL_ASSIGN;	break;
	case TOKEN_DIV_ASSIGN:		node_type = NODE_DIV_ASSIGN;	break;
	case TOKEN_MOD_ASSIGN:		node_type = NODE_MOD_ASSIGN;	break;
	case TOKEN_LSHFT_ASSIGN:	node_type = NODE_LSHFT_ASSIGN;	break;
	case TOKEN_RSHFT_ASSIGN:	node_type = NODE_RSHFT_ASSIGN;	break;
	case TOKEN_AND_ASSIGN:		node_type = NODE_AND_ASSIGN;	break;
	case TOKEN_XOR_ASSIGN:		node_type = NODE_XOR_ASSIGN;	break;
	case TOKEN_OR_ASSIGN:		node_type = NODE_OR_ASSIGN;		break;
	case TOKEN_ADD:				node_type = NODE_ADD;			break;
	case TOKEN_SUB:				node_type = NODE_SUB;			break;
	case TOKEN_LROT:			node_type = NODE_LROT;			break;
	case TOKEN_LSHIFT:			node_type = NODE_LSHIFT;		break;
	case TOKEN_RROT:			node_type = NODE_RROT;			break;
	case TOKEN_RSHIFT:			node_type = NODE_RSHIFT;		break;
	case TOKEN_LE:				node_type = NODE_LE;			break;
	case TOKEN_GE:				node_type = NODE_GE;			break;
	case TOKEN_LT:				node_type = NODE_LT;			break;
	case TOKEN_GT:				node_type = NODE_GT;			break;
	case TOKEN_EQ:				node_type = NODE_EQ;			break;
	case TOKEN_NE:				node_type = NODE_NE;			break;
	case TOKEN_BITWISE_AND:		node_type = NODE_BITWISE_AND;	break;
	case TOKEN_BITWISE_XOR:		node_type = NODE_BITWISE_XOR;	break;
	case TOKEN_BITWISE_OR:		node_type = NODE_BITWISE_OR;	break;
	case TOKEN_AND:				node_type = NODE_AND;			break;
	case TOKEN_OR:				node_type = NODE_OR;			break;
	case TOKEN_BLOCK_END:		node_type = NODE_BLOCK;			break;
	case TOKEN_CONDITIONAL:		node_type = NODE_CONDITIONAL;	break;
	case TOKEN_COND_ELSE:		node_type = NODE_COND_ELSE;		break;
	case TOKEN_IF:				node_type = NODE_IF;			break;
	case TOKEN_ELSE:			node_type = NODE_ELSE;			break;
	case TOKEN_REPEAT:			node_type = NODE_REPEAT;		break;
	case TOKEN_BREAK:			node_type = NODE_BREAK;			break;
	case TOKEN_CONTINUE:		node_type = NODE_CONTINUE;		break;
	case TOKEN_ASSIGN:			node_type = NODE_ASSIGN;		break;
	case TOKEN_SIN:				node_type = NODE_SIN;			break;
	case TOKEN_COS:				node_type = NODE_COS; 			break;
	case TOKEN_TAN:				node_type = NODE_TAN; 			break;
	case TOKEN_SINH:			node_type = NODE_SINH;			break;
	case TOKEN_COSH:			node_type = NODE_COSH;			break;
	case TOKEN_TANH:			node_type = NODE_TANH;			break;
	case TOKEN_ASIN:			node_type = NODE_ASIN;			break;
	case TOKEN_ACOS:			node_type = NODE_ACOS;			break;
	case TOKEN_ATAN:			node_type = NODE_ATAN;			break;
	case TOKEN_ATAN2:			node_type = NODE_ATAN2;			break;
	case TOKEN_EXPNT:			node_type = NODE_EXPNT;			break;
	case TOKEN_LOG:				node_type = NODE_LOG;			break;
	case TOKEN_LOG10:			node_type = NODE_LOG10;			break;
	case TOKEN_POW:				node_type = NODE_POW;			break;
	case TOKEN_SQRT:			node_type = NODE_SQRT;			break;
	case TOKEN_CEIL:			node_type = NODE_CEIL;			break;
	case TOKEN_FLOOR:			node_type = NODE_FLOOR;			break;
	case TOKEN_ABS:				node_type = NODE_ABS;			break;
	case TOKEN_FABS:			node_type = NODE_FABS;			break;
	case TOKEN_FMOD:			node_type = NODE_FMOD; 			break;
	case TOKEN_GCD:				node_type = NODE_GCD; 			break;
	case TOKEN_ARRAY_INT:		node_type = NODE_ARRAY_INT; 	break;
	case TOKEN_ARRAY_UINT:		node_type = NODE_ARRAY_UINT; 	break;
	case TOKEN_ARRAY_LONG:		node_type = NODE_ARRAY_LONG; 	break;
	case TOKEN_ARRAY_ULONG:		node_type = NODE_ARRAY_ULONG; 	break;
	case TOKEN_ARRAY_FLOAT:		node_type = NODE_ARRAY_FLOAT; 	break;
	case TOKEN_ARRAY_DOUBLE:	node_type = NODE_ARRAY_DOUBLE; 	break;
	case TOKEN_STORAGE_CNT:		node_type = NODE_STORAGE_CNT; 	break;
	case TOKEN_STORAGE_IDX:		node_type = NODE_STORAGE_IDX; 	break;
	case TOKEN_FUNCTION:		node_type = NODE_FUNCTION;		break;
	case TOKEN_CALL_FUNCTION:	node_type = NODE_CALL_FUNCTION;	break;
	case TOKEN_RESULT:			node_type = NODE_RESULT;		break;
	default: return NODE_ERROR;
	}

	return node_type;
}

static bool create_exp(SOURCE_TOKEN *token, int token_num) {
	int i;
	uint32_t len;
	bool is_signed = false;
	bool is_vm_mem = false;
	int64_t val_int64 = 0;
	uint64_t val_uint64 = 0;
	double val_double = 0.0;
	unsigned char *svalue = NULL;
	NODE_TYPE node_type = NODE_ERROR;
	DATA_TYPE data_type;
	ast *exp, *left = NULL, *right = NULL;

	node_type = get_node_type(token, token_num);
	data_type = token->data_type;
	
	// Map Token To Node Type
	if (node_type == NODE_ERROR) {
		applog(LOG_ERR, "Unknown Token in ElasticPL Source.  Line: %d, Token Type: %d", token->line_num, token->type);
		return false;
	}

	// Confirm Required Number / Types Of Expressions Are On Stack
	if (!validate_inputs(token, token_num, node_type))
		return false;

	switch (token->exp) {

	case EXP_EXPRESSION:

		// Constant Expressions
		if (token->inputs == 0) {

			if (token->type == TOKEN_TRUE) {
				val_uint64 = 1;
				data_type = DT_UINT;
			}
			else if (token->type == TOKEN_FALSE) {
				val_uint64 = 0;
				data_type = DT_UINT;
			}
			else if (node_type == NODE_CONSTANT) {

				data_type = DT_NONE;

				if (token->literal[0] == '-')
					is_signed = true;
				else
					is_signed = false;

				len = strlen(token->literal);

				if (token->data_type == DT_INT) {

					// Convert Hex Numbers
					if ((len > 2) && (token->literal[0] == '0') && (token->literal[1] == 'x')) {
						if (len > 18) {
							applog(LOG_ERR, "Syntax Error: Line: %d - Hex value exceeds 64 bits", token->line_num);
							return false;
						}
						else if (len < 11) {
							data_type = DT_UINT;
						}
						else {
							data_type = DT_ULONG;
						}

						val_uint64 = strtoull(&token->literal[2], NULL, 16);
					}

					// Convert Binary Numbers
					else if ((len > 2) && (token->literal[0] == '0') && (token->literal[1] == 'b')) {
						if (len > 66) {
							applog(LOG_ERR, "Syntax Error: Line: %d - Binary value exceeds 64 bits", token->line_num);
							return false;
						}
						else if (len < 35) {
							data_type = DT_UINT;
						}
						else {
							data_type = DT_ULONG;
						}

						val_uint64 = strtoull(&token->literal[2], NULL, 2);
					}

					// Convert Integer Numbers
					else if (len > 0) {
						if (is_signed)
							val_int64 = strtoll(&token->literal[0], NULL, 10);
						else
							val_uint64 = strtoull(&token->literal[0], NULL, 10);

						if (errno) {
							applog(LOG_ERR, "Syntax Error: Line: %d - Integer value exceeds 64 bits", token->line_num);
							return false;
						}

						if (is_signed) {
							if ((val_int64 >= INT32_MIN) && (val_int64 <= INT32_MAX)) {
								data_type = DT_INT;
							}
							else {
								data_type = DT_LONG;
							}
						}
						else {
							if (val_uint64 <= UINT32_MAX) {
								data_type = DT_UINT;
							}
							else {
								data_type = DT_ULONG;
							}
						}
					}
				}
				else if (token->data_type == DT_FLOAT) {

					val_double = strtod(&token->literal[0], NULL);

					if (errno) {
						applog(LOG_ERR, "Syntax Error: Line: %d - Decimal value exceeds 64 bits", token->line_num);
						return false;
					}

					if ((val_double >= FLT_MIN) && (val_double <= FLT_MAX)) {
						data_type = DT_FLOAT;
					}
					else {
						data_type = DT_DOUBLE;
					}
				}
				else {
					svalue = calloc(1, strlen(token->literal) + 1);
					if (!svalue)
						return false;
					strcpy(svalue, token->literal);
				}
			}
		}
		// Unary Expressions
		else if (token->inputs == 1) {

			left = pop_exp();

			// Remove Expression For Variables w/ Constant Index
			if (node_type == NODE_VAR_CONST) {
				val_uint64 = left->uvalue;
				left = NULL;
			}

			// Set Indicator For m[] Array
			if (((node_type == NODE_VAR_CONST) || (node_type == NODE_VAR_EXP)) && (token->data_type == DT_NONE)) {
				is_vm_mem = true;
				data_type = DT_UINT;
			}

		}
		// Binary Expressions
		else if (token->inputs == 2) {
			right = pop_exp();
			left = pop_exp();
		}
		
		break;

	case EXP_STATEMENT:

		// Unary Statements
		if (token->inputs == 1) {
			left = pop_exp();
			if (node_type == NODE_CALL_FUNCTION) {
				svalue = &left->svalue[0];
				left = NULL;
			}
		}
		// Binary Statements
		else if (token->inputs == 2) {
			if (node_type == NODE_BLOCK && stack_exp[stack_exp_idx]->type != NODE_BLOCK)
				right = NULL;
			else
				right = pop_exp();
			left = pop_exp();

			if (node_type == NODE_FUNCTION) {
				svalue = left->svalue;
				left = NULL;
			}
		}
		// Repeat Statements
		else if (node_type == NODE_REPEAT) {
			right = pop_exp();				// Block
			val_int64 = pop_exp()->uvalue;	// Max # Of Iterations
			left = pop_exp();				// # Of Iterations
			val_uint64 = pop_exp()->uvalue;	// Loop Counter

			if (val_int64 <= 0) {
				applog(LOG_ERR, "Syntax Error: Line: %d - Invalid value for max iterations", token->line_num);
				return false;
			}

			if ((left->type == NODE_CONSTANT) && (left->uvalue > (uint64_t)val_int64)) {
				applog(LOG_ERR, "Syntax Error: Line: %d - Number of iterations exceeds maximum", token->line_num);
				return false;
			}
		}
		break;

	case EXP_FUNCTION:

		if (token->inputs > 0) {
			// First Paramater
			left = pop_exp();
			exp = add_exp(NODE_PARAM, EXP_EXPRESSION, false, 0, 0, 0.0, NULL, 0, 0, DT_NONE, left, NULL);
			push_exp(exp);

			// Remaining Paramaters
			for (i = 1; i < token->inputs; i++) {
				right = pop_exp();
				left = pop_exp();
				exp = add_exp(NODE_PARAM, EXP_EXPRESSION, false, 0, 0, 0.0, NULL, 0, 0, DT_NONE, left, right);
				push_exp(exp);
			}
			left = NULL;
			right = pop_exp();
		}
		else {
			left = NULL;
			right = NULL;
		}
	}

	exp = add_exp(node_type, token->exp, is_vm_mem, val_int64, val_uint64, val_double, svalue, token_num, token->line_num, data_type, left, right);

	// Update The "End Statement" Indicator For If/Else/Repeat/Block/Function/Result
	if ((exp->type == NODE_IF) || (exp->type == NODE_ELSE) || (exp->type == NODE_REPEAT) || (exp->type == NODE_BLOCK) || (exp->type == NODE_FUNCTION) || (exp->type == NODE_RESULT))
		exp->end_stmnt = true;

	if (exp)
		push_exp(exp);
	else
		return false;

	return true;
}

extern bool parse_token_list(SOURCE_TOKEN_LIST *token_list) {

	int i, j, token_id;
	ast *left, *right;
	bool found;

	// Used To Validate Inputs
	num_exp = 0;

	for (i = 0; i < token_list->num; i++) {

		// Process Existing Items On The Stack
		if ((token_list->token[i].type == TOKEN_END_STATEMENT) ||
			(token_list->token[i].type == TOKEN_BLOCK_END) ||
			(token_list->token[i].type == TOKEN_VAR_END) ||
			(token_list->token[i].type == TOKEN_CLOSE_PAREN) ||
			(token_list->token[i].type == TOKEN_COMMA) ||
			(token_list->token[i].type == TOKEN_COND_ELSE)) {

			while ((top_op >= 0) && (token_list->token[top_op].prec >= token_list->token[i].prec)) {

				// The Following Operators Require Special Handling
				if ((token_list->token[top_op].type == TOKEN_OPEN_PAREN) ||
					(token_list->token[top_op].type == TOKEN_BLOCK_BEGIN) ||
					(token_list->token[top_op].type == TOKEN_VAR_BEGIN) ||
					(token_list->token[top_op].type == TOKEN_IF) ||
					(token_list->token[top_op].type == TOKEN_ELSE) ||
					(token_list->token[top_op].type == TOKEN_REPEAT) ) {
					break;
				}

				// Add Expression To Stack
				token_id = pop_op();
				if (!create_exp(&token_list->token[token_id], token_id)) return false;
			}
		}

		// Process If/Else/Repeat Operators On Stack
		while ((top_op >= 0) && (stack_exp_idx >= 1) &&
			((token_list->token[top_op].type == TOKEN_IF) || (token_list->token[top_op].type == TOKEN_ELSE) || (token_list->token[top_op].type == TOKEN_REPEAT))) {

			// Validate That If/Repeat Condition Is On The Stack
			if (((token_list->token[top_op].type == TOKEN_IF) || (token_list->token[top_op].type == TOKEN_REPEAT)) &&
				((stack_exp[stack_exp_idx - 1]->token_num < top_op) || (stack_exp[stack_exp_idx - 1]->end_stmnt)))
				break;

			// Validate That Else Left Statement Is On The Stack
			if ((token_list->token[top_op].type == TOKEN_ELSE) && (!stack_exp[stack_exp_idx - 1]->end_stmnt))
				break;

			// Validate That If/Else/Repeat Statement Is On The Stack
			if ((stack_exp[stack_exp_idx]->token_num < top_op) || (!stack_exp[stack_exp_idx]->end_stmnt))
				break;

			// Add If/Else/Repeat Expression To Stack
			token_id = pop_op();
			if (!create_exp(&token_list->token[token_id], token_id)) return false;

			// Only Process A Single Statement When "Else" Token Arrives.  Still Need To Process Rest Of Else
			if (token_list->token[i].type == TOKEN_ELSE)
				break;
		}

		// Process Token
		switch (token_list->token[i].type) {

		case TOKEN_COMMA:
			continue;
			break;

		case TOKEN_LITERAL:
		case TOKEN_TRUE:
		case TOKEN_FALSE:
			if (!create_exp(&token_list->token[i], i)) return false;
			break;

		case TOKEN_END_STATEMENT:
			// Flag Last Item On Stack As A Statement
			if (!stack_exp[stack_exp_idx]->end_stmnt) {
				stack_exp[stack_exp_idx]->end_stmnt = true;
				num_exp--;
			}
			break;

		case TOKEN_VAR_END:
			// Validate That The Top Operator Is The Var Begin
			if (token_list->token[top_op].type != TOKEN_VAR_BEGIN) {
				applog(LOG_ERR, "Syntax Error: Line: %d - Missing '['\n", token_list->token[i].line_num);
				return false;
			}
			if ((stack_exp_idx < 0) || stack_exp[stack_exp_idx]->token_num < top_op) {
				applog(LOG_ERR, "Syntax Error: Line: %d - Missing variable index\n", token_list->token[i].line_num);
				return false;
			}

			// Set TOKEN_VAR_END To Match Data Type 
			token_list->token[i].data_type = token_list->token[stack_op[stack_op_idx]].data_type;

			pop_op();
			if (!create_exp(&token_list->token[i], i)) return false;

			// Check For Unary Operators On The Variable
			while ((top_op >= 0) && (token_list->token[top_op].type != TOKEN_VAR_BEGIN) && (token_list->token[top_op].exp == EXP_EXPRESSION) && (token_list->token[top_op].inputs <= 1)) {
				token_id = pop_op();
				if (!create_exp(&token_list->token[token_id], token_id)) return false;
			}

			break;

		case TOKEN_CLOSE_PAREN:
			// Validate That The Top Operator Is The Open Paren
			if (token_list->token[top_op].type != TOKEN_OPEN_PAREN) {
				applog(LOG_ERR, "Syntax Error: Line: %d - Missing '('\n", token_list->token[i].line_num);
				return false;
			}
			pop_op();

			// Check If We Need To Link What's In Parentheses To A Function
			if ((top_op >= 0) && (token_list->token[top_op].exp == EXP_FUNCTION)) {
				token_id = pop_op();
				if (!create_exp(&token_list->token[token_id], token_id))
					return false;
			}
			break;

		case TOKEN_BLOCK_END:
			// Validate That The Top Operator Is The Block Begin
			if (token_list->token[top_op].type != TOKEN_BLOCK_BEGIN) {
				applog(LOG_ERR, "Syntax Error: Line: %d - Missing '{'\n", token_list->token[i].line_num);
				return false;
			}

			// Create Block For First Statement
			if (stack_exp_idx > 0) {
				if (!create_exp(&token_list->token[i], top_op)) return false;
				stack_exp[stack_exp_idx]->end_stmnt = true;
			}
			else {
				applog(LOG_ERR, "Syntax Error: Line: %d - '{}' Needs to include at least one statement\n", token_list->token[i].line_num);
				return false;
			}

			// Create A Linked List Of Remaining Statements In The Block
			while (stack_exp_idx > 0 && stack_exp[stack_exp_idx - 1]->token_num > top_op && stack_exp[stack_exp_idx]->token_num < i) {
				if (!create_exp(&token_list->token[i], top_op)) return false;
				stack_exp[stack_exp_idx]->end_stmnt = true;
			}
			pop_op();

			// Link Block To If/Repeat/Function Statement
			while ((top_op >= 0) && (token_list->token[top_op].type == TOKEN_IF || token_list->token[top_op].type == TOKEN_ELSE || token_list->token[top_op].type == TOKEN_REPEAT || token_list->token[top_op].type == TOKEN_FUNCTION)) {
					token_id = pop_op();
				if (!create_exp(&token_list->token[token_id], token_id))
					return false;
			}
			break;

		case TOKEN_ELSE:
			// Validate That "Else" Has A Corresponding "If"
			if ((stack_exp_idx < 0) || stack_exp[stack_exp_idx]->type != NODE_IF) {
				applog(LOG_ERR, "Syntax Error: Line: %d - Missing 'If'\n", token_list->token[i].line_num);
				return false;
			}

			// Put If Operator Back On Stack For Later Processing
			push_op(stack_exp[stack_exp_idx]->token_num);

			left = stack_exp[stack_exp_idx]->left;
			right = stack_exp[stack_exp_idx]->right;

			// Remove If Expression From Stack
			pop_exp();

			// Return Left & Right Expressions Back To Stack
			push_exp(left);
			push_exp(right);
			push_op(i);
			break;

		case TOKEN_COND_ELSE:
			// Validate That The Top Operator Is The Conditional
			if (stack_op_idx < 0 || token_list->token[stack_op[stack_op_idx]].type != TOKEN_CONDITIONAL) {
				applog(LOG_ERR, "Syntax Error: Line: %d - Invalid 'Conditional' Statement\n", token_list->token[token_id].line_num);
				return false;
			}
			push_op(i);
			break;

		case TOKEN_BREAK:
		case TOKEN_CONTINUE:
			// Validate That "Break" & "Continue" Are Tied To "Repeat"
			found = false;
			for (j = 0; j < stack_op_idx; j++) {
				if (token_list->token[stack_op[j]].type == TOKEN_REPEAT) {
					found = true;
					push_op(i);
					break;
				}
			}

			if (!found) {
				applog(LOG_ERR, "Syntax Error: Line: %d - Invalid '%s' Statement\n", token_list->token[i].line_num, (token_list->token[i].type == TOKEN_BREAK ? "Break" : "Continue"));
				return false;
			}
			break;

		default:
			// Process Expressions Already In Stack Based On Precedence
			while ((top_op >= 0) && (token_list->token[top_op].prec <= token_list->token[i].prec)) {

				// The Following Operators Require Special Handling
				if ((token_list->token[top_op].type == TOKEN_FUNCTION) ||
					(token_list->token[top_op].type == TOKEN_OPEN_PAREN) ||
					(token_list->token[top_op].type == TOKEN_BLOCK_BEGIN) ||
					(token_list->token[top_op].type == TOKEN_VAR_BEGIN) ||
					(token_list->token[top_op].type == TOKEN_IF) ||
					(token_list->token[top_op].type == TOKEN_ELSE) ||
					(token_list->token[top_op].type == TOKEN_REPEAT) ||
					(token_list->token[top_op].type == TOKEN_CONDITIONAL)) {
					break;
				}

				token_id = pop_op();
				if (!create_exp(&token_list->token[token_id], token_id))
					return false;
			}

			push_op(i);
			break;
		}
	}

	if (!validate_ast())
		return false;

	return true;
}

static bool validate_ast() {
	int i, storage_cnt_idx = 0, storage_idx_idx = 0;
	
	ast_func_idx = 0;

	if ((stack_exp_idx < 0) || (stack_op_idx >= 0)) {
		applog(LOG_ERR, "Fatal Error: Unable to parse source into ElasticPL");
		return false;
	}

	// Get Index Of First Function
	for (i = 0; i < stack_exp_idx; i++) {
		if ((stack_exp[i]->type != NODE_ARRAY_INT) && (stack_exp[i]->type != NODE_ARRAY_UINT) && (stack_exp[i]->type != NODE_ARRAY_LONG) && (stack_exp[i]->type != NODE_ARRAY_ULONG) && (stack_exp[i]->type != NODE_ARRAY_FLOAT) && (stack_exp[i]->type != NODE_ARRAY_DOUBLE) && (stack_exp[i]->type != NODE_STORAGE_CNT) && (stack_exp[i]->type != NODE_STORAGE_IDX)) {
			break;
		}
		ast_func_idx++;
	}

	if (ast_func_idx == 0) {
		applog(LOG_ERR, "Syntax Error: Line: %d - At least one variable array must be declared", stack_exp[0]->line_num);
		return false;
	}

	// Get Index Of Storage Declarations
	for (i = 0; i < stack_exp_idx; i++) {
		if (stack_exp[i]->type == NODE_STORAGE_CNT) {
			if (storage_cnt_idx) {
				applog(LOG_ERR, "Syntax Error: Line: %d - Storage declaration 'storage_cnt' can only be declared once", stack_exp[i]->line_num);
				return false;
			}
			storage_cnt_idx = i;
		}
		else if (stack_exp[i]->type == NODE_STORAGE_IDX) {
			if (storage_idx_idx) {
				applog(LOG_ERR, "Syntax Error: Line: %d - Storage declaration 'storage_import_idx' can only be declared once", stack_exp[i]->line_num);
				return false;
			}
			storage_idx_idx = i;
		}
	}

	// If Storage Is Declared, Ensure Both Count & Index Are There
	if (storage_cnt_idx || storage_idx_idx) {
		if (!storage_cnt_idx || !ast_storage_cnt) {
			applog(LOG_ERR, "Syntax Error: 'storage_cnt' must be declared and greater than zero");
			return false;
		}
		else if (!storage_idx_idx) {
			applog(LOG_ERR, "Syntax Error: Missing 'storage_idx' declaration");
			return false;
		}
	}

	if (!validate_functions())
		return false;

	return true;
}

static bool validate_functions() {
	int i;
	ast *exp;

	ast_main_idx = 0;
	ast_verify_idx = 0;

	for (i = ast_func_idx; i <= stack_exp_idx; i++) {

		if (stack_exp[i]->type != NODE_FUNCTION) {
			applog(LOG_ERR, "Syntax Error: Line: %d - Statements must be contained in functions", stack_exp[i]->line_num);
			return false;
		}

		// Validate That Only One Instance Of "Main" Function Exists
		if (!strcmp(stack_exp[i]->svalue, ("main"))) {
			if (ast_main_idx > 0) {
				applog(LOG_ERR, "Syntax Error: Line: %d - \"main\" function already declared", stack_exp[i]->line_num);
				return false;
			}
			ast_main_idx = i;
		}

		// Validate That Only One Instance Of "Verify" Function Exists
		else if (!strcmp(stack_exp[i]->svalue, ("verify"))) {
			if (ast_verify_idx > 0) {
				applog(LOG_ERR, "Syntax Error: Line: %d - \"verify\" function already declared", stack_exp[i]->line_num);
				return false;
			}
			ast_verify_idx = i;
		}

		// Validate Function Has Brackets
		if (!stack_exp[i]->right) {
			applog(LOG_ERR, "Syntax Error: Line: %d - Function missing {} brackets", stack_exp[i]->line_num);
			return false;
		}

		// Validate Function Has At Least One Statement
		if (!stack_exp[i]->right->left) {
			applog(LOG_ERR, "Syntax Error: Line: %d - Functions must have at least one statement", stack_exp[i]->line_num);
			return false;
		}

		// Validate Function Only Contains Valid Statements
		exp = stack_exp[i];
		while (exp->right) {
			if (exp->right->left && !exp->right->left->end_stmnt) {
				applog(LOG_ERR, "Syntax Error: Line: %d - Invalid Statement", exp->line_num);
				return false;
			}
			exp = exp->right;
		}
	}

	// Validate That "Main" Function Exists
	if (ast_main_idx == 0) {
		applog(LOG_ERR, "Syntax Error: \"main\" function not declared");
		return false;
	}

	// Validate That "Verify" Function Exists
	if (ast_verify_idx == 0) {
		applog(LOG_ERR, "Syntax Error: \"verify\" function not declared");
		return false;
	}

	// Check For Recursive Calls To Functions
	if (!validate_function_calls(ast_main_idx, ast_verify_idx))
		return false;

	return true;
}

static bool validate_function_calls() {
	int i, call_idx, rpt_idx;
	bool downward = true;
	ast *root = NULL;
	ast *ast_ptr = NULL;
	ast *call_stack[CALL_STACK_SIZE];
	ast *rpt_stack[REPEAT_STACK_SIZE];

	if (opt_debug_epl) {
		fprintf(stdout, "\n*********************************************************\n");
		fprintf(stdout, "Function Calls\n");
		fprintf(stdout, "*********************************************************\n");
		fprintf(stdout, "Call Function: 'main()'\n");
	}

	// Set Root To Main Function
	root = stack_exp[ast_main_idx];

	rpt_idx = 0;
	call_idx = 0;
	call_stack[call_idx++] = root;

	ast_ptr = root;

	while (ast_ptr) {

		// Navigate Down The Tree
		if (downward) {

			// Navigate To Lowest Left Parent Node
			while (ast_ptr->left) {
				ast_ptr = ast_ptr->left;

				// Validate Repeat Node
				if (ast_ptr->type == NODE_REPEAT) {

					// Validate That Repeat Counter Has Not Been Used
					for (i = 0; i < rpt_idx; i++) {
						if (ast_ptr->uvalue == rpt_stack[i]->uvalue) {
							applog(LOG_ERR, "Syntax Error: Line: %d - Repeat loop counter already used", ast_ptr->line_num);
							return false;
						}
					}

					rpt_stack[rpt_idx++] = ast_ptr;

					if (rpt_idx >= REPEAT_STACK_SIZE) {
						applog(LOG_ERR, "Syntax Error: Line: %d - Repeat statements can only be nested up to %d levels", ast_ptr->line_num, REPEAT_STACK_SIZE - 1);
						return false;
					}

					if (opt_debug_epl)
						fprintf(stdout, "\tBegin Repeat (Line #%d)\t- Depth: %d\n", ast_ptr->line_num, rpt_idx);
				}
			}

			// Switch To Root Of Called Function
			if (ast_ptr->type == NODE_CALL_FUNCTION) {
				if (ast_ptr->left)
					ast_ptr = ast_ptr->left;

				if (opt_debug_epl)
					fprintf(stdout, "Call Function: '%s()'\n", ast_ptr->svalue);

				// Get AST Index For The Function
				if (!ast_ptr->uvalue) {

					for (i = 0; i <= stack_exp_idx; i++) {
						if ((stack_exp[i]->type == NODE_FUNCTION) && !strcmp(stack_exp[i]->svalue, ast_ptr->svalue))
							ast_ptr->uvalue = i;
					}
				}

				// Validate Function Exists
				if (!ast_ptr->uvalue) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Function '%s' not found", ast_ptr->line_num, ast_ptr->svalue);
					return false;
				}

				// Validate That "main" & "verify" Functions Are Not Called
				if ((ast_ptr->uvalue == ast_main_idx) || (ast_ptr->uvalue == ast_verify_idx)) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Illegal function call", ast_ptr->line_num);
					return false;
				}

				// Validate That Functions Is Not Recursively Called
				for (i = 0; i < call_idx; i++) {
					if (ast_ptr->uvalue == call_stack[i]->uvalue) {
						applog(LOG_ERR, "Syntax Error: Line: %d - Illegal recursive function call", ast_ptr->line_num);
						return false;
					}
				}

				// Store The Lowest Level In Call Stack For The Function
				// Needed To Determine Order Of Processing Functions During WCET Calc
				if (call_idx > stack_exp[ast_ptr->uvalue]->uvalue)
					stack_exp[ast_ptr->uvalue]->uvalue = call_idx;

				call_stack[call_idx++] = ast_ptr;
				ast_ptr = stack_exp[ast_ptr->uvalue];

				if (call_idx >= CALL_STACK_SIZE) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Functions can only be nested up to %d levels", ast_ptr->line_num, CALL_STACK_SIZE - 1);
					return false;
				}

			}

			// If There Is A Right Node, Switch To It
			if (ast_ptr->right) {
				ast_ptr = ast_ptr->right;

				// Validate Repeat Node
				if (ast_ptr->type == NODE_REPEAT) {

					// Validate That Repeat Counter Has Not Been Used
					for (i = 0; i < rpt_idx; i++) {
						if (ast_ptr->uvalue == rpt_stack[i]->uvalue) {
							applog(LOG_ERR, "Syntax Error: Line: %d - Repeat loop counter already used", ast_ptr->line_num);
							return false;
						}
					}

					rpt_stack[rpt_idx++] = ast_ptr;

					if (rpt_idx >= REPEAT_STACK_SIZE) {
						applog(LOG_ERR, "Syntax Error: Line: %d - Repeat statements can only be nested up to %d levels", ast_ptr->line_num, REPEAT_STACK_SIZE - 1);
						return false;
					}

					if (opt_debug_epl)
						fprintf(stdout, "\tBegin Repeat (Line #%d)\t- Depth: %d\n", ast_ptr->line_num, rpt_idx);
				}
			}
			// Otherwise, Print Current Node & Navigate Back Up The Tree
			else {
				downward = false;
			}
		}

		// Navigate Back Up The Tree
		else {

			// Quit When We Reach The Root Of Main Function
			if (ast_ptr == root)
				break;

			// Remove 'Repeat' From Stack
			if (ast_ptr->type == NODE_REPEAT) {
				if (opt_debug_epl)
					fprintf(stdout, "\t  End Repeat (Line #%d)\n", ast_ptr->line_num);
				rpt_stack[rpt_idx--] = 0;
			}

			// Return To Calling Function When We Reach The Root Of Called Function
			if (ast_ptr->parent->type == NODE_FUNCTION) {
				call_stack[call_idx--] = 0;
				ast_ptr = call_stack[call_idx];
				if (opt_debug_epl)
					fprintf(stdout, "Return From:   '%s()'\n", call_stack[call_idx]->svalue);
			}
			else {
				// Check If We Need To Navigate Back Down A Right Branch
				if ((ast_ptr == ast_ptr->parent->left) && (ast_ptr->parent->right)) {
					ast_ptr = ast_ptr->parent->right;
					downward = true;
				}
				else {
					ast_ptr = ast_ptr->parent;
				}
			}
		}
	}
	return true;
}
