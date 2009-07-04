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
        TUCK        \  kflag count kflag
        -           \  kflag count-?
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
: CTELL COUNT TELL ;
: SPACES BEGIN DUP 0> WHILE SPACE 1- REPEAT DROP ;
: @++  ( addr -- addr+1 n )     DUP @ SWAP 1 CELLS + SWAP ;
: C@++ ( caddr -- caddr+1 n )   DUP C@ SWAP 1+ SWAP ;
: DUMP ( caddr len -- ) HEX >R BEGIN R@ 0> WHILE C@++ 3 U.R R> 1- >R REPEAT CR R> 2DROP DEC ;


\ decompiler!
: XT-NAME 9 CELLS - COUNT F_HIDDEN F_IMMED F_NOINTERP OR OR INVERT AND ;
: CCOUNT DUP 1 CELLS + SWAP @ ;
: '."' 46 EMIT 34 EMIT SPACE ;
: 'S"' [ CHAR S ] LITERAL EMIT 34 EMIT SPACE ;
: SEE
    BL WORD DUP FIND
    DUP 0= IF ." (not found)" CR 2DROP EXIT THEN \ bail out if the word is not found
    DUP DE>CFA @ DOCOL <> IF ." (native)" CR 2DROP EXIT THEN \ bail if its not a colon def
    ." :" SPACE SWAP COUNT TELL SPACE \ ": FOO "
    DUP 1 CELLS + C@
    DUP F_IMMED AND 0<> IF ." IMMEDIATE" SPACE THEN
    DUP F_NOINTERP AND 0<> IF ." NOINTERPRET" SPACE THEN
    DROP CR
    DE>DFA DUP >R BEGIN
        DUP @ DUP 2 PICK R@ < OR
    WHILE
        DUP [ ' LIT ] LITERAL = IF           
            TAB 40 EMIT SPACE XT-NAME TELL SPACE 41 EMIT 3 SPACES
            1 CELLS +
            DUP @ . CR
        ELSE
            DUP [ ' LITSTRING ] LITERAL = IF
                TAB 40 EMIT SPACE XT-NAME TELL SPACE
                1 CELLS +
                DUP @ 0 .R SPACE 41 EMIT 
                3 SPACES 'S"' DUP CCOUNT TUCK TELL 34 EMIT
                DUP ALIGNED /CELLS 3 SPACES 40 EMIT SPACE . ." cells " 41 EMIT CR
                ALIGNED +
            ELSE
                DUP [ ' 0BRANCH ] LITERAL = IF
                    TAB XT-NAME TELL SPACE
                    1 CELLS +
                    DUP @ DUP 0> IF
                        2DUP CELLS + 
                        DUP R@ > IF R> SWAP >R THEN
                        DROP
                    THEN
                    DROP
                    DUP @ . CR
                ELSE
                    DUP [ ' BRANCH ] LITERAL = IF
                        TAB XT-NAME TELL SPACE
                        1 CELLS +
                        DUP @ DUP 0> IF
                            2DUP CELLS + 
                            DUP R@ > IF R> SWAP >R THEN
                            DROP
                        THEN
                        DROP
                        DUP @ . CR
                    ELSE
                        DUP 0= IF
                            DROP \ EXIT
                            TAB ." EXIT" CR
                        ELSE
                            TAB XT-NAME TELL CR
                        THEN
                    THEN
                THEN
            THEN
        THEN    
        1 CELLS +
    REPEAT
    ." ;" CR
    R> 3 NDROP
;

: MARKER IMMEDIATE
    CREATE  ( -- dfa )
    DUP DFA>CFA DOCOL SWAP !                        \ set the code field to DOCOL
    DFA>DE  ( dfa -- de )
    DUP [ ' LIT ] LITERAL , ,                       \ compile literal DE address
        [ ' HERE ] LITERAL , [ ' ! ] LITERAL ,      \ compile HERE !
    @       ( de -- link )
        [ ' LIT ] LITERAL , ,                       \ compile literal link address
        [ ' LATEST ] LITERAL , [ ' ! ] LITERAL ,    \ compile LATEST !
    [ 0 ] LITERAL ,                                 \ compile EXIT
;




MARKER reset
