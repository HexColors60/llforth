: if ' 0branch , here@ 0 , ; immediate
: else ' branch , here@ 0 , swap here swap ! ; immediate
: then here swap ! ; immediate

: begin here ; immediate
: until ' 0branch , , ; immediate

: 0> 0 > ;
: 0< 0 < ;
: 0= 0 = ;
: 1+ 1 + ;

: 2dup over over ;
: 2drop drop drop ;

: while ' 0branch , here@ 0 , ; immediate
: repeat ' branch , here 1+ swap ! , ; immediate
: leave ' branch , here@ swap 0 , ; immediate

: do here ' >r , ' >r , ; immediate
: loop ' r> , ' r> , ' 1+ , ' 2dup , ' = , ' 0branch , , ' 2drop , ; immediate
: +loop ' r> , ' r> , ' rot , ' + , ' 2dup , ' = , ' 0branch , , ' 2drop , ; immediate

: ."
    state @
    0branch .interpreting

    inbuf word
    inbuf strcpy
    ' lit , ,
    ' prints ,
    exit

.interpreting:
    inbuf word
    drop
    inbuf prints

; immediate

: main

.start:
    inbuf word
    0branch .empty
    inbuf find
    dup
    0branch .number

    state @
    0branch .interpreting

    flag@
    0branch .compiling

.interpreting:
    execute
    branch .start

.compiling:
    ,
    branch .start

.number:
    drop
    inbuf number

    state @
    0branch .start

    lit lit , ,
    branch .start

.empty:
    inbuf@ 10 <>
    0branch .enter
    inbuf@ -1 <>
    0branch .end
    branch .start

.enter:
    cr
    branch .start

.end:
    bye

;