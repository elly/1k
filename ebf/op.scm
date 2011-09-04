; brainfuck opcodes

(module op scheme
	(provide op-type
					 op-target
					 make-op)

	(define-struct op (type target)))
