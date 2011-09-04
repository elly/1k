; brainfuck codegen

(module codegen scheme
	(provide codegen)
	(require "op.scm")
	(require "pcodegen.scm")

  (define (codegen-one op)
		(case (op-type op)
			((add) (pcodegen-add op))
			((sub) (pcodegen-sub op))
			((addp) (pcodegen-addp op))
			((subp) (pcodegen-subp op))
			((read) (pcodegen-read op))
			((write) (pcodegen-write op))
			((jz) (pcodegen-jz op))
			((jnz) (pcodegen-jnz op))
			((label) (pcodegen-label op))
			(else (error "Unknown op type?"))))

	(define (codegen ops)
		(append
			(pcodegen-prefix ops)
			(apply append (map codegen-one ops))
			(pcodegen-suffix ops)))
)
