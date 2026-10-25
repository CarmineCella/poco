; prelude.lisp -- everyday definitions for vlisp, written in vlisp
;
; use:      (load "prelude.lisp")  at the top of a script, or  vl prelude.lisp script.lisp
;
; Everything here is a two- or three-liner over the primitives; nothing needs to be in C++.
; Functions on arrays are vectorized wherever the primitives allow it; the few that loop
; with for are marked "loops" and are fine for prototypes, slow for long signals.

; ---------------------------------------------------------------- constants
(define pi 3.141592653589793)
(define tau (* 2 pi))

; ---------------------------------------------------------------- logic
; not, and, or are procedures: both arguments are evaluated (use if for short-circuit)
(define (not x) (if x 0 1))
(define (and a b) (if a (if b 1 0) 0))
(define (or a b) (if a 1 (if b 1 0)))
(define (any x) (> (sum (!= x 0)) 0))              ; some element non-zero
(define (all x) (= (sum (!= x 0)) (len x)))        ; every element non-zero

; ---------------------------------------------------------------- lists
(define (map f l) (if (null? l) '() (cons (f (car l)) (map f (cdr l)))))
(define (for-each f l) (if (null? l) '() {(f (car l)) (for-each f (cdr l))}))
(define (fold f acc l) (if (null? l) acc (fold f (f acc (car l)) (cdr l))))
(define (filter p l)
  (if (null? l) '()
      (if (p (car l)) (cons (car l) (filter p (cdr l))) (filter p (cdr l)))))
(define (reverse l) (fold (lambda (acc x) (cons x acc)) '() l))
(define (append a b) (if (null? a) b (cons (car a) (append (cdr a) b))))
(define (nth l i) (if (= i 0) (car l) (nth (cdr l) (- i 1))))
(define (last l) (if (null? (cdr l)) (car l) (last (cdr l))))
(define (iota n) (array->list (range n)))
(define (zip a b) (if (or (null? a) (null? b)) '() (cons (list (car a) (car b)) (zip (cdr a) (cdr b)))))
(define (count p l) (fold (lambda (n x) (if (p x) (+ n 1) n)) 0 l))
(define (apply-to-array f x) (list->array (map f (array->list x))))   ; f on each element; loops

; ---------------------------------------------------------------- numbers
(define (mod x y) (- x (* y (floor (/ x y)))))       ; sign of y, like Python
(define (round x) (floor (+ x 0.5)))
(define (ceil x) (- (floor (- x))))
(define (sign x) (- (> x 0) (< x 0)))
(define (square x) (* x x))
(define (log10 x) (/ (log x) (log 10)))
(define (log2 x) (/ (log x) (log 2)))
(define (clip x lo hi) (+ (* x (and-mask (>= x lo) (<= x hi))) (* lo (< x lo)) (* hi (> x hi))))
(define (and-mask a b) (* a b))                      ; masks multiply
(define (minimum a b) (+ (* a (<= a b)) (* b (< b a))))   ; elementwise min of two arrays
(define (maximum a b) (+ (* a (>= a b)) (* b (> b a))))

; ---------------------------------------------------------------- arrays
(define (zeros n) (make n 0))
(define (ones n) (make n 1))
(define (linspace a b n) (+ a (* (range n) (/ (- b a) (- n 1)))))
(define (first x) (ref x 0))
(define (rest x) (slice x 1 (len x)))
(define (head x n) (slice x 0 n))
(define (tail x n) (slice x (- (len x) n) (len x)))
(define (reverse-array x)                           ; loops
  { (define y (make (len x) 0))
  (for (i (len x)) (set-ref! y i (ref x (- (len x) 1 i))))
  y })
(define (diff x) (- (rest x) (head x (- (len x) 1))))
(define (cumsum x)                                   ; loops
  { (define y (+ x 0))
  (for (i (- (len x) 1)) (set-ref! y (+ i 1) (+ (ref y i) (ref x (+ i 1)))))
  y })
(define (argmax x)                                   ; index of the first maximum; loops
  { (define m (max x))
  (define (go i) (if (= (ref x i) m) i (go (+ i 1))))
  (go 0) })
(define (argmin x) (argmax (- x)))
(define (dot a b) (sum (* a b)))
(define (norm x) (sqrt (dot x x)))
(define (mean x) (/ (sum x) (len x)))
(define (var x) (mean (square (- x (mean x)))))
(define (std x) (sqrt (var x)))
(define (rms x) (sqrt (mean (square x))))
(define (normalize x) (/ x (max (abs x))))
(define (pad x n) (cat x (make n 0)))                ; n zeros at the end
(define (repeat x n) (if (= n 0) [] (cat x (repeat x (- n 1)))))
(define (set-slice! y i x)                           ; y[i .. i+len(x)) = x, in place; loops
  { (for (k (len x)) (set-ref! y (+ i k) (ref x k)))
  y })
(define (map-array f x)                              ; f on each element with ref/set-ref!; loops
  { (define y (make (len x) 0))
  (for (i (len x)) (set-ref! y i (f (ref x i))))
  y })

; ---------------------------------------------------------------- signals
(define (time n sr) (/ (range n) sr))                ; n samples at rate sr, in seconds
(define (sine f t) (sin (* tau f t)))
(define (cosine f t) (cos (* tau f t)))
(define (phasor f n sr) (mod (* f (time n sr)) 1))   ; 0 .. 1 ramp at f Hz
(define (saw f n sr) (- (* 2 (phasor f n sr)) 1))
(define (pulse f n sr duty) (< (phasor f n sr) duty))
(define (noise n seed)                               ; Park-Miller generator, deterministic, seed > 0; loops
  { (define y (make n 0))
  (define s seed)
  (for (i n)
    { (set! s (mod (* 16807 s) 2147483647))
    (set-ref! y i (- (/ s 1073741823.5) 1)) })
  y })
(define (db x) (* 20 (log10 x)))
(define (undb x) (pow 10 (/ x 20)))
(define (mtof m) (* 440 (pow 2 (/ (- m 69) 12))))    ; MIDI note to Hz
(define (ftom f) (+ 69 (* 12 (log2 (/ f 440)))))
(define (hann n) (* 0.5 (- 1 (cos (/ (* tau (range n)) n)))))
(define (hamming n) (- 0.54 (* 0.46 (cos (/ (* tau (range n)) n)))))
(define (blackman n)
  { (define k (/ (* tau (range n)) n))
  (+ (- 0.42 (* 0.5 (cos k))) (* 0.08 (cos (* 2 k)))) })
(define (ramp n) (linspace 0 1 n))
(define (fade-in x n) (* x (cat (ramp n) (ones (- (len x) n)))))
(define (fade-out x n) (* x (cat (ones (- (len x) n)) (reverse-array (ramp n)))))
(define (envelope x n) (fade-out (fade-in x n) n))
(define (delay x n) (cat (make n 0) (head x (- (len x) n))))   ; shift right by n samples
(define (moving-average x n)                         ; vectorized: a sum of shifted copies
  { (define acc (make (len x) 0))
  (for (k n) (set! acc (+ acc (cshift x (- k)))))
  (/ acc n) })
(define (convolve x h)                               ; direct convolution; loops over h only
  { (define y (make (+ (len x) (len h) -1) 0))
  (for (k (len h)) (set! y (+ y (* (ref h k) (cat (make k 0) (pad x (- (len h) 1 k)))))))
  y })
(define (frames x n hop)                             ; list of windowed frames, first frame first
  { (define (go i acc)
    (if (> (+ i n) (len x)) (reverse acc) (go (+ i hop) (cons (* (hann n) (slice x i (+ i n))) acc))))
  (go 0 '()) })
(define (overlap-add fs n hop)                       ; inverse of frames (up to the window); loops
  { (define y (make (+ (* hop (- (len fs) 1)) n) 0))
  (define (go i l) (if (null? l) y {(set-slice! y i (+ (slice y i (+ i n)) (car l))) (go (+ i hop) (cdr l))}))
  (go 0 fs) })
(define (energy x) (dot x x))
(define (zero-crossings x) (sum (< (* (head x (- (len x) 1)) (rest x)) 0)))
(define (peak x) (max (abs x)))

; ---------------------------------------------------------------- misc
(define (assert c msg) (if c '() (display "assertion failed:" msg)))
(define (show name x) { (display name "=" x) x })
(define (compose f g) (lambda (x) (f (g x))))
(define (loaded) (display "prelude loaded:" (len (env)) "names"))
