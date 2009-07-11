: \ IMMEDIATE   10 WORD DROP ;
: CR    10 EMIT ;
: TAB   9 EMIT ;
: SPACE 32 EMIT ;
: CMP   2DUP > ROT < - ;
: BIN   2 BASE ! ;
: DEC   10 BASE ! ;
: HEX   16 BASE ! ;
: .     0 .R SPACE ;
: U.    0 U.R SPACE ;
: LITERAL IMMEDIATE COMPILE-ONLY    [ ' LIT , ] LIT , , ;
: IF IMMEDIATE COMPILE-ONLY     POSTPONE 0BRANCH HERE @ >CTRL 0 , ;
: THEN IMMEDIATE COMPILE-ONLY   CTRL> HERE @ OVER - /CELLS SWAP ! ; 
: ELSE IMMEDIATE COMPILE-ONLY 
    POSTPONE BRANCH CTRL> HERE @ 0 , >CTRL HERE @ OVER - /CELLS SWAP ! 
; 
: <=>   2DUP < IF -1 ELSE > IF 1 ELSE 0 THEN THEN ;
: CHAR  32 WORD 1+ C@ ;
: BEGIN IMMEDIATE COMPILE-ONLY HERE @ >CTRL ;
: UNTIL IMMEDIATE COMPILE-ONLY POSTPONE 0BRANCH CTRL> HERE @ - /CELLS , ;
: AGAIN IMMEDIATE COMPILE-ONLY POSTPONE BRANCH CTRL> HERE @ - /CELLS , ;
: WHILE IMMEDIATE COMPILE-ONLY POSTPONE 0BRANCH HERE @ >CTRL 0 , ;
: REPEAT IMMEDIATE COMPILE-ONLY
    POSTPONE BRANCH 2CTRL> SWAP HERE @ - /CELLS , HERE @ OVER - /CELLS SWAP !
;
: RECURSE IMMEDIATE COMPILE-ONLY    LATEST @ DE>CFA , ;
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
: ALIGNED   3 + 3 INVERT AND ;
: ALIGN     HERE @ ALIGNED HERE ! ;
: C,        HERE @ C! 1 HERE +! ;                    
: ." IMMEDIATE COMPILE-ONLY
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
: VARIABLE  CREATE 1 CELLS ALLOT ;
: CONSTANT  CREATE DFA>CFA DOCON SWAP !  , ;
: EXIT IMMEDIATE    COMPILE-ONLY 0 , ;
DEC 32 CONSTANT BL
: COUNT DUP 1+ SWAP C@ ;
: CTELL COUNT TELL ;
: SPACES    BEGIN DUP 0> WHILE SPACE 1- REPEAT DROP ;
: @++  ( addr -- addr+1 n )     DUP @ SWAP 1 CELLS + SWAP ;
: C@++ ( caddr -- caddr+1 n )   DUP C@ SWAP 1+ SWAP ;
: DUMP ( caddr len -- ) HEX >R BEGIN R@ 0> WHILE C@++ 3 U.R R> 1- >R REPEAT CR R> 2DROP DEC ;
: VALUE     CREATE DFA>CFA DOVAL SWAP ! , ; 
: TO IMMEDIATE
    BL WORD FIND DE>DFA
    DUP DFA>CFA @ DOVAL <> IF ." Not a value!" CR EXIT THEN \ FIXME make this sane
    STATE S_COMPILE = IF
        POSTPONE LIT    
        ,
        POSTPONE !
    ELSE
        !
    THEN
;



\ decompiler!
: XT-NAME   9 CELLS - COUNT F_HIDDEN F_IMMED F_COMPONLY OR OR INVERT AND ;
: CCOUNT    DUP 1 CELLS + SWAP @ ;
: '."'      46 EMIT 34 EMIT SPACE ;
: 'S"'      [ CHAR S ] LITERAL EMIT 34 EMIT SPACE ;
: SEE
    BL WORD DUP FIND
    DUP 0= IF ." (not found)" CR 2DROP EXIT THEN \ bail out if the word is not found
    DUP DE>CFA @ DOCOL <> IF ." (native)" CR 2DROP EXIT THEN \ bail if its not a colon def
    ." :" SPACE SWAP COUNT TELL SPACE \ ": FOO "
    DUP 1 CELLS + C@
    DUP F_IMMED AND 0<> IF ." IMMEDIATE" SPACE THEN
    DUP F_COMPONLY AND 0<> IF ." COMPILE-ONLY" SPACE THEN
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

: MARKER ( FIXME should be flagged "no compile" )
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
