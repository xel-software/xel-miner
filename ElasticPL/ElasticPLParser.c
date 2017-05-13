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

#include "ElasticPL.h"
#include "../miner.h"

int num_exp = 0;

int d_stack_op[10];	// DEBUG
ast *d_stack_exp[10];	// DEBUG


static ast* add_exp(NODE_TYPE node_type, EXP_TYPE exp_type, bool is_64bit, bool is_signed, bool is_float, int64_t val_int64, uint64_t val_uint64, double val_double, unsigned char *svalue, int token_num, int line_num, DATA_TYPE data_type, ast* left, ast* right) {
	ast* e = calloc(1, sizeof(ast));
	if (e) {
		e->type = node_type;
		e->exp = exp_type;
		e->is_64bit = is_64bit;
		e->is_signed = is_signed;
		e->is_float = is_float;
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

		// ElasticPL Operator Nones Inherit Data Type From Child Nodes
		if ((data_type != DT_NONE) && (node_type != NODE_VAR_CONST) && (node_type != NODE_VAR_EXP) && (node_type != NODE_CONSTANT)) {
			e->is_64bit = (left ? left->is_64bit : false) | (right ? right->is_64bit : false);
			e->is_signed = (left ? left->is_signed : false) | (right ? right->is_signed : false);
			e->is_float = (left ? left->is_float : false) | (right ? right->is_float : false);

			// Map Data Type Based On Indicators
			if (e->is_float) {
				if (e->is_64bit)
					e->data_type = DT_DOUBLE;
				else
					e->data_type = DT_FLOAT;
			}
			else {
				if (e->is_64bit) {
					if (e->is_signed)
						e->data_type = DT_LONG;
					else
						e->data_type = DT_ULONG;
				}
				else {
					if (e->is_signed)
						e->data_type = DT_INT;
					else
						e->data_type = DT_UINT;
				}
			}
		}


		// Map Constants Based On Indicators
		//if (node_type == NODE_CONSTANT) {
		//	if (e->is_float) {
		//		if (e->is_64bit)
		//			e->val.d = val_double;
		//		else
		//			e->val.f = (float)val_double;
		//	}
		//	else {
		//		if (e->is_64bit) {
		//			if (e->is_signed)
		//				e->val.l = val_int64;
		//			else
		//				e->val.ul = val_uint64;
		//		}
		//		else {
		//			if (e->is_signed)
		//				e->val.i = (int32_t)val_int64;
		//			else
		//				e->val.u = (int32_t)val_uint64;
		//		}
		//	}
		//}
		//else if (node_type == NODE_VAR_CONST)
		//	e->val.u = (uint32_t)val_uint64;

		if (left)
			e->left->parent = e;
		if (right)
			e->right->parent = e;
	}
	return e;
};

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

	// Validate The Inputs Are The Correct Type
	switch (node_type) {

	// VM Memory Declarations
	case NODE_ARRAY_INT:
	case NODE_ARRAY_UINT:
	case NODE_ARRAY_LONG:
	case NODE_ARRAY_ULONG:
	case NODE_ARRAY_FLOAT:
	case NODE_ARRAY_DOUBLE:
		if ((stack_exp[stack_exp_idx]->token_num > token_num) && !stack_exp[stack_exp_idx]->is_signed && !stack_exp[stack_exp_idx]->is_float) {

			switch (node_type) {
			case NODE_ARRAY_INT:
				if (max_vm_ints != 0) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Int array already declared", token->line_num);
					return false;
				}
				max_vm_ints = stack_exp[stack_exp_idx]->uvalue;
				break;
			case NODE_ARRAY_UINT:
				if (max_vm_uints != 0) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Unsigned Int array already declared", token->line_num);
					return false;
				}
				max_vm_uints = stack_exp[stack_exp_idx]->uvalue;
				break;
			case NODE_ARRAY_LONG:
				if (max_vm_longs != 0) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Long array already declared", token->line_num);
					return false;
				}
				max_vm_longs = stack_exp[stack_exp_idx]->uvalue;
				break;
			case NODE_ARRAY_ULONG:
				if (max_vm_ulongs != 0) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Unsigned Long array already declared", token->line_num);
					return false;
				}
				max_vm_ulongs = stack_exp[stack_exp_idx]->uvalue;
				break;
			case NODE_ARRAY_FLOAT:
				if (max_vm_floats != 0) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Float array already declared", token->line_num);
					return false;
				}
				max_vm_floats = stack_exp[stack_exp_idx]->uvalue;
				break;
			case NODE_ARRAY_DOUBLE:
				if (max_vm_doubles != 0) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Double array already declared", token->line_num);
					return false;
				}
				max_vm_doubles = stack_exp[stack_exp_idx]->uvalue;
				break;
			}

			if ((((max_vm_ints + max_vm_uints + max_vm_floats) * 4) + ((max_vm_longs + max_vm_ulongs + max_vm_doubles) * 8)) > MAX_VM_MEMORY_SIZE) {
				applog(LOG_ERR, "Syntax Error - Requested VM Memory (%d bytes) exceeds allowable (%d bytes)", (((max_vm_ints + max_vm_uints + max_vm_floats) * 4) + ((max_vm_longs + max_vm_ulongs + max_vm_doubles) * 8)), MAX_VM_MEMORY_SIZE);
				return false;
			}
			return true;
		}
		break;

	// Expressions w/ 1 Literal & 1 Block
	case NODE_FUNCTION:
		if ((stack_exp[stack_exp_idx - 1]->type == NODE_CONSTANT) && (stack_exp[stack_exp_idx]->type == NODE_BLOCK))
			return true;
		break;

	// Expressions w/ 1 Int / Float & 1 If/Else/Repeat/Break/Continue Statement
	case NODE_IF:
		if (((stack_exp[stack_exp_idx - 1]->data_type == DT_INT) || (stack_exp[stack_exp_idx - 1]->data_type == DT_FLOAT)) &&
				((stack_exp[stack_exp_idx]->end_stmnt == true) || (stack_exp[stack_exp_idx]->type == NODE_IF) || (stack_exp[stack_exp_idx]->type == NODE_ELSE) || (stack_exp[stack_exp_idx]->type == NODE_REPEAT) || (stack_exp[stack_exp_idx]->type == NODE_BREAK) || (stack_exp[stack_exp_idx]->type == NODE_CONTINUE)))
				return true;
		break;

	// Expressions w/ 1 Int, 1 Constant Int & 1 Block
	case NODE_REPEAT:
		if ((stack_exp_idx > 1) &&
			(stack_exp[stack_exp_idx - 2]->data_type == DT_INT) &&
			(stack_exp[stack_exp_idx - 1]->type == NODE_CONSTANT) &&
			(stack_exp[stack_exp_idx - 1]->value > 0) &&
			(!stack_exp[stack_exp_idx - 1]->is_float) &&
			(stack_exp[stack_exp_idx]->type == NODE_BLOCK))
			return true;
		break;

	case NODE_ELSE:
		if ((stack_exp[stack_exp_idx - 1]->end_stmnt == true) && (stack_exp[stack_exp_idx]->end_stmnt == true))
			return true;
		break;

	// Expressions w/ 1 Int/Uint (Right Operand)
	case NODE_ABS:
	case NODE_VERIFY:
		if ((stack_exp[stack_exp_idx]->token_num > token_num) && (stack_exp[stack_exp_idx]->data_type != DT_NONE))
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

	// Expressions w/ 1 Unsigned Int/Long (Right Operand)
	case NODE_VAR_CONST:
	case NODE_VAR_EXP:
		if ((stack_exp[stack_exp_idx]->token_num < token_num) &&
			((stack_exp[stack_exp_idx]->data_type == DT_UINT) || (stack_exp[stack_exp_idx]->data_type == DT_ULONG))) {

			switch (token->data_type) {
			case DT_INT:
				if (max_vm_ints == 0) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Int array not declared", token->line_num);
					return false;
				}
				break;
			case DT_UINT:
				if (max_vm_uints == 0) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Unsigned Int array not declared", token->line_num);
					return false;
				}
				break;
			case DT_LONG:
				if (max_vm_longs == 0) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Long array not declared", token->line_num);
					return false;
				}
				break;
			case DT_ULONG:
				if (max_vm_ulongs == 0) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Unsigned Long array not declared", token->line_num);
					return false;
				}
				break;
			case DT_FLOAT:
				if (max_vm_floats == 0) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Float array not declared", token->line_num);
					return false;
				}
				break;
			case DT_DOUBLE:
				if (max_vm_doubles == 0) {
					applog(LOG_ERR, "Syntax Error: Line: %d - Double array not declared", token->line_num);
					return false;
				}
				break;
			}
			return true;
		}
		break;

	// Expressions w/ Any 1 Number (Right Operand)
	case NODE_CONSTANT:
		if ((stack_exp[stack_exp_idx]->token_num < token_num) && (stack_exp[stack_exp_idx]->data_type != DT_NONE))
			return true;
		break;

	// Expressions w/ 1 Int or Float (Left Operand)
	case NODE_COMPL:
	case NODE_NOT:
	case NODE_NEG:
		if ((stack_exp[stack_exp_idx]->token_num > token_num) &&
			(stack_exp[stack_exp_idx]->data_type == DT_INT) || (stack_exp[stack_exp_idx]->data_type == DT_UINT) || (stack_exp[stack_exp_idx]->data_type == DT_FLOAT))
			return true;
		break;

	// Expressions w/ 1 Variable (Left Operand) & 1 Int or Float (Right Operand)
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
		if (((stack_exp[stack_exp_idx - 1]->token_num < token_num) && (stack_exp[stack_exp_idx]->token_num > token_num)) &&
			((stack_exp[stack_exp_idx - 1]->type == NODE_VAR_CONST) || (stack_exp[stack_exp_idx - 1]->type == NODE_VAR_EXP)) &&
			(stack_exp[stack_exp_idx]->data_type != DT_NONE))
			return true;
		break;

	// Expressions w/ 2 Ints or Floats
	case NODE_MUL:
	case NODE_DIV:
	case NODE_MOD:
	case NODE_ADD:
	case NODE_SUB:
	case NODE_LROT:
	case NODE_LSHIFT:
	case NODE_RROT:
	case NODE_RSHIFT:
	case NODE_LE:
	case NODE_GE:
	case NODE_LT:
	case NODE_GT:
	case NODE_EQ:
	case NODE_NE:
	case NODE_BITWISE_AND:
	case NODE_BITWISE_XOR:
	case NODE_BITWISE_OR:
	case NODE_AND:
	case NODE_OR:
	case NODE_CONDITIONAL:
	case NODE_COND_ELSE:
		if (((stack_exp[stack_exp_idx - 1]->token_num < token_num) && (stack_exp[stack_exp_idx]->token_num > token_num)) &&
			((stack_exp[stack_exp_idx - 1]->data_type == DT_INT) || (stack_exp[stack_exp_idx - 1]->data_type == DT_UINT) || (stack_exp[stack_exp_idx - 1]->data_type == DT_FLOAT)) &&
			(stack_exp[stack_exp_idx]->data_type == DT_INT) || (stack_exp[stack_exp_idx]->data_type == DT_UINT) || (stack_exp[stack_exp_idx]->data_type == DT_FLOAT))
			return true;
		break;

	// Functions w/ 1 Int or Float
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
			(stack_exp[stack_exp_idx]->data_type == DT_INT) || (stack_exp[stack_exp_idx]->data_type == DT_UINT) || (stack_exp[stack_exp_idx]->data_type == DT_FLOAT))
			return true;
		break;

	// Functions w/ 2 Ints or Floats
	case NODE_ATAN2:
	case NODE_POW:
	case NODE_FMOD:
	case NODE_GCD:
		if (((stack_exp[stack_exp_idx - 1]->token_num > token_num) && (stack_exp[stack_exp_idx]->token_num > token_num)) &&
			((stack_exp[stack_exp_idx - 1]->data_type == DT_INT) || (stack_exp[stack_exp_idx - 1]->data_type == DT_UINT) || (stack_exp[stack_exp_idx - 1]->data_type == DT_FLOAT)) &&
			(stack_exp[stack_exp_idx]->data_type == DT_INT) || (stack_exp[stack_exp_idx]->data_type == DT_UINT) || (stack_exp[stack_exp_idx]->data_type == DT_FLOAT))
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
	case TOKEN_VERIFY:			node_type = NODE_VERIFY;		break;
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
	case TOKEN_FUNCTION:		node_type = NODE_FUNCTION;		break;
	case TOKEN_INIT_ONCE:		node_type = NODE_INIT_ONCE;		break;
	default: return NODE_ERROR;
	}

	return node_type;
}

static bool create_exp(SOURCE_TOKEN *token, int token_num) {
	int i;
	uint32_t val[2];
	uint32_t len;
	bool is_64bit = true;
	bool is_signed = false;
	bool is_float = false;
	int64_t val_int64 = 0;
	uint64_t val_uint64 = 0;
	double val_double = 0.0;
	double value = 0.0;
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
				val_uint64 = 1.0;
				is_64bit = true;
				is_signed = false;
				is_float = false;
			}
			else if (token->type == TOKEN_FALSE) {
				val_uint64 = 0.0;
				is_64bit = true;
				is_signed = false;
				is_float = false;
			}
			else if (node_type == NODE_CONSTANT) {

				data_type = DT_NONE;

				if (token->literal[0] == '-')
					is_signed = true;
				else
					is_signed = false;

				len = strlen(token->literal);

				if (token->data_type == DT_INT) {

					is_float = false;

					// Convert Hex Numbers
					if ((len > 2) && (token->literal[0] == '0') && (token->literal[1] == 'x')) {
						if (len > 18) {
							applog(LOG_ERR, "Syntax Error: Line: %d - Hex value exceeds 64 bits", token->line_num);
							return false;
						}
						else if (len < 11) {
							is_64bit = false;
							token->data_type = DT_UINT;
						}
						else {
							is_64bit = true;
							token->data_type = DT_ULONG;
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
							is_64bit = false;
							token->data_type = DT_UINT;
						}
						else {
							is_64bit = true;
							token->data_type = DT_ULONG;
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
								is_64bit = false;
								token->data_type = DT_INT;
							}
							else {
								is_64bit = true;
								token->data_type = DT_LONG;
							}
						}
						else {
							if (val_uint64 <= UINT32_MAX) {
								is_64bit = false;
								token->data_type = DT_UINT;
							}
							else {
								is_64bit = true;
								token->data_type = DT_ULONG;
							}
						}
					}

					// Check If Conversion Failed

					//// Check For Hex - If Found, Convert To Int
					//if ((strlen(token->literal) > 2) && (strlen(token->literal) <= 10) && (token->literal[0] == '0') && (token->literal[1] == 'x')) {
					//		hex2ints(val, 1, token->literal + 2, strlen(token->literal) - 2);
					//	sprintf(token->literal, "%ll", val[0]);
					//}

					//// Check For Binary - If Found, Convert To Decimal String
					//else if ((strlen(token->literal) > 2) && (strlen(token->literal) <= 34) && (token->literal[0] == '0') && (token->literal[1] == 'b')) {
					//	val[0] = bin2int(token->literal + 2);
					//	sprintf(token->literal, "%ll", val[0]);
					//}

					//// Convert Literal To Corresponding Integer Data Type
					//if (strlen(token->literal) > 0)

					//else if ((strlen(token->literal) > 0) && (strlen(token->literal) <= 11)) {
					//	UINT32_MAX;
					//	INT32_MAX;
					//	if (token->data_type == DT_INT)
					//		ivalue = (int32_t)strtod(token->literal, NULL);
					//	else
					//		uvalue = (uint32_t)strtod(token->literal, NULL);
					//}
				}
				else if (token->data_type == DT_FLOAT) {
					is_float = true;

					val_double = strtod(&token->literal[0], NULL, 10);

					if (errno) {
						applog(LOG_ERR, "Syntax Error: Line: %d - Decimal value exceeds 64 bits", token->line_num);
						return false;
					}

					//if ((val_double < ?) || (val_double > ?)) {
					//	is_64bit = false;
					//	token->data_type = DT_FLOAT;
					//}
					//else {
						is_64bit = true;
						token->data_type = DT_DOUBLE;
//					}
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

			if ((node_type == NODE_VAR_CONST) || (node_type == NODE_VAR_EXP)) {
				switch (token->data_type) {
				case DT_INT:
					is_64bit = false;
					is_signed = true;
					is_float = false;
					break;
				case DT_UINT:
					is_64bit = false;
					is_signed = false;
					is_float = false;
					break;
				case DT_LONG:
					is_64bit = true;
					is_signed = true;
					is_float = false;
					break;
				case DT_ULONG:
					is_64bit = true;
					is_signed = false;
					is_float = false;
					break;
				case DT_FLOAT:
					is_64bit = false;
					is_signed = true;
					is_float = true;
					break;
				case DT_DOUBLE:
					is_64bit = true;
					is_signed = true;
					is_float = true;
					break;
				}
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
		}
		// Binary Statements
		else if (token->inputs == 2) {
			if (node_type == NODE_BLOCK && stack_exp[stack_exp_idx]->type != NODE_BLOCK)
				right = NULL;
			else
				right = pop_exp();
			left = pop_exp();

			if (node_type == NODE_FUNCTION) {
				//if (left->svalue) {
				//	svalue = strdup(left->svalue);
				//	free(left->svalue);
				//}
				svalue = &left->svalue[0];
				left = NULL;
			}
		}
		// Repeat Statements
		else if (node_type == NODE_REPEAT) {
			right = pop_exp();			// Block
			value = pop_exp()->value;	// Max # Of Iterations
			left = pop_exp();			// # Of Iterations
		}
		break;

	case EXP_FUNCTION:

		if (token->inputs > 0) {
			// First Paramater
			left = pop_exp();
			exp = add_exp(NODE_PARAM, EXP_EXPRESSION, true, false, false, 0, 0, 0.0, NULL, 0, 0, DT_NONE, left, NULL);
			push_exp(exp);

			// Remaining Paramaters
			for (i = 1; i < token->inputs; i++) {
				right = pop_exp();
				left = pop_exp();
				exp = add_exp(NODE_PARAM, EXP_EXPRESSION, true, false, false, 0, 0, 0.0, NULL, 0, 0, DT_NONE, left, right);
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

	exp = add_exp(node_type, token->exp, is_64bit, is_signed, is_float, val_int64, val_uint64, val_double, svalue, token_num, token->line_num, token->data_type, left, right);

	// Update The "End Statement" Indicator For If/Else/Repeat/Block
	if ((exp->type == NODE_IF) || (exp->type == NODE_ELSE) || (exp->type == NODE_REPEAT) || (exp->type == NODE_BLOCK) || (exp->type == NODE_FUNCTION))
		exp->end_stmnt = true;

	if (exp)
		push_exp(exp);
	else
		return false;

	return true;
}

static bool validate_exp_list() {
	int i;

	if (stack_exp_idx < 0) {
		applog(LOG_ERR, "Syntax Error - Invalid Source File");
		return false;
	}

	if (stack_exp[stack_exp_idx]->type != NODE_VERIFY) {
		applog(LOG_ERR, "Syntax Error - Missing Verify Statement");
		return false;
	}

	if ((stack_exp[0]->type != NODE_ARRAY_INT) && (stack_exp[0]->type != NODE_ARRAY_UINT) && (stack_exp[0]->type != NODE_ARRAY_FLOAT)) {
		applog(LOG_ERR, "Syntax Error: Line: %d -'int', 'uint', or 'float' must be defined at beginning of program", stack_exp[0]->line_num);
		return false;
	}

	for (i = 1; i < stack_exp_idx; i++) {
		if (((stack_exp[i]->type == NODE_ARRAY_INT) || (stack_exp[i]->type == NODE_ARRAY_UINT) || (stack_exp[i]->type == NODE_ARRAY_FLOAT)) &&
			(stack_exp[i - 1]->type != NODE_ARRAY_INT) && (stack_exp[i - 1]->type != NODE_ARRAY_UINT) && (stack_exp[i - 1]->type != NODE_ARRAY_FLOAT)) {
			applog(LOG_ERR, "Syntax Error: Line: %d -'int', 'uint', or 'float' must be defined at beginning of program", stack_exp[i]->line_num);
			return false;
		}
	}

	for (i = 1; i < stack_exp_idx; i++) {
		//if (stack_exp[i]->type == NODE_INIT_ONCE) {
		//	if ((i > 0) && (stack_exp[i - 1]->type != NODE_ARRAY_INT) && (stack_exp[i - 1]->type != NODE_ARRAY_UINT) && (stack_exp[i - 1]->type != NODE_ARRAY_FLOAT)) {
		//		applog(LOG_ERR, "Syntax Error: Line: %d -Init_Once must be first statement after viable declaration", stack_exp[i]->line_num);
		//		return false;
		//	}

		//	if (stack_exp[i]->left->type != NODE_BLOCK) {
		//		applog(LOG_ERR, "Syntax Error: Line: %d -Init_Once Statement Missing {}", stack_exp[i]->line_num);
		//		return false;
		//	}
		//}

		if (stack_exp[i]->type == NODE_VERIFY) {
			applog(LOG_ERR, "Syntax Error: Line: %d -Invalid Verify Statement", stack_exp[i]->line_num);
			return false;
		}

		if (!stack_exp[i]->end_stmnt) {
			applog(LOG_ERR, "Syntax Error: Line: %d - Invalid Statement", stack_exp[i]->line_num);
			return false;
		}
	}

	if (stack_op_idx >= 0) {
		applog(LOG_ERR, "Syntax Error - Unable To Clear Operator Stack");
		return false;
	}

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
			//if ((i > 0) && (token_list->token[i - 1].type == TOKEN_FUNCTION))
			//	break;

			if (!create_exp(&token_list->token[i], i)) return false;
//			stack_exp[stack_exp_idx]->data_type = token_list->token[i].data_type;
			// Map Data Type
			//if (stack_exp[stack_exp_idx]->is_float) {
			//	if (stack_exp[stack_exp_idx]->is_64bit)
			//		stack_exp[stack_exp_idx]->data_type = DT_FLOAT;
			//	else
			//		stack_exp[stack_exp_idx]->data_type = DT_DOUBLE;
			//}
			//else {
			//	if (stack_exp[stack_exp_idx]->is_64bit)
			//		if (stack_exp[stack_exp_idx]->is_signed)
			//			stack_exp[stack_exp_idx]->data_type = DT_INT;
			//		else
			//			stack_exp[stack_exp_idx]->data_type = DT_UINT;
			//	else
			//		if (stack_exp[stack_exp_idx]->is_signed)
			//			stack_exp[stack_exp_idx]->data_type = DT_LONG;
			//		else
			//			stack_exp[stack_exp_idx]->data_type = DT_ULONG;
			//}
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

			// Link Block To If/Repeat/Init Operator
//			while ((top_op >= 0) && (token_list->token[top_op].type == TOKEN_IF || token_list->token[top_op].type == TOKEN_ELSE || token_list->token[top_op].type == TOKEN_REPEAT || token_list->token[top_op].type == TOKEN_INIT_ONCE)) {
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

		case TOKEN_VERIFY:
			// Validate That "Verify" Is Not Embeded In A Block
			if (stack_op_idx >= 0) {
				applog(LOG_ERR, "Syntax Error: Line: %d - Invalid Verify Statement\n", stack_exp[stack_exp_idx]->line_num);
				return false;
			}

			push_op(i);
			break;

		default:
			push_op(i);
			break;
		}
	}

	if (!validate_exp_list())
		return false;

	return true;
}