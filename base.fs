: CR 10 EMIT ;
: TAB 9 EMIT ;
: SPACE 32 EMIT ;
: CMP 2DUP > ROT < - ;
: BIN 2 BASE ! ;
: DEC 10 BASE ! ;
: HEX 16 BASE ! ;
: . 0 .R SPACE ;
: U. 0 U.R SPACE ;
: IF IMMEDIATE [ ' LIT , ] 0BRANCH , HERE @ 0 , ;
: THEN IMMEDIATE DUP HERE @ SWAP - 4 / SWAP ! ;
: ELSE IMMEDIATE [ ' LIT , ] BRANCH , HERE @ 0 , SWAP DUP HERE @ SWAP - 4 / SWAP ! ;
: <=> 2DUP < IF -1 ELSE > IF 1 ELSE 0 THEN THEN ;
