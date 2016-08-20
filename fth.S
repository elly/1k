/* fth.S */
/* x86-64: args rdi rsi rdx rcx r8 r9, saved rsp rbp r12-r15 */
/* linux syscall: nr rax, args rdi rsi rdx r10 r8 r9 */
/* osx syscall: nr | 0x200_0000 rax, args rdi rsi rdx r10 r8 19 */
/* rsp = dstack, r13 = rstack, r14 = fth ip */

.bss
here: .quad 0
dict: .quad 0
dstack: .space 1048576, 0
dstacktop:
rstack: .space 1048576, 0
rstacktop:
arena: .space 1048576, 0

.data

/* Entries: 3b flags, 5b namelen, nb name, 64b next, 64b interp, nb rest */

.text

/* void exit(int code) */
exit:
	/* linux: 0x3c */
	mov $0x2000001, %rax
	syscall

/* int strlen(char *str) */
strlen:
	xor %rax, %rax
1:
	mov (%rdi), %cl
	test %cl, %cl
	jz 2f
	inc %rax
	inc %rdi
	jmp 1b
2:
	ret

/* void memzero(char *buf, int len) */
memzero:
	test %rsi, %rsi
	jz 1f
	xor %rax, %rax
	mov %al, (%rdi)
	inc %rdi
	dec %rsi
	jmp memzero
1:
	ret

/* void memcpy(char *dest, char *src, int len) */
memcpy:
	test %rdx, %rdx
	jz 1f
	mov (%rsi), %cl
	mov %cl, (%rdi)
	inc %rdi
	inc %rsi
	dec %rdx
	jmp memcpy
1:
	ret

/* void memeq(char *a, char *b, int len) */
memeq:
	test %rdx, %rdx
	jz 1f
	mov (%rsi), %cl
	cmp (%rdi), %cl
	jne 2f
	inc %rdi
	inc %rsi
	dec %rdx
	jmp memeq
1:
	mov $0x1, %rax
	ret
2:
	xor %rax, %rax
	ret

/* void appendq(long qword) */
appendq:
	push %r12
	mov here(%rip), %r12
	mov %rdi, (%r12)
	add $0x8, %r12
	mov %r12, here(%rip)
	pop %r12
	ret

initregs:
	lea dstacktop(%rip), %r12
	lea rstacktop(%rip), %r13
	ret

initmem:
	lea arena(%rip), %rax
	mov %rax, here(%rip)
	ret

/* void addword(const char *name) */
addword:
	mov here(%rip), %rcx
	mov %rcx, %r14

	/* len */
	push %rdi
	push %rcx
	call strlen
	pop %rcx
	pop %rdi
	and $0x1f, %rax
	mov %rax, %r8
	mov %al, (%rcx)
	inc %rcx

	/* name */
	push %rcx
	push %rdi
	mov $0x1f, %rsi
	mov %rcx, %rdi
	call memzero
	pop %rdi
	pop %rcx

	mov %r8, %rdx
	mov %rdi, %rsi
	mov %rcx, %rdi
	push %rcx
	call memcpy
	pop %rcx
	add $0x1f, %rcx

	/* link ptr */
	mov dict(%rip), %rdi
	mov %rdi, (%rcx)
	add $0x8, %rcx
	mov %r14, dict(%rip)

	/* done */
	mov %rcx, here(%rip)
	mov %rcx, %r15
	ret

/* void* getword(const char *name, int fl) */
getword:
	push %rdi
	push %rsi
	call strlen
	pop %rsi
	pop %rdi
	mov %rax, %rcx

	mov dict(%rip), %rdx
1:
	test %rdx, %rdx
	jz 2f
	xor %rax, %rax
	mov (%rdx), %al
	and $0x1f, %al
	cmp %rax, %rcx
	jne 6f
	push %rdi
	push %rcx
	push %rdx
	inc %rdx
4:
	test %rcx, %rcx
	jz 5f
	mov (%rdx), %al
	cmp (%rdi), %al
	jne 3f
	inc %rdx
	inc %rdi
	dec %rcx
	jmp 4b
3:
	pop %rdx
	pop %rcx
	pop %rdi
6:
	mov 0x20(%rdx), %rdx
	jmp 1b
5:
	pop %rdx
	pop %rcx
	pop %rdi
2:
	mov %rdx, %rax
	ret

/* void* de2code(void* de) */
de2code:
	lea 0x28(%rdi), %rax
	ret

/* void* de2args(void* de) */
de2args:
	lea 0x30(%rdi), %rax
	ret

fthnext:
	mov (%r14), %rax
	add $0x8, %r14
	jmp *(%rax)

pushfip:
	sub $0x8, %r13
	mov %rdi, (%r13)
	ret

popfip:
	mov (%r13), %rax
	add $0x8, %r13
	ret

docol:
	call pushfip
	add $0x8, %rax
	mov %rax, %r14
	jmp fthnext

/* To run a fth function:
 * 1) Find the codeword for it
 * 2) Save the current fth ip on the rstack
 * 3) Set the fth ip to its first data word
 * 4) Jump to its codeword
 *
 * To exit it:
 * 1) Restore the old fth ip
 * 2) Use fthnext as though we were continuing execution there
 */

dupname: .asciz "dup"
dupbody:
	mov (%rsp), %rax
	push %rax
	jmp fthnext

initdict:
	lea dupname(%rip), %rdi
	call addword
	lea dupbody(%rip), %rdi
	call appendq

	ret

/* Next steps here:
 * 1) Build QUIT, LIT, and * as assembly primitives
 * 2) Define SQUARE as a fth word that is DUP *
 * 3) Define another word (foobar) that does 4 SQUARE or smth
 * 4) Hack up enough of INTERPRET to run FOOBAR and see if 16 ends up on the
 *    dstack
 */

.globl _start
_start:
	call initregs
	call initmem
	call initdict
	push $0x12345678
	lea dupname(%rip), %rdi
	call getword
	mov %rax, %rdi
	call de2code
	int3
	jmp *(%rax)
	call exit
	ret

.globl start
start:
	jmp _start
