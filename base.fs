: \ IMMEDIATE 10 WORD DROP ;
: CR 10 EMIT ;
: TAB 9 EMIT ;
: SPACE 32 EMIT ;
: CMP 2DUP > ROT < - ;
: BIN 2 BASE ! ;
: DEC 10 BASE ! ;
: HEX 16 BASE ! ;
: . 0 .R SPACE ;
: U. 0 U.R SPACE ;
: LITERAL IMMEDIATE NOINTERPRET [ ' LIT , ] LIT , , ;
: IF IMMEDIATE NOINTERPRET [ ' 0BRANCH ] LITERAL , HERE @ 0 , ;
: THEN IMMEDIATE NOINTERPRET DUP HERE @ SWAP - /CELLS SWAP ! ;
: ELSE IMMEDIATE NOINTERPRET 
    [ ' BRANCH ] LITERAL , HERE @ 0 , SWAP DUP HERE @ SWAP - /CELLS SWAP ! 
;
: <=> 2DUP < IF -1 ELSE > IF 1 ELSE 0 THEN THEN ;
: CHAR 32 WORD 1+ C@ ;
: BEGIN IMMEDIATE NOINTERPRET HERE @ ;
: UNTIL IMMEDIATE NOINTERPRET [ ' 0BRANCH ] LITERAL , HERE @ - /CELLS , ;
: AGAIN IMMEDIATE NOINTERPRET [ ' BRANCH ] LITERAL , HERE @ - /CELLS , ;
: WHILE IMMEDIATE NOINTERPRET [ ' 0BRANCH ] LITERAL , HERE @ 0 , ;
: REPEAT IMMEDIATE NOINTERPRET 
    [ ' BRANCH ] LITERAL , SWAP HERE @ - /CELLS , DUP HERE @ SWAP - /CELLS SWAP ! 
;
: ( IMMEDIATE
    DEC 1           \  count
    BEGIN           \  
        KEY         \  count key
        DUP         \  count key key
        40          \  count key key '('
        =           \  count key lflag
        -ROT        \  key lflag count
        +           \  key count+?
        SWAP        \  count key
        41          \  count key ')'
        =           \  count kflag
\         DUP         \  count kflag kflag
\         -ROT        \  kflag kflag count
\         SWAP        \  kflag count kflag
        TUCK        \  kflag count kflag
        -           \  kflag count-?
\         SWAP        \  count kflag
\         OVER        \  count kflag count
        TUCK        \  count kflag count
        0=          \  count kflag cflag
        +           \  count kcflag
        2           \  count kcflag 2
        =           \  count loopflag
    UNTIL
    DROP            \ drop count
;
: ALIGNED 3 + 3 INVERT AND ;
: ALIGN HERE @ ALIGNED HERE ! ;
: C, HERE @ C! 1 HERE +! ;                    
: ." IMMEDIATE NOINTERPRET
    [ ' LITSTRING ] LITERAL ,
    HERE @ 0 ,
    BEGIN
        34 KEY DUP ROT <>
    WHILE
        C,
        DUP 1 SWAP +!
    REPEAT
    2DROP
    ALIGN
    [ ' TELL ] LITERAL ,
;
: VARIABLE CREATE 1 CELLS ALLOT ;
: CONSTANT CREATE DFA>CFA DOCON SWAP !  , ;
: EXIT IMMEDIATE NOINTERPRET 0 , ;
DEC 32 CONSTANT BL
: COUNT DUP 1+ SWAP C@ ;
