#include <glisp.h>

static void VirtualMachineCode_dump(VirtualMachineCode *code)
{
	switch (code->op) {
	case OPADD:
		fprintf(stderr, "OPADD : ");
		break;
	case OPSUB:
		fprintf(stderr, "OPSUB : ");
		break;
	case OPMUL:
		fprintf(stderr, "OPMUL : ");
		break;
	case OPDIV:
		fprintf(stderr, "OPDIV : ");
		break;
	case OPMOV:
		fprintf(stderr, "OPMOV : ");
		break;
	case OPCMP:
		fprintf(stderr, "OPCMP : ");
		break;
	case OPJL:
		fprintf(stderr, "OPJL  : ");
		break;
	case OPJG:
		fprintf(stderr, "OPJG  : ");
		break;
	case OPSTORE:
		fprintf(stderr, "OPSTORE : ");
		fprintf(stderr, "name = %s ", code->name);
		if (code->args != NULL) {
			int i = 0;
			while (code->args[i] != NULL) {
				fprintf(stderr, "args[%d] = %s ", i, code->args[i]);
				i++;
			}
		}
		break;
	case OPLOAD:
		fprintf(stderr, "OPLOAD : ");
		break;
	case OPPUSH:
		fprintf(stderr, "OPPUSH : ");
		break;
	case OPPOP:
		fprintf(stderr, "OPPOP : ");
		break;
	case OPCALL:
		fprintf(stderr, "OPCALL : ");
		break;
	case OPRET:
		fprintf(stderr, "OPRET : ");
		break;
	default:
		break;
	}
	fprintf(stderr, "dst = %d src = %d\n", code->dst, code->src);
}

//===========Bad Code===============//
static int false_jump_register = 0;
static int ret_call_count = 0;
//==================================//
static int stack_count = 0;
VirtualMachineCode *new_VirtualMachineCode(Conscell *c, int base_count)
{
	//asm("int3");
	VirtualMachineCode *ret = (VirtualMachineCode *)gmalloc(sizeof(VirtualMachineCode));
	ret->name = NULL;
	ret->args = NULL;
	ret->dump = VirtualMachineCode_dump;
	if (c == NULL) {
		ret->op = OPRET;
		ret->dst = 0;
		ret->src = stack_count;
		return ret;
	}
	switch (c->type) {
	case ADD:
		ret->op = OPADD;
		ret->dst = stack_count;
		ret->src = stack_count + base_count;
		false_jump_register++;
		break;
	case SUB:
		ret->op = OPSUB;
		ret->dst = stack_count;
		ret->src = stack_count + base_count;
		false_jump_register++;
		break;
	case MULTI:
		ret->op = OPMUL;
		ret->dst = stack_count;
		ret->src = stack_count + base_count;
		false_jump_register++;
		break;
	case DIV:
		ret->op = OPDIV;
		ret->dst = stack_count;
		ret->src = stack_count + base_count;
		false_jump_register++;
		break;
	case NUM:
		ret->op = OPMOV;
		ret->dst = stack_count + base_count;
		ret->src = c->num;
		stack_count++;
		false_jump_register++;
		break;
	case FUNC_ARGS:
		ret->op = OPPUSH;
		ret->dst = stack_count + base_count;
		ret->src = c->num;
		//ret->dst = c->num;
		//ret->src = -1;
		stack_count++;
		false_jump_register++;
		break;
	case IF:
		ret->op = OPCMP;
		ret->dst = false_jump_register + 2;
		ret->src = -1;
		false_jump_register++;
		//ret->src = base_count;
		break;
	case LESS:
		ret->op = OPJL;
		ret->dst = stack_count;
		ret->src = stack_count + base_count;
		false_jump_register++;
		break;
	case GRATER:
		ret->op = OPJG;
		ret->dst = stack_count;
		ret->src = stack_count + base_count;
		false_jump_register++;
		break;
	case DEFUN:
		if (c->cdr->cdr->car != NULL) {
			Conscell *args_cell = c->cdr->cdr->car;
			Conscell *tmp = args_cell;
			int i = 1;
			while (tmp->cdr != NULL) {
				tmp = tmp->cdr;
				i++;
			}
			const char **args = (const char **)gmalloc(sizeof(char *) * i);
			const char *arg_name = args_cell->string;
			args[0] = (char *)gmalloc(strlen(arg_name) + 1);
			strncpy((char *)args[0], arg_name, strlen(arg_name) + 1);
			i = 1;
			while (args_cell->cdr != NULL) {
				args_cell = args_cell->cdr;
				arg_name = args_cell->string;
				args[i] = (char *)gmalloc(strlen(arg_name) + 1);
				strncpy((char *)args[i], arg_name, strlen(arg_name) + 1);
				i++;
			}
			ret->args = args;
		}
		false_jump_register++;
		//through
	case SETQ:
		ret->op = OPSTORE;
		ret->dst = stack_count;
		ret->src = stack_count + base_count;
		Conscell *variable_cell = c->cdr;
		char *variable_name = variable_cell->string;
		size_t variable_name_length = strlen(variable_name) + 1;
		ret->name = (char *)gmalloc(variable_name_length);
		strncpy((char *)ret->name, variable_name, variable_name_length);
		false_jump_register++;
		break;
	case STRING:
		ret->op = OPLOAD;
		ret->dst = stack_count;
		ret->src = -1; //unused parameter
		variable_name = c->string;
		variable_name_length = strlen(variable_name) + 1;
		ret->name = (char *)gmalloc(variable_name_length);
		strncpy((char *)ret->name, variable_name, variable_name_length);
		stack_count++;
		false_jump_register++;
		break;
	case FUNC:
		ret->op = OPCALL;
		ret->dst = stack_count;
		ret->src = -1;
		variable_name = c->string;
		variable_name_length = strlen(variable_name) + 1;
		ret->name = (char *)gmalloc(variable_name_length);
		strncpy((char *)ret->name, variable_name, variable_name_length);
		false_jump_register++;
		break;
	default:
		break;
	}
	return ret;
}

static VirtualMachineCodeArray *Compiler_compile(Compiler *compiler, Conscell *path);
static VirtualMachineCode *local_func_code = NULL;
static int isExecFlag = 1;
static int isConditionFlag = 0;
static int isSetFlag = 0;
static int Compiler_opCompile(Compiler *c, Conscell *path, int isRecursive)
{
	int ret = 0;
	Conscell *root = path;
	int opcount = 0;
	while (root->cdr != NULL) {
		root = root->cdr;
		opcount++;
	}
	int create_inst_num = (isRecursive) ? opcount : opcount - 1;
	//fprintf(stderr, "opcount = [%d]\n", opcount);
	int optype = path->type;
	if (optype == ADD || optype == SUB || optype == MULTI || optype == DIV ||
		optype == IF || optype == LESS || optype == GRATER) {
		int i, n = 0;
		Conscell *tmp = path;
		tmp = tmp->cdr;
		for (i = 0; i < create_inst_num; i++) {
			if (tmp->type == LEFT_PARENTHESIS) {
				//fprintf(stderr, "Recursive Call\n");
				n = Compiler_opCompile(c, tmp->car, 1);//recursive
				ret += n;
				//fprintf(stderr, "n = [%d], i = [%d] ret = [%d]\n", n, i, ret);
				if (!isRecursive) {
					VirtualMachineCode *code = new_VirtualMachineCode(path, ret);
					code->dump(code);
					c->vmcodes->add(c->vmcodes, code);
				}
			} else {
				ret++;
				//fprintf(stderr, "ret = [%d]\n", ret);
				//fprintf(stderr, "Value Cell\n");
				if (!isRecursive) {
					VirtualMachineCode *code = new_VirtualMachineCode(path, ret);
					code->dump(code);
					c->vmcodes->add(c->vmcodes, code);
				}
			}
			tmp = tmp->cdr;
		}
	} else if (optype == SETQ || optype == DEFUN || optype == FUNC) {
		if (!isRecursive) {
			VirtualMachineCode *code = new_VirtualMachineCode(path, 0);
			code->dump(code);
			c->vmcodes->add(c->vmcodes, code);
			if (optype == SETQ ||optype == DEFUN) {
				isSetFlag = 1;
			}
			if (optype == DEFUN) {
				isExecFlag = 0;
				local_func_code = code; //set: ====> local_func_code
			} else if (optype == FUNC && !isExecFlag) {
				fprintf(stderr, "defun\n");
				//TODO :  must support some arguments
				//========== create push code ===========//
				code = new_VirtualMachineCode(path, 0);
				code->op = OPPUSH;
				code->dst = stack_count;
				code->src = -1;
				//=======================================//
				code->dump(code);
				c->vmcodes->add(c->vmcodes, code);
			}
		} else {
			if (optype == FUNC) {
				int n = Compiler_opCompile(c, path->cdr->car, 1);//recursive
				ret += n;
			}
		}
	} else if (optype == IF) {
		//asm("int3");
		Conscell *tmp = path->cdr;
		while (tmp->cdr != NULL) {
			tmp = tmp->cdr;
			if (tmp->type == NUM) {
				VirtualMachineCode *code = new_VirtualMachineCode(tmp, ret);
				code->dump(code);
				c->vmcodes->add(c->vmcodes, code);
			} else {
				Compiler_opCompile(c, tmp, 0);
			}
		}
		//c->vmcodes->dump(c->vmcodes);
	} else {
		while (path->cdr != NULL) {
			path = path->cdr;
			ret++;
		}
	}
	return ret;
}

static VirtualMachineCodeArray *Compiler_compile(Compiler *compiler, Conscell *path)
{
	//asm("int3");
	//fprintf(stderr, "---init---\n");
	Conscell *tmp = NULL;
	while (path->car != NULL && path->type != FUNC) {
		path = path->car;
		if (path->cdr == NULL) break;
		fprintf(stderr, "opcode = ");
		path->printTypeName(path);
		if (path->type == IF) {
			tmp = path->cdr;
			while (tmp->cdr != NULL) {
				if (tmp->type == SETQ || tmp->type == DEFUN) tmp = tmp->cdr;//skip string cell
				tmp = tmp->cdr;
				VirtualMachineCode *code = new_VirtualMachineCode(NULL, 0);//OPRET
				code->dump(code);
				compiler->vmcodes->add(compiler->vmcodes, code);
				//=====TODO : must fix this bad code becase cannot support if nested code.====
				ret_call_count++;
				if (ret_call_count == 2) {
					false_jump_register = 0;//stack_count;
				}
				//============================================================================
				if (tmp->type != LEFT_PARENTHESIS) {
					tmp->printTypeName(tmp);
					VirtualMachineCode *c = new_VirtualMachineCode(tmp, 0);
					c->dump(c);
					compiler->vmcodes->add(compiler->vmcodes, c);
					compiler->vmcodes->dump(compiler->vmcodes);
				} else {
					Compiler_compile(compiler, tmp);//recursive call
				}
			}
			puts("=== create calculate operation code ===");
			Compiler_opCompile(compiler, path, 0);
			puts("=======================================");
			path = path->cdr;
		} else {
			puts("=== create calculate operation code ===");
			Compiler_opCompile(compiler, path, 0);
			puts("=======================================");
			tmp = path;
			while (tmp->cdr != NULL) {
				if (tmp->type == SETQ || tmp->type == DEFUN) tmp = tmp->cdr;//skip string cell
				tmp = tmp->cdr;
				if (tmp->type != LEFT_PARENTHESIS) {
					tmp->printTypeName(tmp);
					VirtualMachineCode *c = new_VirtualMachineCode(tmp, 0);
					c->dump(c);
					compiler->vmcodes->add(compiler->vmcodes, c);
					//compiler->vmcodes->dump(compiler->vmcodes);
				} else {
					Compiler_compile(compiler, tmp);//recursive call
				}
			}
		}
	}
	//fprintf(stderr, "---end---\n");
	return compiler->vmcodes;
}

Compiler *new_Compiler(void)
{
	Compiler *ret = (Compiler *)malloc(sizeof(Compiler));
	ret->vmcodes = new_VirtualMachineCodeArray();
	ret->compile = Compiler_compile;
	VirtualMachineCode *code = new_VirtualMachineCode(NULL, 0);//OPRET
	ret->vmcodes->add(ret->vmcodes, code);
	return ret;
}

#define MAX_VIRTUAL_MEMORY_SIZE 32
static GMap *virtual_memory[MAX_VIRTUAL_MEMORY_SIZE] = {NULL};
static int virtual_memory_address = 0;
GMap *new_GMap(const char *key, void *value)
{
	GMap *ret = (GMap *)gmalloc(sizeof(GMap));
	ret->key = (const char *)gmalloc(strlen(key) + 1);
	strncpy((char *)ret->key, key, strlen(key) + 1);
	ret->value = value;
	//fprintf(stderr, "key = [%s]\n", key);
	//fprintf(stderr, "value = [%d]\n", (int)value);
	return ret;
}

static void store_to_virtual_memory(GMap *map)
{
	virtual_memory[virtual_memory_address] = map;
	virtual_memory_address++;
}

static void *fetch_from_virtual_memory(const char *key)
{
	int i = 0;
	for (i = 0; i < virtual_memory_address; i++) {
		GMap *map = virtual_memory[i];
		if (strlen(key) == strlen(map->key) &&
			!strncmp(map->key, key, strlen(key))) {
			return map->value;
		}
	}
	return NULL;
}

static int search_func_args_from_vmcode(VirtualMachineCode *c, const char *key)
{
	int ret = 0;
	int i = 0;
	while (c->args[i] != NULL) {
		const char *arg_name = c->args[i];
		if (strlen(arg_name) == strlen(key) &&
			!strncmp(arg_name, key, strlen(arg_name) + 1)) {
			ret = 1;
		}
		i++;
	}
	return ret;
}

static int function_arg_stack[MAX_STACK_SIZE] = {0};
static int arg_stack_count = 0;
static int cur_arg = -1; //this value is flag that copys pop num to all function argument
static int VirtualMachine_run(VirtualMachineCodeArray *vmcode)
{
	//asm("int3");
	stack_count = 0;
	int stack[MAX_STACK_SIZE] = {0};
	int vm_stack_top = vmcode->size;
	//fprintf(stderr, "false_jump_register = [%d]\n", false_jump_register);
	VirtualMachineCode **pc = vmcode->a + vm_stack_top - 1;
L_HEAD:
	switch ((*pc)->op) {
	case OPMOV:
		//fprintf(stderr, "OPMOV\n");
		stack[(*pc)->dst] = (*pc)->src;
		pc--;
		goto L_HEAD;
	case OPADD:
		//fprintf(stderr, "OPADD\n");
		stack[(*pc)->dst] += stack[(*pc)->src];
		pc--;
		goto L_HEAD;
	case OPSUB:
		//fprintf(stderr, "OPSUB\n");
		stack[(*pc)->dst] -= stack[(*pc)->src];
		pc--;
		goto L_HEAD;
	case OPMUL:
		//fprintf(stderr, "OPMUL\n");
		stack[(*pc)->dst] *= stack[(*pc)->src];
		pc--;
		goto L_HEAD;
	case OPDIV:
		//fprintf(stderr, "OPDIV\n");
		stack[(*pc)->dst] /= stack[(*pc)->src];
		pc--;
		goto L_HEAD;
	case OPCMP:
		//fprintf(stderr, "OPCMP\n");
		pc -= (*pc)->dst;
		goto L_HEAD;
	case OPJL:
		//fprintf(stderr, "OPJL\n");
		if (stack[(*pc)->dst] < stack[(*pc)->src] && isExecFlag) {
			pc -= 2;
		} else {
			pc -= 3;
		}
		goto L_HEAD;
	case OPJG:
		//fprintf(stderr, "OPJG\n");
		if (stack[(*pc)->dst] > stack[(*pc)->src]) {
			pc -= 2;
		} else {
			pc -= 3;
		}
		goto L_HEAD;
	case OPSTORE: {
		//fprintf(stderr, "OPSTORE\n");
		GMap *map = NULL;
		if ((*pc)->args == NULL) {
			//variable
			map = new_GMap((*pc)->name, (void *)stack[(*pc)->src]);
		} else {
			//function
			local_func_code = *pc;
			int base_code_num = 1; //for excluding OPSTORE code
			VirtualMachineCodeArray *vmcodes = vmcode->copy(vmcode, base_code_num);
			map = new_GMap((*pc)->name, (void *)vmcodes);
		}
		store_to_virtual_memory(map);
		pc--;
		goto L_HEAD;
	}
	case OPLOAD:
		//fprintf(stderr, "OPLOAD\n");
		//load map from virtual memory
		if (local_func_code != NULL && search_func_args_from_vmcode(local_func_code, (*pc)->name)) {
			fprintf(stderr, "This variable is function's argument!!\n");
			fprintf(stderr, "convert virtual machine code from OPLOAD to OPPOP\n");
			(*pc)->op = OPPOP;
			pc--;
			goto L_HEAD;
		}
		void *value = fetch_from_virtual_memory((*pc)->name);
		if (value == NULL) {
			fprintf(stderr, "[ERROR] : undefined variable\n");
			break;
		}
		stack[(*pc)->dst] = (int)value;
		pc--;
		goto L_HEAD;
	case OPCALL:
		//fprintf(stderr, "OPCALL\n");
		if (isExecFlag) {
			VirtualMachineCodeArray *func_vmcode = (VirtualMachineCodeArray *)fetch_from_virtual_memory((*pc)->name);
			if (func_vmcode == NULL) {
				fprintf(stderr, "[ERROR] : undefined function name [%s]\n", (*pc)->name);
				break;
			}
			//fprintf(stderr, "func_vmcode = [%d]\n", (int)func_vmcode);
			int res = VirtualMachine_run(func_vmcode);
			arg_stack_count--;
			cur_arg = -1;
			//fprintf(stderr, "arg_stack_count--\n");
			//fprintf(stderr, "res = [%d]\n", res);
			stack[(*pc)->dst] = res;
		}
		pc--;
		goto L_HEAD;
	case OPPUSH:
		//fprintf(stderr, "OPPUSH\n");
		arg_stack_count++;
		if ((*pc)->src != -1) {
			function_arg_stack[arg_stack_count] = (*pc)->src;
		} else {
			function_arg_stack[arg_stack_count] = stack[(*pc)->dst];
		}
		cur_arg = -1;
		pc--;
		goto L_HEAD;
	case OPPOP:
		//fprintf(stderr, "OPPOP\n");
		if (cur_arg == -1) {
			cur_arg = function_arg_stack[arg_stack_count];
			(*pc)->src = cur_arg;
		} else {
			(*pc)->src = cur_arg;
		}
		stack[(*pc)->dst] = (*pc)->src;
		pc--;
		goto L_HEAD;
	case OPRET:
		//fprintf(stderr, "OPRET\n");
		stack[0] = stack[(*pc)->src];
		pc--;
		break;
	default:
		break;
	}
	if (isSetFlag && (*pc) != NULL) {
		pc = vmcode->a + 1;
		goto L_HEAD; //call OPSTORE for setq or defun
	}
	/*
	  int i = 0;
	  while (i < vm_count) {
	  free(vmcode[i]);
	  vmcode[i] = NULL;
	  i++;
	  }
	*/
	//vmcode->dump(vmcode);
	//arg_stack_count++;
	local_func_code = NULL;
	isExecFlag = 1;
	isConditionFlag = 0;
	isSetFlag = 0;
	ret_call_count = 0;
	false_jump_register = 0;
	return stack[0];
}

VirtualMachine *new_VirtualMachine(void)
{
	VirtualMachine *ret = (VirtualMachine *)gmalloc(sizeof(VirtualMachine));
	ret->run = VirtualMachine_run;
	return ret;
}
