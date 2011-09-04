(module tokenizer scheme
	(provide tokenize

					 token-type
					 print-token)
	(require "op.scm")

	(define-struct token (type))
	(define (print-token t)
		(format "~a" (token-type t)))

	(define (tokenize-char c)
		(case c
			((#\>) (make-token 'incp))
			((#\<) (make-token 'decp))
			((#\+) (make-token 'inc))
			((#\-) (make-token 'dec))
			((#\.) (make-token 'write))
			((#\,) (make-token 'read))
			((#\[) (make-token 'loop-begin))
			((#\]) (make-token 'loop-end))
			(else #f)))

	(define (tokenize p)
		(let ((c (read-char p)))
			(if (eof-object? c)
				null
				(let ((t (tokenize-char c)))
					(if t
						(cons t (tokenize p))
						(tokenize p))))))
)
