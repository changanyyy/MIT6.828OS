// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//


static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	pte_t pte = uvpt[PGNUM(addr)];

	//cprintf("%e\n",err);
	if( (err&FEC_WR) == 0 && (pte & PTE_COW)){
		cprintf("%x\n",addr);
		panic("pgfault: it's not a write err\n");
	}


	
	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	
	//cprintf("%x\n", ENVX(sys_getenvid()));
	addr = ROUNDDOWN(addr, PGSIZE);

	//allocate a new page, and map it at PFTEMP
	if(sys_page_alloc(0, (void *)PFTEMP, PTE_P|PTE_W|PTE_U) < 0){
		panic("sys_page_alloc: alloc failed\n");
	}

	//move the content at virtual address addr to PFTEMP
	memmove((void *)PFTEMP, addr, PGSIZE);
	
	//remap the page at PFTEMP at new virtual address addr
	//Here was a small bug troubling me for a long time, which I ignored PTE_W !!!
	if(sys_page_map(0, (void *)PFTEMP, 0, (void *)addr, PTE_W|PTE_U|PTE_P) < 0){
		panic("map failed\n");
	}

	//unmap the virtual address PFTEMP
	if(sys_page_unmap(0, (void *)PFTEMP) < 0){
		panic("failed\n");
	}

}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//


static int
duppage(envid_t envid, unsigned pn)
{
	int r;
	struct Env *dstenv = (struct Env *)envs + ENVX(envid);
	void *addr = (void *)(pn * PGSIZE);
	//the pte is certainly existing, because fork will check it.
	pte_t pte = uvpt[pn];
	// LAB 4: Your code here.


	if((pte&PTE_COW) || (pte&PTE_W)){
		
		if((r = sys_page_map(0, addr, dstenv->env_id, addr, PTE_P|PTE_U|PTE_COW)) < 0){
			cprintf("sys_page_map: %e\n", r);
		}
		if((r = sys_page_map(0, addr, 0, addr, PTE_P|PTE_U|PTE_COW)) < 0){
			cprintf("sys_page_map: %e\n", r);
		}
	}
	else{
		sys_page_map(0,addr, dstenv->env_id, addr, PTE_U|PTE_P);
	}

	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	envid_t envid;
	uint8_t *addr;
	int r;
	uint32_t pn;

	set_pgfault_handler(pgfault);

	envid = sys_exofork();
	
	//sys_exfork returns from child 
	if (envid == 0) {
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	// operate through all the page below UTOP
	for (addr = 0; (uint32_t)addr < USTACKTOP; addr += PGSIZE){
		pn = PGNUM(addr);
		if( (PTE_P & uvpd[PDX(addr)]) && (PTE_P&uvpt[pn])){		
			duppage(envid, pn);
		}
	}

	// allocate a new page for UXSTACKTOP
	sys_page_alloc(envid, (void *)(UXSTACKTOP - PGSIZE), PTE_W|PTE_U|PTE_P);

	if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0){
		panic("sys_env_set_status: %e", r);
	}

	return envid;

}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
