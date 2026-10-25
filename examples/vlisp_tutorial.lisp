; vlisp_tutorial.lisp -- a tour of vlisp
;
; vlisp is a small Scheme in which every number is an array of doubles.
; A scalar is an array of size one, so 3 and [3] are the same value.
; Run this file with   vl vlisp_tutorial.lisp   and read along.

(display "--- 1. values")

; numbers, arrays, strings, symbols, lists
(display 3 -2.5 1e3)                 ; scalars
(display [1 2 3] [] [0.5 -1])        ; array literals: scalars only, no commas
(display "a string")                 ; display prints strings without quotes
(display 'symbol)                    ; a quoted symbol
(display '(1 "two" three (4)))       ; a quoted list can hold anything
(display (list 1 2 3))               ; list builds a list from evaluated arguments
(display (type [1]) (type '()) (type "s") (type 'a) (type +) (type (lambda (x) x)))

(display "--- 2. arithmetic broadcasts")

; operators work element by element; a scalar stretches to the array's size
(display (+ [1 2 3] 1))              ; [2 3 4]
(display (* [1 2 3] [10 20 30]))     ; [10 40 90]
(display (- 10 [1 2 3]))             ; [9 8 7]
(display (+ 1 2 3 4))                ; + - * / take any number of arguments
(display (- 5) (/ 4))                ; and are unary negation and reciprocal with one
(display (pow [2 3] 2) (sqrt [4 9]) (abs [-1 2]) (floor [1.7 -1.2]))
(display (sin 0) (cos 0) (exp 1) (log 1) (tan 0) (atan 1))
(display (sum [1 2 3]) (min [3 1 2]) (max [3 1 2]) (len [1 2 3]))

; comparisons return masks of 0 and 1
(display (< [1 5 3] 3))              ; [1 0 0]
(display (= [1 2] [1 3]))            ; [1 0]
(display (* [1 5 3] (< [1 5 3] 3)))  ; masks multiply: keep what is below 3

(display "--- 3. making arrays")

(display (make 4 0))                 ; (make n value)
(display (range 5))                  ; 0 .. n-1
(display (range 2 5))                ; start .. end-1
(display (range 0 1 0.25))           ; start, end excluded, step
(display (range 5 0 -1))             ; steps can be negative
(display (slice (range 10) 2 5))     ; (slice x start end), end excluded
(display (slice (range 10) 0 10 3))  ; optional step
(display (cat [1 2] [3 4]))          ; concatenation
(display (cshift [1 2 3 4] 1))       ; circular shift, negative goes the other way
(display (ref [10 20 30] 1))         ; element access, 0-based

(display "--- 4. mutation and sharing")

(define a [1 2 3])
(set-ref! a 0 99)                    ; set-ref! changes the array in place
(display a)
(define b a)                         ; b is the same array, not a copy
(set-ref! b 1 0)
(display a)                          ; [99 0 3]
(define c (+ a 0))                   ; any arithmetic produces a new array
(set-ref! c 2 -1)
(display a c)                        ; a is untouched
; literals in a program are never changed: each evaluation gives a fresh array
(define (three) [1 2 3])
(set-ref! (three) 0 100)
(display (three))                    ; still [1 2 3]

(display "--- 5. definitions and functions")

(define sr 100)                                    ; a variable
(define square (lambda (x) (* x x)))               ; a procedure
(define (cube x) (* x x x))                        ; the same, shorter
(display (square [1 2 3]) (cube 2))
(define (stats x)                                  ; a body is one expression: {} sequences several, the last is the value
  { (define m (/ (sum x) (len x)))
  (define v (/ (sum (pow (- x m) 2)) (len x)))
  (list m (sqrt v)) })
(display (stats [2 4 4 4 5 5 7 9]))                ; mean and standard deviation
(set! sr 200)                                      ; set! changes an existing binding
(display sr)
(display square)                                   ; procedures print as their source

; closures capture their environment
(define (make-counter)
  { (define n 0)
  (lambda () { (set! n (+ n 1)) n }) })
(define tick (make-counter))
(tick) (tick)
(display (tick))                                   ; 3

; there is no let: a lambda applied at once does the same
(display ((lambda (p q) (+ (* p p) (* q q))) 3 4))

(display "--- 6. control")

(display (if (> 2 1) "yes" "no"))
(display (if 0 "yes"))               ; no else branch: ()
; an array is true when it is non-empty and all its elements are non-zero
(display (if [1 2] "all" "some") (if [1 0] "all" "some") (if [] "non-empty" "empty"))
(display (do (display "first") (display "second") "last"))
(display {(display "braces") "are do"})           ; {e1 e2 ...} reads as (do e1 e2 ...)
(display (if (> 3 2) {(display "then") 1} {(display "else") 2}))

; recursion; tail calls run in constant stack
(define (fact n) (if (< n 2) 1 (* n (fact (- n 1)))))
(display (fact 10))
(define (count-up i acc) (if (= i 0) acc (count-up (- i 1) (+ acc 1))))
(display (count-up 100000 0))

; for binds i = 0 .. n-1 in a child frame; use it for per-sample loops
(define y (make 5 0))
(for (i (len y)) (set-ref! y i (* i i)))
(display y)

(display "--- 7. lists")

(define l '(1 2 3))
(display (car l) (cdr l) (cons 0 l) (null? l) (null? '()) (len l))
(define (map f l) (if (null? l) '() (cons (f (car l)) (map f (cdr l)))))
(define (fold f acc l) (if (null? l) acc (fold f (f acc (car l)) (cdr l))))
(display (map square l) (fold + 0 l))
(display (apply + l))                              ; apply spreads a list as arguments
(display (array->list [1 2]) (list->array '(3 4)))
(display (list [1 2 3] [4 5]))                     ; a list of arrays: frames, channels, ...

(display "--- 8. equality")

(display (equal? [1 2] [1 2]) (equal? [1 2] [1 2 3]) (equal? 'a 'a) (equal? "a" 'a))
(display (equal? '(1 (2 [3])) '(1 (2 [3]))) (equal? square square) (equal? square cube))

(display "--- 9. files")

; save writes values in printed form, one per line, exactly; read gives them back as a list
(define signal (sin (* 2 3.141592653589793 5 (/ (range 8) 100))))
(save "tutorial_tmp.vl" signal '(1 "two" three) square)
(define back (read "tutorial_tmp.vl"))
(display (len back) (equal? (car back) signal))
; load evaluates a file and returns its last value; a saved definition loads back as a definition
(save "tutorial_tmp.vl" (list 'define 'twice (lambda (x) (* 2 x))) '(twice 21))
(display (load "tutorial_tmp.vl") (twice 4))
; load looks in the current folder, then in ~/.vlisp/ -- the place for your own libraries
; a file of bare numbers, as written by other tools, reads as a list of scalars:
(save "tutorial_tmp.vl" 1 2 3)
(display (list->array (read "tutorial_tmp.vl")))

(display "--- 10. a little signal processing")

(define sr 1000)
(define t (/ (range sr) sr))                             ; one second of time
(define (sine f) (sin (* 2 3.141592653589793 f t)))
(define x (+ (sine 5) (* 0.5 (sine 12))))                ; two partials
(define (rms x) (sqrt (/ (sum (* x x)) (len x))))
(display "rms" (rms x))
(define (hann n) (* 0.5 (- 1 (cos (* 2 3.141592653589793 (/ (range n) n))))))
(display "window" (slice (hann 8) 0 8))
(define (moving-average x n)                              ; vectorized: a sum of shifted copies
  { (define acc (make (len x) 0))
  (for (k n) (set! acc (+ acc (cshift x k))))
  (/ acc n) })
(display "smoothed" (slice (moving-average x 10) 0 4))
(define (frames x n hop)                                  ; a list of windowed frames
  { (define (go i acc)
    (if (> (+ i n) (len x))
        acc
        (go (+ i hop) (cons (* (hann n) (slice x i (+ i n))) acc))))
  (go 0 '()) })
(display "frames" (len (frames x 256 128)))
(define (fir x h)                                         ; per-sample loop: fine for prototypes, slow for real work
  { (define y (make (len x) 0))
  (for (i (len x))
    (for (k (len h))
      (if (>= (- i k) 0)
          (set-ref! y i (+ (ref y i) (* (ref h k) (ref x (- i k))))))))
  y })
(display "fir" (slice (fir [1 0 0 1 0] [0.5 0.5]) 0 5))
(save "tutorial_tmp.vl" x)                                ; keep the result

(display "--- 11. introspection and errors")

(display (len (env)))                 ; every name visible here
(define (f u) (car (env)))
(display (f 1))                       ; u: the innermost frame comes first
; errors stop the file and print the expression where they happened, with file:line when loaded:
;   error: vlisp_tutorial.lisp:NN: undefined variable
;       in undefined-thing
; uncomment the next line to see it
; (display undefined-thing)
(display "done")
