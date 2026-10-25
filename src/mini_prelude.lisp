; mini_prelude.lisp -- the primitives mini.h left out, written in mini itself
; use:  mini mini_prelude.lisp script.scm
;
; mini has: + - * / (binary)  < =  sqrt exp log floor sin cos  range (slice x start n) cat len sum max ref
;           car cdr cons list display  and the forms define lambda if do {} set! quote
; arrays are immutable: everything below builds new arrays with range, slice, cat and arithmetic

; ---------------------------------------------------------------- constants
(define pi 3.141592653589793)
(define tau (* 2 pi))

; ---------------------------------------------------------------- logic and comparison
(define not (lambda (x) (if x 0 1)))
(define and (lambda (a b) (if a (if b 1 0) 0)))
(define or (lambda (a b) (if a 1 (if b 1 0))))
(define > (lambda (a b) (< b a)))
(define <= (lambda (a b) (- 1 (< b a))))
(define >= (lambda (a b) (- 1 (< a b))))
(define != (lambda (a b) (- 1 (= a b))))
(define any (lambda (x) (< 0 (sum (!= x 0)))))
(define all (lambda (x) (= (sum (!= x 0)) (len x))))
(define equal? (lambda (a b)                         ; arrays: same size and elements
  (if (= (len a) (len b)) (if (= (len a) 0) 1 (all (= a b))) 0)))

; ---------------------------------------------------------------- numbers
(define neg (lambda (x) (- 0 x)))
(define sign (lambda (x) (- (< 0 x) (< x 0))))
(define abs (lambda (x) (* x (sign x))))
(define min (lambda (x) (neg (max (neg x)))))
(define minimum (lambda (a b) (+ (* a (<= a b)) (* b (< b a)))))   ; elementwise
(define maximum (lambda (a b) (+ (* a (>= a b)) (* b (> b a)))))
(define pow (lambda (x y) (exp (* y (log x)))))      ; positive base
(define square (lambda (x) (* x x)))
(define tan (lambda (x) (/ (sin x) (cos x))))
(define log2 (lambda (x) (/ (log x) (log 2))))
(define log10 (lambda (x) (/ (log x) (log 10))))
(define mod (lambda (x y) (- x (* y (floor (/ x y))))))
(define round (lambda (x) (floor (+ x 0.5))))
(define ceil (lambda (x) (neg (floor (neg x)))))
(define clip (lambda (x lo hi) (minimum (maximum x lo) hi)))

; ---------------------------------------------------------------- lists
(define null? (lambda (l) (= (len l) 0)))
(define map (lambda (f l) (if (null? l) '() (cons (f (car l)) (map f (cdr l))))))
(define for-each (lambda (f l) (if (null? l) '() { (f (car l)) (for-each f (cdr l)) })))
(define fold (lambda (f acc l) (if (null? l) acc (fold f (f acc (car l)) (cdr l)))))
(define filter (lambda (p l)
  (if (null? l) '() (if (p (car l)) (cons (car l) (filter p (cdr l))) (filter p (cdr l))))))
(define reverse (lambda (l) (fold (lambda (acc x) (cons x acc)) '() l)))
(define append (lambda (a b) (if (null? a) b (cons (car a) (append (cdr a) b)))))
(define nth (lambda (l i) (if (= i 0) (car l) (nth (cdr l) (- i 1)))))
(define last (lambda (l) (if (null? (cdr l)) (car l) (last (cdr l)))))
(define iota (lambda (n) (array->list (range n))))
(define array->list (lambda (x) {
  (define go (lambda (i) (if (= i (len x)) '() (cons (ref x i) (go (+ i 1))))))
  (go 0) }))
(define list->array (lambda (l) (fold (lambda (acc v) (cat acc v)) [] l)))   ; a list of scalars

; ---------------------------------------------------------------- arrays
(define make (lambda (n v) (+ v (* 0 (range n)))))
(define zeros (lambda (n) (make n 0)))
(define ones (lambda (n) (make n 1)))
(define linspace (lambda (a b n) (+ a (* (range n) (/ (- b a) (- n 1))))))
(define first (lambda (x) (ref x 0)))
(define rest (lambda (x) (slice x 1 (- (len x) 1))))
(define head (lambda (x n) (slice x 0 n)))
(define tail (lambda (x n) (slice x (- (len x) n) n)))
(define cshift (lambda (x k)                          ; circular shift left by k, k may be negative
  (if (null? x) x { (define j (mod k (len x))) (cat (slice x j (- (len x) j)) (slice x 0 j)) })))
(define delay (lambda (x n) (cat (zeros n) (head x (- (len x) n)))))   ; shift right, zero filled
(define reverse-array (lambda (x) (list->array (reverse (array->list x)))))   ; loops
(define diff (lambda (x) (- (rest x) (head x (- (len x) 1)))))
(define cumsum (lambda (x)                            ; loops
  (if (null? x) x (cat (first x) (+ (first x) (cumsum (rest x)))))))
(define argmax (lambda (x) {
  (define m (max x))
  (define go (lambda (i) (if (= (ref x i) m) i (go (+ i 1)))))
  (go 0) }))
(define argmin (lambda (x) (argmax (neg x))))
(define dot (lambda (a b) (sum (* a b))))
(define norm (lambda (x) (sqrt (dot x x))))
(define mean (lambda (x) (/ (sum x) (len x))))
(define var (lambda (x) (mean (square (- x (mean x))))))
(define std (lambda (x) (sqrt (var x))))
(define rms (lambda (x) (sqrt (mean (square x)))))
(define normalize (lambda (x) (/ x (max (abs x)))))
(define pad (lambda (x n) (cat x (zeros n))))
(define repeat (lambda (x n) (if (= n 0) [] (cat x (repeat x (- n 1))))))
(define map-array (lambda (f x) (list->array (map f (array->list x)))))   ; f on each element; loops

; ---------------------------------------------------------------- signals
(define time (lambda (n sr) (/ (range n) sr)))
(define sine (lambda (f t) (sin (* (* tau f) t))))
(define cosine (lambda (f t) (cos (* (* tau f) t))))
(define phasor (lambda (f n sr) (mod (* f (time n sr)) 1)))
(define saw (lambda (f n sr) (- (* 2 (phasor f n sr)) 1)))
(define pulse (lambda (f n sr duty) (< (phasor f n sr) duty)))
(define db (lambda (x) (* 20 (log10 x))))
(define undb (lambda (x) (pow 10 (/ x 20))))
(define mtof (lambda (m) (* 440 (pow 2 (/ (- m 69) 12)))))
(define ftom (lambda (f) (+ 69 (* 12 (log2 (/ f 440))))))
(define hann (lambda (n) (* 0.5 (- 1 (cos (/ (* tau (range n)) n))))))
(define hamming (lambda (n) (- 0.54 (* 0.46 (cos (/ (* tau (range n)) n))))))
(define ramp (lambda (n) (linspace 0 1 n)))
(define fade-in (lambda (x n) (* x (cat (ramp n) (ones (- (len x) n))))))
(define fade-out (lambda (x n) (* x (cat (ones (- (len x) n)) (reverse-array (ramp n))))))
(define envelope (lambda (x n) (fade-out (fade-in x n) n)))
(define moving-average (lambda (x n) {                ; vectorized: a sum of shifted copies
  (define go (lambda (k acc) (if (= k n) (/ acc n) (go (+ k 1) (+ acc (cshift x (neg k)))))))
  (go 0 (zeros (len x))) }))
(define convolve (lambda (x h) {                      ; direct convolution; loops over h only
  (define m (- (len h) 1))
  (define term (lambda (k) (* (ref h k) (cat (zeros k) (pad x (- m k))))))
  (define go (lambda (k acc) (if (< k (len h)) (go (+ k 1) (+ acc (term k))) acc)))
  (go 0 (zeros (+ (len x) m))) }))
(define frames (lambda (x n hop) {                    ; list of windowed frames, first frame first
  (define go (lambda (i acc)
    (if (< (len x) (+ i n)) (reverse acc) (go (+ i hop) (cons (* (hann n) (slice x i n)) acc)))))
  (go 0 '()) }))
(define energy (lambda (x) (dot x x)))
(define zero-crossings (lambda (x) (sum (< (* (head x (- (len x) 1)) (rest x)) 0))))
(define peak (lambda (x) (max (abs x))))

; ---------------------------------------------------------------- misc
(define assert (lambda (c msg) (if c '() (display 'assertion-failed: msg))))
(define show (lambda (name x) { (display name '= x) x }))
(define compose (lambda (f g) (lambda (x) (f (g x)))))
