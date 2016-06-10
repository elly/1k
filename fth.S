/* fth.S */
/* x86-64: args rdi rsi rdx rcx r8 r9, saved rsp rbp r12-r15 */
/* linux syscall: nr rax, args rdi rsi rdx r10 r8 r9 */
/* osx syscall: nr | 0x200_0000 rax, args rdi rsi rdx r10 r8 19 */
/* r12 = dstack, r13 = rstack */

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

initregs:
	lea dstacktop(%rip), %r12
	lea rstacktop(%rip), %r13
	ret

initmem:
	lea arena(%rip), %rax
	mov %rax, here(%rip)
	ret

/* void addword(const char *name, int flags) */
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
	or %rsi, %rax
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

foobar: .asciz "foobar"
quxbaz: .asciz "quxbaz"
omglolz: .asciz "omglolz"
initdict:
	mov $0, %rsi
	xor %rsi, %rsi
	lea foobar(%rip), %rdi
	call addword
	xor %rsi, %rsi
	lea quxbaz(%rip), %rdi
	call addword
	ret

.globl _start
_start:
	call initregs
	call initmem
	call initdict
	lea foobar(%rip), %rdi
	call getword
	int3
	lea quxbaz(%rip), %rdi
	call getword
	int3
	lea omglolz(%rip), %rdi
	call getword
	int3
	call exit
	ret

.globl start
start:
	jmp _start
