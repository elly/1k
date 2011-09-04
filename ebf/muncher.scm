(module muncher scheme
	(provide munch)
	(require "tokenizer.scm")
	(require "op.scm")

  (define *max-label* 0)
	(define (make-label)
		(let ((n *max-label*))
			(set! *max-label* (+ n 1))
			(format "L~a" n)))

  (define (munch-normal type val)
		(lambda (tl block)
				(cons (make-op type val)
						(munch-ctx (cdr tl) block))))

  (define munch-inc (munch-normal 'add 1))
	(define munch-dec (munch-normal 'sub 1))
	(define munch-incp (munch-normal 'addp 1))
	(define munch-decp (munch-normal 'subp 1))
	(define munch-read (munch-normal 'read #f))
	(define munch-write (munch-normal 'write #f))

  (define (munch-loop-begin tl ctxs)
		(let* ((begin-label (make-label))
					 (end-label (make-label))
					 (nctx (cons begin-label end-label)))
			(append
				(list (make-op 'jz end-label))
				(list (make-op 'label begin-label))
				(munch-ctx (cdr tl) (cons nctx ctxs)))))

	(define (munch-loop-end tl ctxs)
		(let* ((begin-label (caar ctxs))
					 (end-label (cdar ctxs)))
			(append
				(list (make-op 'jnz begin-label))
				(list (make-op 'label end-label))
				(munch-ctx (cdr tl) (cdr ctxs)))))

  (define (munch-ctx tl block)
		(if (null? tl)
			null
			(case (token-type (car tl))
				((inc) (munch-inc tl block))
				((dec) (munch-dec tl block))
				((incp) (munch-incp tl block))
				((decp) (munch-decp tl block))
				((read) (munch-read tl block))
				((write) (munch-write tl block))
				((loop-begin) (munch-loop-begin tl block))
				((loop-end) (munch-loop-end tl block)))))
				

	(define (munch tl) (munch-ctx tl #f))
)
