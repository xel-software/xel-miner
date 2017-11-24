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

char *stack_code[CODE_STACK_SIZE];
int stack_code_idx;

// Work ID Used To Make ElasticPL Functions Unique Per Job
char job_suffix[22];

// Hard Coded Tabs...Could Make This Dynamic
char *tab[] = { "\t", "\t\t", "\t\t\t", "\t\t\t\t", "\t\t\t\t\t", "\t\t\t\t\t\t", "\t\t\t\t\t\t\t", "\t\t\t\t\t\t\t" };
int tabs;


static void push_code(char *str) {
	stack_code[stack_code_idx++] = str;
}

static char* pop_code() {
	if (stack_code_idx > 0)
		return stack_code[--stack_code_idx];
	else
		return NULL;
}

extern bool convert_ast_to_c(char *work_str) {
	int i, j;
	char file_name[100];

	// Copy WorkID To Job Suffix
	sprintf(job_suffix, "%s", work_str);

	sprintf(file_name, "./work/job_%s.h", job_suffix);

	FILE* f = fopen(file_name, "w");
	if (!f)
		return false;

	// Write Function Declarations
	for (i = ast_func_idx; i <= stack_exp_idx; i++) {
		if ((i == ast_main_idx) || (i == ast_verify_idx))
			fprintf(f, "void %s_%s(uint32_t *, uint32_t, uint32_t *, uint32_t *, uint32_t *);\n", stack_exp[i]->svalue, job_suffix);
		else
			fprintf(f, "void %s_%s();\n", stack_exp[i]->svalue, job_suffix);
	}
	fprintf(f, "\n");
	fflush(f);

	// Write Function Definitions
	for (i = ast_func_idx; i <= stack_exp_idx; i++) {

		stack_code_idx = 0;
		tabs = 0;

		if (!convert_function(stack_exp[i])) {
			fclose(f);
			return false;
		}

		for (j = 0; j < stack_code_idx; j++) {
			if (stack_code[j]) {
				fprintf(f, "%s", stack_code[j]);
				free(stack_code[j]);
				stack_code[j] = NULL;
			}
		}
		fprintf(f, "\n");
		fflush(f);
	}

	fflush(f);
	fclose(f);

	return true;
}

extern bool convert_ast_to_opencl(FILE* f) {
	int i, j;

	if (!f)
		return false;

	// Write Function Declarations
	for (i = ast_func_idx; i <= stack_exp_idx; i++) {
		if (i == ast_main_idx)
			continue;
		else if (i == ast_verify_idx)
			fprintf(f, "uint %s(uint *target, uint *hash, uint *m%s%s%s%s%s%s%s);\n", \
				stack_exp[i]->svalue, \
				ast_vm_ints ? ", int *i" : "", \
				ast_vm_uints ? ", uint *u" : "", \
				ast_vm_longs ? ", long *l" : "", \
				ast_vm_ulongs ? ", ulong *ul" : "", \
				ast_vm_floats ? ", float *f" : "", \
				ast_vm_doubles ? ", double *d" : "", \
				ast_submit_sz ? ", __global uint *s" : "");
		else
			fprintf(f, "void %s(uint *m%s%s%s%s%s%s%s);\n", \
				stack_exp[i]->svalue, \
				ast_vm_ints ? ", int *i" : "", \
				ast_vm_uints ? ", uint *u" : "", \
				ast_vm_longs ? ", long *l" : "", \
				ast_vm_ulongs ? ", ulong *ul" : "", \
				ast_vm_floats ? ", float *f" : "", \
				ast_vm_doubles ? ", double *d" : "", \
				ast_submit_sz ? ", __global uint *s" : "");
	}
	fprintf(f, "\n");
	fflush(f);

	// Write Function Definitions
	for (i = ast_func_idx; i <= stack_exp_idx; i++) {

		stack_code_idx = 0;
		tabs = 0;

		// Add Variable Declarations For OpenCL 'Execute' Function
		if ((i == ast_main_idx)) {
			fprintf(f, "__kernel void execute(__global uint* base_data, __global uint *rnd, volatile __global uint* result, volatile __global uint* output, volatile __global uint* submit, __global uint* storage) {\n");
			fprintf(f, "\tint j;\n");
			fprintf(f, "\tuint msg[20];\n");
			fprintf(f, "\tuint hash[4];\n");
			fprintf(f, "\tuint target[4];\n");
			fprintf(f, "\tuint m[12];\n");

			if (ast_vm_ints)
				fprintf(f, "\tint i[%d];\n", ast_vm_ints);
			if (ast_vm_uints)
				fprintf(f, "\tuint u[%d];\n", ast_vm_uints);
			if (ast_vm_longs)
				fprintf(f, "\tlong l[%d];\n", ast_vm_longs);
			if (ast_vm_ulongs)
				fprintf(f, "\tulong ul[%d];\n", ast_vm_ulongs);
			if (ast_vm_floats)
				fprintf(f, "\tfloat f[%d];\n", ast_vm_floats);
			if (ast_vm_doubles)
				fprintf(f, "\tdouble d[%d];\n", ast_vm_doubles);
			if (ast_submit_sz)
				fprintf(f, "\tglobal uint* s = &storage[0];\n");

			fprintf(f, "\tuint res = 0;\n\n");
			fprintf(f, "\tint idx = get_global_id(0); // Index in the wavefront Dim1\n");
			fprintf(f, "\tint round_num = idx + (rnd[0] * get_global_size(0));  // Each GPU Thread Gets A Unique Round Number\n\n");

			fprintf(f, "\t// 96 Bytes of base_data is made up of:\n");
			fprintf(f, "\t// 32 Byte Multiplicator\n");
			fprintf(f, "\t// 32 Byte Public Key\n");
			fprintf(f, "\t//  8 Byte Work ID\n");
			fprintf(f, "\t//  8 Byte Block ID\n");
			fprintf(f, "\t// 16 Byte POW Target;\n\n");

			fprintf(f, "\t// Copy base_data Into MD5 Message\n");
			fprintf(f, "\tfor (j = 0; j < 20; j++) // 80 bytes\n");
			fprintf(f, "\t\tmsg[j] = base_data[j];\n\n");

			fprintf(f, "\t// Copy base_data Into Target\n");
			fprintf(f, "\tfor (j = 0; j < 4; j++)\n");
			fprintf(f, "\t\ttarget[j] = base_data[j + 20];\n\n");

			fprintf(f, "\t// Update Round # (Multiplicator[1]) in MD5 Message\n");
			fprintf(f, "\tmsg[1] = round_num;\n\n");

			fprintf(f, "\t// Update GPU Thread ID (Multiplicator[3]) in MD5 Message\n");
			fprintf(f, "\tmsg[3] = idx;\n\n");

			fprintf(f, "\t// Get MD5 Hash of 80 Byte Input\n");
			fprintf(f, "\tmd5((char*)&msg[0], 80, &hash[0]);\n\n");

			fprintf(f, "\t// Randomize Inputs m[0]-m[9]\n");
			fprintf(f, "#pragma unroll\n");
			fprintf(f, "\tfor (j = 0; j < 10; j++) {\n");
			fprintf(f, "\t\tm[j] = swap32(hash[j %% 4]);\n");
			fprintf(f, "\t\tif (j > 4)\n");
			fprintf(f, "\t\t\tm[j] = m[j] ^ m[j - 3];\n");
			fprintf(f, "\t}\n\n");

			fprintf(f, "\t// Set m[10] To Round #\n");
			fprintf(f, "\tm[10] = round_num;\n\n");

			fprintf(f, "\t// Set m[11] To Iteration #\n");
			fprintf(f, "\tm[11] = base_data[3];\n\n");
			fflush(f);
		}

		if (!convert_function(stack_exp[i])) {
			fflush(f);
			fclose(f);
			return false;
		}

		for (j = 0; j < stack_code_idx; j++) {
			if (stack_code[j]) {
				fprintf(f, "%s", stack_code[j]);
				free(stack_code[j]);
				stack_code[j] = NULL;
			}
		}
		fprintf(f, "\n");
		fflush(f);
	}

	return true;
}

static bool convert_function(ast* root) {
	bool downward = true;
	ast *ast_ptr = NULL;

	if (!root)
		return false;

	ast_ptr = root;

	while (ast_ptr) {

		// Navigate Down The Tree
		if (downward) {

			// Navigate To Lowest Left Node
			while (ast_ptr->left) {
				ast_ptr = ast_ptr->left;

				// Indent Statements Below IF / REPEAT
				if ((ast_ptr->type == NODE_IF) || (ast_ptr->type == NODE_REPEAT))
					tabs++;
			}

			if (ast_ptr->type == NODE_FUNCTION)
				if (!convert_node(ast_ptr))
					return false;

			// If There Is A Right Node, Switch To It
			if (ast_ptr->right) {
				ast_ptr = ast_ptr->right;
			}
			// Otherwise, Convert Current Node & Navigate Back Up The Tree
			else {
				if (!convert_node(ast_ptr))
					return false;
				downward = false;
			}
		}

		// Navigate Back Up The Tree
		else {
			if (ast_ptr->parent == root)
				break;

			// Check If We Need To Navigate Back Down A Right Branch
			if ((ast_ptr == ast_ptr->parent->left) && (ast_ptr->parent->right)) {
				ast_ptr = ast_ptr->parent->right;

				if ((ast_ptr->parent->type == NODE_IF) || (ast_ptr->parent->type == NODE_ELSE) || (ast_ptr->parent->type == NODE_REPEAT)) {
					if (!convert_node(ast_ptr->parent))
						return false;

					if ((ast_ptr->type == NODE_IF) || (ast_ptr->type == NODE_REPEAT))
						tabs++;
				}

				downward = true;
			}
			else {
				ast_ptr = ast_ptr->parent;
				if ((ast_ptr->type == NODE_IF) || (ast_ptr->type == NODE_ELSE) || (ast_ptr->type == NODE_REPEAT)) {
					if (tabs) tabs--;
				}
				else if (ast_ptr->type == NODE_BLOCK) {
					if ((ast_ptr->parent->type == NODE_IF) || (ast_ptr->parent->type == NODE_ELSE) || (ast_ptr->parent->type == NODE_REPEAT) || (ast_ptr->parent->type == NODE_FUNCTION)) {
						if (!convert_node(ast_ptr))
							return false;
					}
				}
				else {
					if (!convert_node(ast_ptr))
						return false;
				}
			}
		}
	}

	return true;
}

static bool convert_node(ast* node) {
	char op[10];
	char lcast[16];
	char rcast[16];
	char *lstr = NULL;
	char *rstr = NULL;
	char *str = NULL;
	char *tmp = NULL;
	bool var_exp_flg = false;

	if (!node)
		return false;

	// Get Left & Right Values From Code Stack
	if (!get_node_inputs(node, &lstr, &rstr))
		return false;

	switch (node->type) {
	case NODE_FUNCTION:
		str = malloc(350);
		if (!strcmp(node->svalue, "main")) {
			if (opt_opencl)
				return true;
			else
				sprintf(str, "void %s_%s(uint32_t *bounty_found, uint32_t verify_pow, uint32_t *pow_found, uint32_t *target, uint32_t *hash) {\n", node->svalue, job_suffix);
		}
		else if (!strcmp(node->svalue, "verify")) {
			if (opt_opencl)
				sprintf(str, "uint %s(uint *target, uint *hash, uint *m%s%s%s%s%s%s%s) {\n\tuint res = 0;\n\n", \
					node->svalue, \
					ast_vm_ints ? ", int *i" : "", \
					ast_vm_uints ? ", uint *u" : "", \
					ast_vm_longs ? ", long *l" : "", \
					ast_vm_ulongs ? ", ulong *ul" : "", \
					ast_vm_floats ? ", float *f" : "", \
					ast_vm_doubles ? ", double *d" : "", \
					ast_submit_sz ? ", __global uint *s" : "");
			else
				sprintf(str, "void %s_%s(uint32_t *bounty_found, uint32_t verify_pow, uint32_t *pow_found, uint32_t *target, uint32_t *hash) {\n", node->svalue, job_suffix);
		}
		else {
			if ( opt_opencl )
				sprintf(str, "void %s(uint *m%s%s%s%s%s%s%s) {\n", \
					node->svalue, \
					ast_vm_ints ? ", int *i" : "", \
					ast_vm_uints ? ", uint *u" : "", \
					ast_vm_longs ? ", long *l" : "", \
					ast_vm_ulongs ? ", ulong *ul" : "", \
					ast_vm_floats ? ", float *f" : "", \
					ast_vm_doubles ? ", double *d" : "", \
					ast_submit_sz ? ", __global uint *s" : "");
			else
				sprintf(str, "void %s_%s() {\n", node->svalue, job_suffix);
		}
		break;
	case NODE_CALL_FUNCTION:
		str = malloc(256);
		if (!strcmp(node->svalue, "verify"))
			if (opt_opencl)
				sprintf(str, "res = %s(target, hash, m%s%s%s%s%s%s%s)", \
					node->svalue, \
					ast_vm_ints ? ", i" : "", \
					ast_vm_uints ? ", u" : "", \
					ast_vm_longs ? ", l" : "", \
					ast_vm_ulongs ? ", ul" : "", \
					ast_vm_floats ? ", f" : "", \
					ast_vm_doubles ? ", d" : "", \
					ast_submit_sz ? ", s" : "");
			else
				sprintf(str, "%s_%s(bounty_found, verify_pow, pow_found, target, hash)", node->svalue, job_suffix);
		else {
			if (opt_opencl)
				sprintf(str, "%s(m%s%s%s%s%s%s%s)", \
					node->svalue, \
					ast_vm_ints ? ", i" : "", \
					ast_vm_uints ? ", u" : "", \
					ast_vm_longs ? ", l" : "", \
					ast_vm_ulongs ? ", ul" : "", \
					ast_vm_floats ? ", f" : "", \
					ast_vm_doubles ? ", d" : "", \
					ast_submit_sz ? ", s" : "");
			else
				sprintf(str, "%s_%s()", node->svalue, job_suffix);
		}
		break;
	case NODE_VERIFY_BTY:
		str = malloc(strlen(lstr) + 50);
		if (opt_opencl)
			sprintf(str, "res += (uint)(%s != 0 ? 2 : 0)", lstr);
		else
			sprintf(str, "*bounty_found = (uint32_t)(%s != 0 ? 1 : 0)", lstr);
		break;
	case NODE_VERIFY_POW:
		str = malloc(strlen(lstr) + 150);
		if (opt_opencl)
			sprintf(str, "res += (uint)check_pow(%s, &m[0], &target[0], &hash[0])", lstr);
		else
			sprintf(str, "if (verify_pow == 1)\n\t\t*pow_found = check_pow(%s, &m[0], &target[0], &hash[0]);\n\telse\n\t\t*pow_found = 0", lstr);
		break;
	case NODE_CONSTANT:
		str = malloc(25);
		switch (node->data_type) {
		case DT_INT:
		case DT_LONG:
			sprintf(str, "%lld", node->ivalue);
			break;
		case DT_UINT:
		case DT_ULONG:
			sprintf(str, "%llu", node->uvalue);
			break;
		case DT_FLOAT:
		case DT_DOUBLE:
			sprintf(str, "%f", node->fvalue);
			break;
		default:
			applog(LOG_ERR, "Compiler Error: Invalid constant at Line: %d", node->line_num);
			return false;
		}
		break;
	case NODE_VAR_CONST:
		str = malloc(30);
		switch (node->data_type) {
		case DT_INT:
			sprintf(str, "i[%llu]", ((node->uvalue >= ast_vm_ints) ? 0 : node->uvalue));
			break;
		case DT_UINT:
			if (node->is_vm_mem)
				sprintf(str, "m[%llu]", ((node->uvalue >= ast_vm_uints) ? 0 : node->uvalue));
			else if (node->is_vm_storage)
				sprintf(str, "s[%llu]", ((node->uvalue >= ast_vm_uints) ? 0 : node->uvalue));
			else
				sprintf(str, "u[%llu]", ((node->uvalue >= ast_vm_uints) ? 0 : node->uvalue));
			break;
		case DT_LONG:
			sprintf(str, "l[%llu]", ((node->uvalue >= ast_vm_longs) ? 0 : node->uvalue));
			break;
		case DT_ULONG:
			sprintf(str, "ul[%llu]", ((node->uvalue >= ast_vm_ulongs) ? 0 : node->uvalue));
			break;
		case DT_FLOAT:
			sprintf(str, "f[%llu]", ((node->uvalue >= ast_vm_floats) ? 0 : node->uvalue));
			break;
		case DT_DOUBLE:
			sprintf(str, "d[%llu]", ((node->uvalue >= ast_vm_doubles) ? 0 : node->uvalue));
			break;
		default:
			applog(LOG_ERR, "Compiler Error: Invalid variable at Line: %d", node->line_num);
			return false;
		}
		break;
	case NODE_VAR_EXP:
		if (node->parent->end_stmnt && (node == node->parent->left))
			var_exp_flg = true;

		str = malloc((3 * strlen(lstr)) + 50);

		switch (node->data_type) {
		case DT_INT:
			if (var_exp_flg)
				sprintf(str, "if((%s) < %lu)\n\t%si[%s]", lstr, ast_vm_ints, tab[tabs], lstr);
			else
				sprintf(str, "i[(((%s) < %lu) ? %s : 0)]", lstr, ast_vm_ints, lstr);
			break;
		case DT_UINT:
			if (node->is_vm_mem) {
				if (var_exp_flg)
					sprintf(str, "if((%s) < %lu)\n\t%sm[%s]", lstr, VM_M_ARRAY_SIZE, tab[tabs], lstr);
				else
					sprintf(str, "m[(((%s) < %lu) ? %s : 0)]", lstr, VM_M_ARRAY_SIZE, lstr);
			}
			else if (node->is_vm_storage) {
				if (var_exp_flg)
					sprintf(str, "if((%s) < %lu)\n\t%ss[%s]", lstr, ast_submit_sz, tab[tabs], lstr);
				else
					sprintf(str, "s[(((%s) < %lu) ? %s : 0)]", lstr, ast_submit_sz, lstr);
			}
			else {
				if (var_exp_flg)
					sprintf(str, "if((%s) < %lu)\n\t%su[%s]", lstr, ast_vm_uints, tab[tabs], lstr);
				else
					sprintf(str, "u[(((%s) < %lu) ? %s : 0)]", lstr, ast_vm_uints, lstr);
			}
			break;
		case DT_LONG:
			if (var_exp_flg)
				sprintf(str, "if((%s) < %lu)\n\t%sl[%s]", lstr, ast_vm_longs, tab[tabs], lstr);
			else
				sprintf(str, "l[(((%s) < %lu) ? %s : 0)]", lstr, ast_vm_longs, lstr);
			break;
		case DT_ULONG:
			if (var_exp_flg)
				sprintf(str, "if((%s) < %lu)\n\t%sul[%s]", lstr, ast_vm_ulongs, tab[tabs], lstr);
			else
				sprintf(str, "ul[(((%s) < %lu) ? %s : 0)]", lstr, ast_vm_ulongs, lstr);
			break;
		case DT_FLOAT:
			if (var_exp_flg)
				sprintf(str, "if((%s) < %lu)\n\t%sf[%s]", lstr, ast_vm_floats, tab[tabs], lstr);
			else
				sprintf(str, "f[(((%s) < %lu) ? %s : 0)]", lstr, ast_vm_floats, lstr);
			break;
		case DT_DOUBLE:
			if (var_exp_flg)
				sprintf(str, "if((%s) < %lu)\n\t%sd[%s]", lstr, ast_vm_doubles, tab[tabs], lstr);
			else
				sprintf(str, "d[(((%s) < %lu) ? %s : 0)]", lstr, ast_vm_doubles, lstr);
			break;
		default:
			applog(LOG_ERR, "Compiler Error: Invalid variable at Line: %d", node->line_num);
			return false;
		}
		break;
	case NODE_IF:
		if (tabs < 1) tabs = 1;
		str = malloc(strlen(lstr) + 25);
		// Always Wrap "IF" In Brackets
		sprintf(str, "%sif (%s) {\n", tab[tabs - 1], lstr);
		break;
	case NODE_ELSE:
		if (tabs < 1) tabs = 1;
		str = malloc(25);
		// Check If "IF" Has Closing Bracket - If Not, Add Closing Bracket
		if (node->left->type != NODE_BLOCK) {
			if (node->right->type == NODE_BLOCK)
				sprintf(str, "%s}\n%selse {\n", tab[tabs - 1], tab[tabs - 1]);
			else
				sprintf(str, "%s\n%selse\n", tab[tabs - 1], tab[tabs - 1]);
		}
			sprintf(str, "%selse {\n", tab[tabs - 1]);
		break;
	case NODE_REPEAT:
		str = malloc(strlen(lstr) + 256);
		if (tabs < 1) tabs = 1;
		sprintf(str, "%sint loop%d;\n%sfor (loop%d = 0; loop%d < (%s); loop%d++) {\n%s\tif (loop%d >= %lld) break;\n%s\tu[%lld] = loop%d;\n", tab[tabs - 1], node->token_num, tab[tabs - 1], node->token_num, node->token_num, lstr, node->token_num, tab[tabs - 1], node->token_num, node->ivalue, tab[tabs - 1], node->uvalue, node->token_num);
		break;
	case NODE_BLOCK:
		str = malloc(1000);
		if (node->parent->type == NODE_FUNCTION) {
			if (opt_opencl) {
				if (!strcmp(node->parent->svalue, "main")) {
					if (!ast_submit_sz)
						sprintf(str, "\n\tif (!res)\n\t\treturn;\n\n\tif (res > 1)\n\t\tprintf(\"\\n***** Bounty Found ***** Round: %%u, Thread : %%u\\n\\n\", round_num, idx);\n\n\tresult[0] = res;\n\toutput[0] = idx;\n\toutput[1] = hash[0];\n\toutput[2] = hash[1];\n\toutput[3] = hash[2];\n\toutput[4] = hash[3];\n}\n");
					else
						sprintf(str, "\n\tif (!res)\n\t\treturn;\n\n\tif (res > 1)\n\t\tprintf(\"\\n***** Bounty Found ***** Round: %%u, Thread : %%u\\n\\n\", round_num, idx);\n\n\tresult[0] = res;\n\toutput[0] = idx;\n\toutput[1] = hash[0];\n\toutput[2] = hash[1];\n\toutput[3] = hash[2];\n\toutput[4] = hash[3];\n\n\tfor (j = 0; j < %u; j++)\n\t\tsubmit[j] = u[j + %u];\n}\n", ast_submit_sz, ast_submit_idx);
				}
				else if (!strcmp(node->parent->svalue, "verify"))
					sprintf(str, "\n\treturn res;\n}\n");
				else
					sprintf(str, "}\n");
			}
			else
				sprintf(str, "}\n");
		}
		else
			sprintf(str, "%s}\n", tab[tabs]);
		break;
	case NODE_BREAK:
		str = malloc(10);
		sprintf(str, "break");
		break;
	case NODE_CONTINUE:
		str = malloc(10);
		sprintf(str, "continue");
		break;

	case NODE_CONDITIONAL:
		tmp = pop_code();
		if (!tmp) {
			applog(LOG_ERR, "Compiler Error: Corupted code stack at Line: %d", node->line_num);
			return false;
		}
		str = malloc(strlen(lstr) + strlen(rstr) + strlen(tmp) + 25);
		sprintf(str, "((%s) ? (%s) : (%s))", tmp, lstr, rstr);
		free(tmp);
		break;

	case NODE_COND_ELSE:
		return true;

	case NODE_ADD:
	case NODE_SUB:
	case NODE_MUL:
	case NODE_EQ:
	case NODE_NE:
	case NODE_GT:
	case NODE_LT:
	case NODE_GE:
	case NODE_LE:
	case NODE_AND:
	case NODE_OR:
	case NODE_BITWISE_AND:
	case NODE_BITWISE_XOR:
	case NODE_BITWISE_OR:
	case NODE_LSHIFT:
	case NODE_RSHIFT:
		switch (node->type) {
		case NODE_ADD:			sprintf(op, "%s", "+");		break;
		case NODE_SUB:			sprintf(op, "%s", "-");		break;
		case NODE_MUL:			sprintf(op, "%s", "*");		break;
		case NODE_EQ:			sprintf(op, "%s", "==");	break;
		case NODE_NE:			sprintf(op, "%s", "!=");	break;
		case NODE_GT:			sprintf(op, "%s", ">");		break;
		case NODE_LT:			sprintf(op, "%s", "<");		break;
		case NODE_GE:			sprintf(op, "%s", ">=");	break;
		case NODE_LE:			sprintf(op, "%s", "<=");	break;
		case NODE_AND:			sprintf(op, "%s", "&&");	break;
		case NODE_OR:			sprintf(op, "%s", "||");	break;
		case NODE_BITWISE_AND:	sprintf(op, "%s", "&");		break;
		case NODE_BITWISE_XOR:	sprintf(op, "%s", "^");		break;
		case NODE_BITWISE_OR:	sprintf(op, "%s", "|");		break;
		case NODE_LSHIFT:		sprintf(op, "%s", "<<");	break;
		case NODE_RSHIFT:		sprintf(op, "%s", ">>");	break;
		}
		str = malloc(strlen(lstr) + strlen(rstr) + 25);
		get_cast(lcast, rcast, node->left->data_type, node->right->data_type, false);
		if (lcast[0])
			sprintf(str, "(%s)(%s) %s (%s)", lcast, lstr, op, rstr);
		else if (rcast[0])
			sprintf(str, "(%s) %s (%s)(%s)", lstr, op, rcast, rstr);
		else
			sprintf(str, "(%s) %s (%s)", lstr, op, rstr);
		break;

	case NODE_DIV:
	case NODE_MOD:
		switch (node->type) {
		case NODE_DIV:	sprintf(op, "%s", "/"); break;
		case NODE_MOD:	sprintf(op, "%s", "%"); break;
		}
		str = malloc(strlen(lstr) + strlen(rstr) + strlen(rstr) + 40);
		get_cast(lcast, rcast, node->left->data_type, node->right->data_type, true);
		if (rcast[0])
			sprintf(str, "(((%s) != 0) ? (%s) %s (%s)(%s) : 0)", rstr, lstr, op, rcast, rstr);
		else
			sprintf(str, "(((%s) != 0) ? (%s) %s (%s) : 0)", rstr, lstr, op, rstr);
		break;

	case NODE_ASSIGN:
	case NODE_ADD_ASSIGN:
	case NODE_SUB_ASSIGN:
	case NODE_MUL_ASSIGN:
	case NODE_LSHFT_ASSIGN:
	case NODE_RSHFT_ASSIGN:
	case NODE_AND_ASSIGN:
	case NODE_XOR_ASSIGN:
	case NODE_OR_ASSIGN:
		switch (node->type) {
		case NODE_ASSIGN:		sprintf(op, "%s", "=");		break;
		case NODE_ADD_ASSIGN:	sprintf(op, "%s", "+=");	break;
		case NODE_SUB_ASSIGN:	sprintf(op, "%s", "-=");	break;
		case NODE_MUL_ASSIGN:	sprintf(op, "%s", "*=");	break;
		case NODE_LSHFT_ASSIGN:	sprintf(op, "%s", "<<=");	break;
		case NODE_RSHFT_ASSIGN:	sprintf(op, "%s", ">>=");	break;
		case NODE_AND_ASSIGN:	sprintf(op, "%s", "&=");	break;
		case NODE_XOR_ASSIGN:	sprintf(op, "%s", "^=");	break;
		case NODE_OR_ASSIGN:	sprintf(op, "%s", "|=");	break;
		}
		str = malloc(strlen(lstr) + strlen(rstr) + 25);
		get_cast(lcast, rcast, node->left->data_type, node->right->data_type, true);
		if (rcast[0])
			sprintf(str, "%s %s (%s)(%s)", lstr, op, rcast, rstr);
		else
			sprintf(str, "%s %s %s", lstr, op, rstr);
		break;

	case NODE_DIV_ASSIGN:
	case NODE_MOD_ASSIGN:
		switch (node->type) {
		case NODE_DIV_ASSIGN:	sprintf(op, "%s", "/");	break;
		case NODE_MOD_ASSIGN:	sprintf(op, "%s", "%");	break;
		}
		str = malloc(strlen(lstr) + strlen(lstr) + strlen(rstr) + strlen(rstr) + 40);
		get_cast(lcast, rcast, node->left->data_type, node->right->data_type, true);
		if (rcast[0])
			sprintf(str, "%s = (((%s) != 0) ? (%s) %s (%s)(%s) : 0)", lstr, rstr, lstr, op, rcast, rstr);
		else
			sprintf(str, "%s = (((%s) != 0) ? (%s) %s (%s) : 0)", lstr, rstr, lstr, op, rstr);
		break;

	case NODE_INCREMENT_R:
		str = malloc(strlen(lstr) + 10);
		sprintf(str, "++%s", lstr);
		break;

	case NODE_INCREMENT_L:
		str = malloc(strlen(lstr) + 10);
		sprintf(str, "%s++", lstr);
		break;

	case NODE_DECREMENT_R:
		str = malloc(strlen(lstr) + 10);
		sprintf(str, "--%s", lstr);
		break;

	case NODE_DECREMENT_L:
		str = malloc(strlen(lstr) + 10);
		sprintf(str, "%s--", lstr);
		break;

	case NODE_NOT:
		str = malloc(strlen(lstr) + 10);
		sprintf(str, "!(%s)", lstr);
		break;

	case NODE_COMPL:
		str = malloc(strlen(lstr) + 10);
		sprintf(str, "~(%s)", lstr);
		break;

	case NODE_NEG:
		str = malloc(strlen(lstr) + 10);
		sprintf(str, "-(%s)", lstr);
		break;

	case NODE_LROT:
		str = malloc(strlen(lstr) + strlen(rstr) + 25);
		if (node->is_64bit)
			sprintf(str, "rotl64(%s, %s)", lstr, rstr);
		else
			sprintf(str, "rotl32(%s, %s)", lstr, rstr);
		break;
	case NODE_RROT:
		str = malloc(strlen(lstr) + strlen(rstr) + 25);
		if (node->is_64bit)
			sprintf(str, "rotr64(%s, %s)", lstr, rstr);
		else
			sprintf(str, "rotr32(%s, %s)", lstr, rstr);
		break;

	case NODE_PARAM:
		str = malloc(strlen(lstr) + 1);
		sprintf(str, "%s", lstr);
		break;
	case NODE_ABS:
		str = malloc(strlen(lstr) + 10);
		sprintf(str, "abs(%s)", lstr);
		use_elasticpl_math = true;
		break;
	case NODE_POW:
		str = malloc(strlen(lstr) + strlen(rstr) + 10);
		sprintf(str, "pow(%s, %s)", lstr, rstr);
		use_elasticpl_math = true;
		break;
	case NODE_SIN:
		str = malloc(strlen(lstr) + 10);
		sprintf(str, "sin(%s)", lstr);
		use_elasticpl_math = true;
		break;
	case NODE_COS:
		str = malloc(strlen(lstr) + 10);
		sprintf(str, "cos(%s)", lstr);
		use_elasticpl_math = true;
		break;
	case NODE_TAN:
		str = malloc(strlen(lstr) + 10);
		sprintf(str, "tan(%s)", lstr);
		use_elasticpl_math = true;
		break;
	case NODE_SINH:
		str = malloc(strlen(lstr) + strlen(lstr) + strlen(lstr) + 50);
		sprintf(str, "(((%s >= -1.0) && (%s <= 1.0)) ? sinh( %s ) : 0.0)", lstr, lstr, lstr);
		use_elasticpl_math = true;
		break;
	case NODE_COSH:
		str = malloc(strlen(lstr) + strlen(lstr) + strlen(lstr) + 50);
		sprintf(str, "(((%s >= -1.0) && (%s <= 1.0)) ? cosh( %s ) : 0.0)", lstr, lstr, lstr);
		use_elasticpl_math = true;
		break;
	case NODE_TANH:
		str = malloc(strlen(lstr) + 10);
		sprintf(str, "tanh(%s)", lstr);
		use_elasticpl_math = true;
		break;
	case NODE_ASIN:
		str = malloc(strlen(lstr) + strlen(lstr) + strlen(lstr) + 50);
		sprintf(str, "(((%s >= -1.0) && (%s <= 1.0)) ? asin( %s ) : 0.0)", lstr, lstr, lstr);
		use_elasticpl_math = true;
		break;
	case NODE_ACOS:
		str = malloc(strlen(lstr) + strlen(lstr) + strlen(lstr) + 50);
		sprintf(str, "(((%s >= -1.0) && (%s <= 1.0)) ? acos( %s ) : 0.0)", lstr, lstr, lstr);
		use_elasticpl_math = true;
		break;
	case NODE_ATAN:
		str = malloc(strlen(lstr) + 10);
		sprintf(str, "atan(%s)", lstr);
		use_elasticpl_math = true;
		break;
	case NODE_ATAN2:
		str = malloc(strlen(lstr) + strlen(rstr) + strlen(rstr) + 50);
		sprintf(str, "((%s != 0) ? atan2(%s, %s) : 0.0)", rstr, lstr, rstr);
		use_elasticpl_math = true;
		break;
	case NODE_EXPNT:
		str = malloc(strlen(lstr) + strlen(lstr) + strlen(lstr) + 60);
		sprintf(str, "((((%s) >= -708.0) && ((%s) <= 709.0)) ? exp( %s ) : 0.0)", lstr, lstr, lstr);
		use_elasticpl_math = true;
		break;
	case NODE_LOG:
		str = malloc(strlen(lstr) + strlen(lstr) + 50);
		sprintf(str, "((%s > 0) ? log( %s ) : 0.0)", lstr, lstr);
		use_elasticpl_math = true;
		break;
	case NODE_LOG10:
		str = malloc(strlen(lstr) + strlen(lstr) + 50);
		sprintf(str, "((%s > 0) ? log10( %s ) : 0.0)", lstr, lstr);
		use_elasticpl_math = true;
		break;
	case NODE_SQRT:
		str = malloc(strlen(lstr) + strlen(lstr) + 50);
		if (opt_opencl)
			sprintf(str, "((%s > 0) ? sqrt( (float)%s ) : 0.0)", lstr, lstr);
		else
			sprintf(str, "((%s > 0) ? sqrt( %s ) : 0.0)", lstr, lstr);
		use_elasticpl_math = true;
		break;
	case NODE_CEIL:
		str = malloc(strlen(lstr) + 10);
		sprintf(str, "ceil(%s)", lstr);
		use_elasticpl_math = true;
		break;
	case NODE_FLOOR:
		str = malloc(strlen(lstr) + 10);
		sprintf(str, "floor(%s)", lstr);
		use_elasticpl_math = true;
		break;
	case NODE_FABS:
		str = malloc(strlen(lstr) + 10);
		sprintf(str, "fabs(%s)", lstr);
		use_elasticpl_math = true;
		break;
	case NODE_FMOD:
		str = malloc(strlen(lstr) + strlen(rstr) + strlen(rstr) + 50);
		sprintf(str, "((%s != 0) ? fmod(%s, %s) : 0.0)", rstr, lstr, rstr);
		use_elasticpl_math = true;
		break;
	case NODE_GCD:
		str = malloc(strlen(lstr) + 10);
		sprintf(str, "gcd(%s, %s)", lstr, rstr);
		use_elasticpl_math = true;
		break;
	default:
		applog(LOG_ERR, "Compiler Error: Unknown expression at Line: %d", node->line_num);
		return false;
	}

	if (lstr) free(lstr);
	if (rstr) free(rstr);

	lstr = NULL;
	rstr = NULL;

	// Terminate Statements
	if (node->end_stmnt && (node->type != NODE_IF) && (node->type != NODE_ELSE) && (node->type != NODE_REPEAT) && (node->type != NODE_BLOCK) && (node->type != NODE_FUNCTION)) {
		tmp = malloc(strlen(str) + 20);
		sprintf(tmp, "%s%s;\n", tab[tabs], str);
		free(str);
		push_code(tmp);

		// Add Closing Bracket To IF / ELSE That Don't Have Them
		if (node->parent && ((node->parent->type == NODE_IF) || (node->parent->type == NODE_ELSE)) && node->parent->right != NODE_BLOCK) {
			tmp = malloc(25);
			sprintf(tmp, "%s}\n", tab[tabs-1]);
			push_code(tmp);
		}
	}
	else {
		push_code(str);
	}

	return true;
}

static void get_cast(char *lcast, char *rcast, DATA_TYPE ldata_type, DATA_TYPE rdata_type, bool right_only) {
	lcast[0] = 0;
	rcast[0] = 0;

	if (ldata_type == rdata_type)
		return;
	else if (right_only || (rdata_type < ldata_type)) {
		switch (ldata_type) {
		case DT_UINT:
			sprintf(rcast, "uint32_t");
			break;
		case DT_LONG:
			sprintf(rcast, "int64_t");
			break;
		case DT_ULONG:
			sprintf(rcast, "uint64_t");
			break;
		case DT_FLOAT:
			sprintf(rcast, "float");
			break;
		case DT_DOUBLE:
			sprintf(rcast, "double");
			break;
		}
		return;
	}
	else {
		switch (rdata_type) {
		case DT_UINT:
			sprintf(lcast, "uint32_t");
			break;
		case DT_LONG:
			sprintf(lcast, "int64_t");
			break;
		case DT_ULONG:
			sprintf(lcast, "uint64_t");
			break;
		case DT_FLOAT:
			sprintf(lcast, "float");
			break;
		case DT_DOUBLE:
			sprintf(lcast, "double");
			break;
		}
		return;
	}
}

static bool get_node_inputs(ast* node, char **lstr, char **rstr) {
	char *tmp[4];

	// Get Left & Right Values From Code Stack
	switch (node->type) {
	case NODE_VAR_EXP:
	case NODE_INCREMENT_R:
	case NODE_INCREMENT_L:
	case NODE_DECREMENT_R:
	case NODE_DECREMENT_L:
	case NODE_NOT:
	case NODE_COMPL:
	case NODE_NEG:
	case NODE_IF:
	case NODE_REPEAT:
	case NODE_VERIFY_BTY:
	case NODE_PARAM:
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
	case NODE_ABS:
	case NODE_FABS:
		*lstr = pop_code();
		if (!lstr) {
			applog(LOG_ERR, "Compiler Error: Corupted code stack at Line: %d", node->line_num);
			return false;
		}
		break;
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
	case NODE_ADD:
	case NODE_SUB:
	case NODE_MUL:
	case NODE_DIV:
	case NODE_MOD:
	case NODE_LSHIFT:
	case NODE_LROT:
	case NODE_RSHIFT:
	case NODE_RROT:
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
	case NODE_CONDITIONAL:
	case NODE_POW:
	case NODE_ATAN2:
	case NODE_FMOD:
	case NODE_GCD:
		*rstr = pop_code();
		*lstr = pop_code();
		if (!lstr || !rstr) {
			applog(LOG_ERR, "Compiler Error: Corupted code stack at Line: %d", node->line_num);
			return false;
		}
		break;

	case NODE_VERIFY_POW:
		tmp[0] = pop_code();
		tmp[1] = pop_code();
		tmp[2] = pop_code();
		tmp[3] = pop_code();
		if (!tmp[0] | !tmp[1] | !tmp[2] | !tmp[3] ) {
			applog(LOG_ERR, "Compiler Error: Corupted code stack at Line: %d", node->line_num);
			return false;
		}
		*lstr = malloc(sizeof(tmp[0]) + sizeof(tmp[1]) + sizeof(tmp[2]) + sizeof(tmp[3]) + 10);
		sprintf(*lstr, "%s,%s,%s,%s", tmp[3], tmp[2], tmp[1], tmp[0]);
		break;

	case NODE_ELSE:
	case NODE_BLOCK:
	case NODE_COND_ELSE:
	case NODE_BREAK:
	case NODE_CONTINUE:
		break;
	default:
		break;
	}

	return true;
}
