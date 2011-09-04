(require "tokenizer.scm")
(require "muncher.scm")
(require "codegen.scm")
(require "optimizer.scm")

(define (read-contents inp)
	(let ((l (read-line inp)))
		(if (eof-object? l)
			""
			(string-append l (read-contents inp)))))

(define (compile infname outfname)
	(let ((inp (open-input-file infname))
				(outp (open-output-file outfname #:exists 'replace)))
		(let ((code (codegen (optimize (munch (tokenize inp))))))
			(for-each
				(lambda (s) (write-string (string-append s "\n") outp))
			code))))
