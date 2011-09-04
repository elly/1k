; x86 codegen

(module pcodegen scheme
	(provide pcodegen-add
					 pcodegen-sub
					 pcodegen-addp
					 pcodegen-subp
					 pcodegen-read
					 pcodegen-write
					 pcodegen-jz
					 pcodegen-jnz
					 pcodegen-label
					 
					 pcodegen-prefix
					 pcodegen-suffix)
	(require "op.scm")

  (define (pcodegen-prefix ops)
		(list
			".global bf_main"
			"bf_main:"
			"pushq %rbx"
			"mov %rdi, %rbx"))

	(define (pcodegen-suffix ops)
		(list
			"popq %rbx"
			"ret"))

	(define (pcodegen-add op)
		(list (format "addq $~a, (%rbx)" (op-target op))))

	(define (pcodegen-sub op)
		(list (format "subq $~a, (%rbx)" (op-target op))))

	(define (pcodegen-addp op)
		(list (format "addq $~a, %rbx" (* 8 (op-target op)))))

	(define (pcodegen-subp op)
		(list (format "subq $~a, %rbx" (* 8 (op-target op)))))

	(define (pcodegen-read op)
		(list "call bfrt_read"
					"mov %al, (%rbx)"))

	(define (pcodegen-write op)
		(list "movzxb (%rbx), %rdi"
					"call bfrt_write"))

	(define (pcodegen-jz op)
		(list "movzx (%rbx), %rax"
					"test %rax, %rax"
					(format "jz ~a" (op-target op))))

	(define (pcodegen-jnz op)
		(list "movzx (%rbx), %rax"
					"test %rax, %rax"
					(format "jnz ~a" (op-target op))))

	(define (pcodegen-label op)
		(list (format "~a:" (op-target op))))
)
