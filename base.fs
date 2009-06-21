: CR 10 EMIT ;
: TAB 9 EMIT ;
: SPACE 32 EMIT ;
: CMP 2DUP > ROT < - ;
: BIN 2 BASE ! ;
: DEC 10 BASE ! ;
: HEX 16 BASE ! ;
: . 0 .R SPACE ;
: U. 0 U.R SPACE ;
