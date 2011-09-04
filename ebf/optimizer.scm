(module optimizer scheme
	(provide optimize)
	(require "op.scm")

  (define (op-rec il op1 op2 func)
		(if (null? il)
			null
			(if (null? (cdr il))
				il
				(if (and (symbol=? (op-type (first il)) op1)
								 (symbol=? (op-type (second il)) op2))
					(op-rec
						(cons (func (first il) (second il))
									(list-tail il 2))
						op1 op2 func)
					(cons (car il) (op-rec (cdr il) op1 op2 func))))))

	(define (single-rec il f)
		(if (null? il)
			null
			(let ((v (f (car il))))
				(if v
					(cons v (single-rec (cdr il) f))
					(single-rec (cdr il) f)))))

  (define (try-add-fold il)
		(op-rec il 'add 'add
			(lambda (o1 o2)
				(make-op 'add (+ (op-target o1)
												 (op-target o2))))))

	(define (try-sub-fold il)
		(op-rec il 'sub 'sub
			(lambda (o1 o2)
				(make-op 'sub (+ (op-target o1)
												 (op-target o2))))))

	(define (try-addp-fold il)
		(op-rec il 'addp 'addp
			(lambda (o1 o2)
				(make-op 'addp (+ (op-target o1)
													(op-target o2))))))

	(define (try-subp-fold il)
		(op-rec il 'subp 'subp
			(lambda (o1 o2)
				(make-op 'subp (+ (op-target o1)
													(op-target o2))))))

	(define (try-addsub-fold il)
		(op-rec il 'add 'sub
			(lambda (o1 o2)
				(let ((o1v (op-target o1))
							(o2v (op-target o2)))
					(if (> o1v o2v)
						(make-op 'add (- o1v o2v))
						(make-op 'sub (- o2v o1v)))))))

	(define (try-subadd-fold il)
		(op-rec il 'sub 'add
			(lambda (o1 o2)
				(let ((o1v (op-target o1))
							(o2v (op-target o2)))
					(if (> o1v o2v)
						(make-op 'sub (- o2v o1v))
						(make-op 'add (- o1v o2v)))))))

	(define (try-nullop il)
		(single-rec il
			(lambda (op)
				(if (and
							(or
								(symbol=? (op-type op) 'add)
								(symbol=? (op-type op) 'sub)
								(symbol=? (op-type op) 'addp)
								(symbol=? (op-type op) 'subp))
							(= (op-target op) 0))
					#f
					op))))

	(define (optimize il)
		(let* ((il (try-add-fold il))
					 (il (try-sub-fold il))
					 (il (try-addp-fold il))
					 (il (try-subp-fold il))
					 (il (try-addsub-fold il))
					 (il (try-subadd-fold il))
					 (il (try-nullop il)))
			il))
)
