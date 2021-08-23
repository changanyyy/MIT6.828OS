// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>
#include <kern/pmap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "h",    "Display this list of commands", mon_help },

	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "ki",       "Display information about the kernel", mon_kerninfo },
	
	{ "backtrace", "Display a listing of function call frames", mon_backtrace},
	{ "bt",        "Display a listing of function call frames", mon_backtrace},

	{ "showmappings", "Display the physical page mappings", mon_showmappings},
	{ "sm",           "Display the physical page mappings", mon_showmappings},

	{ "setpermission", "Set permission for a mapping", mon_setpermission},
	{ "sp", 		   "Set permission for a mapping", mon_setpermission},

	{ "continue", "Continue to run the code", mon_continue},
	{ "c",        "Continue to run the code", mon_continue},

	{ "si", "Step one instruction exactly", mon_si},
	{ "s",  "Step one instruction exactly", mon_si},

	{"break", "Set breakpoint at specified location", mon_breakpoint},
	{"b",     "Set breakpoint at specified location", mon_breakpoint}
};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i+=2)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
	uint32_t ebp, old_ebp, eip;
	uint32_t args[5] = {0};
	ebp = read_ebp();
	//ebp = *((uint32_t*)ebp);
	cprintf("Stack backtrace:\n");
	while(ebp != 0){
		old_ebp = *((uint32_t*)ebp);
		eip = *((uint32_t*)(ebp + 4));
		for(int i = 0; i < 5; i++){
			args[i] = *((uint32_t*)(ebp + 8 + 4 * i));
		}
		cprintf("  ebp %x  eip %x  args %08x %08x %08x %08x %08x\n", 
			ebp, eip, args[0], args[1], args[2], args[3], args[4]);
		struct Eipdebuginfo info;
		debuginfo_eip(eip, &info);
		char fn_name[20];
		strncpy(fn_name, info.eip_fn_name, info.eip_fn_namelen);
		fn_name[info.eip_fn_namelen]='\0';
		cprintf("         %s:%d: %s+%d\n", 
			info.eip_file, 
			info.eip_line, 
			fn_name,
			//info.eip_fn_name, 
			eip - info.eip_fn_addr);
		ebp = old_ebp;
	}

	return 0;
}


int 
mon_showmappings(int argc,char **argv, struct Trapframe *tf){
	if(argc != 3){
		cprintf("showmappings: too few arguments\n");
		return -1;
	}
	uintptr_t left = strtol(argv[1], NULL, 16), right = strtol(argv[2], NULL, 16);
	if((unsigned) left > (unsigned) right){
		cprintf("showmappings: left boundary larger than right boundary\n");
		return -1;
	}
	left = ROUNDDOWN(left, PGSIZE);
	right = ROUNDDOWN(right, PGSIZE);
	
	cprintf("Mapping infomations:\n");

	pte_t* ptep = NULL;
	struct PageInfo* pg = NULL;
	char perm_PS = 0, perm_W = 0, perm_U = 0;
	while(left <= right){

		pg = page_lookup(kern_pgdir, (void *)left, &ptep);
		if(pg == NULL){
			cprintf("Virtual address %x haven't been mapped.\n");
		}
		else{
			perm_PS = (*ptep & PTE_PS) ? 'S' : '-';
			perm_W = (*ptep & PTE_W) ? 'W' : '-';
			perm_U = (*ptep & PTE_U) ? 'U' : '-';
			cprintf("Virtual address %08x are mapped to physics address %08x, permission: -%c----%c%cP\n",
				left, 
				page2pa(pg),
				perm_PS,
				perm_U,
				perm_W);
		}

		left += PGSIZE;
	}
	return 0;
}


int
mon_setpermission(int argc, char **argv, struct Trapframe *tf){
	if(argc != 3){
		cprintf("showmappings: incorrect number of arguments\n");
		return -1;
	}
	uintptr_t left = strtol(argv[1], NULL, 16);
	int perm = strtol(argv[2], NULL, 16);
	left = ROUNDDOWN(left, PGSIZE);

	pte_t* ptep = NULL;
	struct PageInfo* pg = NULL;
	char perm_PS = 0, perm_W = 0, perm_U = 0;

	pg = page_lookup(kern_pgdir, (void *)left, &ptep);
	if(pg == NULL){
		cprintf("Virtual address %x haven't been mapped. Failed.\n");
		return -1;
	}
	else{
		page_insert(kern_pgdir,pg, (void *)left, perm);
		perm_PS = (*ptep & PTE_PS) ? 'S' : '-';
		perm_W = (*ptep & PTE_W) ? 'W' : '-';
		perm_U = (*ptep & PTE_U) ? 'U' : '-';
		cprintf("Successfully set permission.\nVirtual address %08x are mapped to physics address %08x, permission: -%c----%c%cP\n",
			left, 
			page2pa(pg),
			perm_PS,
			perm_U,
			perm_W);
	}
	return 0;
}


int
mon_continue(int argc, char **argv, struct Trapframe *tf){
	if(argc != 1){
		cprintf("mon_continue: two many arguments.\n");
		return 0;
	}
	tf->tf_eflags &= ~0x100;
	return -1;
}

extern int step_cnt;

int num_of_breakpoints = 0;

int
mon_si(int argc, char **argv, struct Trapframe *tf){
	if(argc == 1){
		step_cnt = 0;
	}
	else if(argc == 2){
		step_cnt = strtol(argv[1], NULL, 10);
		//cprintf("%d\n", step_cnt);
	}
	tf->tf_eflags |= 0x100;
	return -1;
}


inline int eip2brk(uint32_t eip){
	int i;
	for(i = 0; i < NBRK; i++){
		if(breakpoints[i].eip == eip){
			return i;
		}
	}
	return i;
}

inline int getfreebrk(uint32_t eip){
	int i;
	for(i = 0;i < NBRK; i++){
		if(breakpoints[i].eip == 0){
			return i;
		}
	}
	return i;
}



int
mon_breakpoint(int argc, char **argv, struct Trapframe *tf){
	if(argc != 2){
		cprintf("mon_breakpoint: The number of arguments is incorrect.\n");
	}
	uint32_t eip = strtol((char *)&argv[1][1], NULL, 16);
	//cprintf("eip: %x\n", eip);

	int brk_index = eip2brk(eip);
	if(brk_index < NBRK){
		cprintf("mon_breakpoint: You've set a break point at %x\n", eip);
		return 0;
	}
	else{
		brk_index = getfreebrk(eip);
		if(brk_index < NBRK){
			breakpoints[brk_index].eip = eip;
			breakpoints[brk_index].op = *((uint8_t *)eip);
			*((uint8_t *)eip) = 0xcc;
			cprintf("Set a breakpoint successfully at %x\n", eip);
			return 0;
		}
		else{
			cprintf("mon_breakpoint: You can just set %d breakpoints\n", brk_index);
			return 0;
		}
	}
}



/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}


