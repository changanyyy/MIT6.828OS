#ifndef JOS_KERN_MONITOR_H
#define JOS_KERN_MONITOR_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

struct Trapframe;

// Activate the kernel monitor,
// optionally providing a trap frame indicating the current state
// (NULL if none).
void monitor(struct Trapframe *tf);

// Functions implementing monitor commands.
int mon_help(int argc, char **argv, struct Trapframe *tf);
int mon_kerninfo(int argc, char **argv, struct Trapframe *tf);
int mon_backtrace(int argc, char **argv, struct Trapframe *tf);
int mon_showmappings(int argc, char **argv, struct Trapframe *tf);
int mon_setpermission(int argc, char **argv, struct Trapframe *tf);
int mon_continue(int argc, char **argv, struct Trapframe *tf);
int mon_si(int argc, char **argv, struct Trapframe *tf);
int mon_breakpoint(int argc, char **argv, struct Trapframe *tf);


int eip2brk(uint32_t eip);
int getfreebrk(uint32_t eip);


struct Breakpoint{
    uint32_t eip;
    uint32_t eflags;
    uint8_t op;
};

#define NBRK 20

int num_of_breakpoints;
struct Breakpoint breakpoints[NBRK];



#endif	// !JOS_KERN_MONITOR_H
