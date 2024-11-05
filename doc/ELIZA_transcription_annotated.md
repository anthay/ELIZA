

```code




        CHANGE   MAD
            EXTERNAL FUNCTION (KEY,MYTRAN)                                      000010
            NORMAL MODE IS INTEGER                                              000020
            ENTRY TO CHANGE.                                                    000030
            LIST.(INPUT)                                                        000040
            V'S G(1)=$TYPE$,$SUBST$,$APPEND$,$ADD$,                             000050
           1$START$,$RANK$,$DISPLA$                                             000060
            V'S SNUMB = $ I3 *$                                                 000070
            FIT=0                                                               000080
CHANGE      PRINT COMMENT $PLEASE INSTRUCT ME$                                  001400
            LISTRD.(MTLIST.(INPUT),0)                                           001410
            JOB=POPTOP.(INPUT)                                                  001420
            T'H IDENT, FOR J=1,1, J.G. 7                                        001430
IDENT       W'R G(J) .E. JOB, T'O THEMA                                         001440
            PRINT COMMENT $CHANGE NOT RECOGNIZED$                               001450
            T'O CHANGE                                                          001460
THEMA       W'R J .E. 5, F'N IRALST.(INPUT)                                     001470
            W'R J .E. 7                                                         001480
                T'H DISPLA, FOR I=0,1, I  .G. 32                                001490
                W'R LISTMT.(KEY(I)) .E. 0, T'O DISPLA                           001500
                S=SEQRDR.(KEY(I))                                               001510
READ(7)         NEXT=SEQLR.(S,F)                                                001520
                W'R F .G. 0, T'O DISPLA                                         001530
                PRINT COMMENT $*$                                               001540
                TPRINT.(NEXT,0)                                                 001550
                PRINT FORMAT SNUMB,I                                            001560
                PRINT COMMENT $ $                                               001570
                T'O READ(7)                                                     001580
DISPLA          CONTINUE                                                        001590
                PRINT COMMENT $ $                                               001600
                PRINT COMMENT $MEMORY LIST FOLLOWS$                             001610
                PRINT COMMENT $ $                                               001620
                T'H MEMLST, FOR I=1 , 1, I .G. 4                                001630
MEMLST          TXTPRT.(MYTRAN(I),0)                                            001640
                T'O CHANGE                                                      001650
            E'L                                                                 001660
            THEME=POPTOP.(INPUT)                                                001670
            SUBJECT=KEY(HASH.(THEME,5))                                         001680
            S=SEQRDR.(SUBJECT)                                                  001690
LOOK        TERM=SEQLR.(S,F)                                                    001700
            W'R F .G. 0, T'O FAIL                                               001710
            W'R TOP.(TERM) .E. THEME, T'O FOUND                                 001720
            T'O LOOK                                                            001730
FOUND       T'O DELTA(J)                                                        001740
DELTA(1)    TPRINT.(TERM,0)                                                     001750
            T'O CHANGE                                                          001760
FAIL        PRINT COMMENT $LIST NOT FOUND$                                      001770
            T'O CHANGE                                                          001780
DELTA(2)    S=SEQRDR.(TERM)                                                     001790
            OLD=POPTOP.(INPUT)                                                  001800
READ(1)     OBJCT=SEQLR.(S,F)                                                   001810
            W'R F .G. 0, T'O FAIL                                               001820
            W'R F .NE. 0, T'O READ(1)                                           001830
            INSIDE=SEQRDR.(OBJECT)                                              001840
READ(2)     IT=SEQLR.(INSIDE,F)                                                 001850
            W'R F .G. 0, T'O READ(1)                                            001860
            SIT=SEQRDR.(IT)                                                     001870
            SOLD=SEQRDR.(OLD)                                                   001880
ITOLD       TOLD=SEQLR.(SOLD,FOLD)                                              001890
            DIT=SEQLR.(SIT,FIT)                                                 001900
            W'R TOLD .E. DIT .AND. FOLD .LE. 0,T'O ITOLD                        001910
            W'R FOLD .G. 0, T'O OK(J)                                           001920
            T'O READ(2)                                                         001930
OK(2)       SUBST.(POPTOP.(INPUT),LSPNTR.(INSIDE))                              001940
            T'O CHANGE                                                          001950
OK(3)       NEWBOT.(POPTOP.(INPUT),OBJCT)                                       001960
            T'O CHANGE                                                          001970
DELTA(3)    T'O DELTA(2)                                                        001980
DELTA(4)    W'R NAMTST.(BOT.(TERM)) .E. 0                                       001990
                BOTTOM=POPBOT.(TERM)                                            002000
                NEWBOT.(POPTOP.(INPUT),TERM)                                    002010
                NEWBOT.(BOTTOM,TERM)                                            002020
            O'E                                                                 002030
                NEWBOT.(POPTOP.(INPUT),TERM)                                    002040
            E'L                                                                 002050
            T'O CHANGE                                                          002060
DELTA(6)    S=SEQRDR.(TERM)                                                     002070
READ(6)     OBJCT=SEQLR.(S,F)                                                   002080
            W'R F .G. 0, T'O FAIL                                               002090
            W'R F .NE. 0, T'O READ(6)                                           002100
            OBJCT=SEQLL.(S,F)                                                   002110
            W'R LNKL.(OBJCT) .E. 0                                              002120
                SUBST.(POPTOP.(INPUT),LSPNTR.(S))                               002130
            O'E                                                                 002140
                NEWTOP.(POPTOP.(INPUT),LSPNTR.(S))                              002150
            E'L                                                                 002160
            T'O CHANGE                                                          002170
           R* * * * * * * * * * END OF MODIFICATION ROUTINE                     002180
            E'N                                                                 002200
```


### TPRINT.(LST)

```code
        TPRINT  MAD
            EXTERNAL FUNCTION (LST)                                             000010
            NORMAL MODE IS INTEGER                                              000020
            ENTRY TO TPRINT.                                                    000030
            SA=SEQRDR.(LST)                                                     000040
            LIST.(OUT)                                                          000050
READ        NEXT=SEQLR.(SA,FA)                                                  000060
            W'R FA .G. 0, T'O P                                                 000070
            W'R FA .E. 0, T'O B                                                 000080
            POINT=NEWBOT.(NEXT,OUT)                                             000100
            W'R SA .L. 0, MRKNEG.(POINT)                                        000110
            T'O READ                                                            000120
B           TXTPRT.(OUT,0)                                                      000130
            SEQLL.(SA,FA)                                                       000140
MORE        NEXT=SEQLR.(SA,FA)                                                  000150
            W'R TOP.(NEXT) .E. $=$                                              000160
                TXTPRT.(NEXT,0)                                                 000170
                T'O MORE                                                        000180
            E'L                                                                 000190
            W'R FA .G. 0, T'O DONE                                              000200
            PRINT COMMENT $ $                                                   000210
            SB=SEQRDR.(NEXT)                                                    000220
MEHR        TERM=SEQLR.(SB,FB)                                                  000230
            W'R FB .L.0                                                         000240
                PRINT ON LINE FORMAT NUMBER, TERM                               000250
                V'S NUMBER = $I3 *$                                             000260
                T'O MEHR                                                        000270
            E'L                                                                 000280
            W'R FB .G. 0, T'O MORE                                              000290
            TXTPRT.(TERM,0)                                                     000300
            T'O MEHR                                                            000310
P           TXTPRT.(OUT,0)                                                      000320
DONE        IRALST.(OUT)                                                        000330
            F'N                                                                 000340
            E'N                                                                 000350
```

The TPRINT.(LST) function prints the given script rule list to the terminal.

- **TPRINT/000060...000120** This loop copies the given LST to a temporary list called OUT.
   The loop terminates at the end of the list (FA > 0 on line TPRINT/000070) or at the first sublist (FA = 0 on line TPRINT/000080).

  - **TPRINT/000110** While copying from LST to OUT, the sign of the cells is also copied. (Recall that the sign bit is used to
    indicate that the word in the current cell is continued in the next cell.)

- **TPRINT/000130** All of the cells in LST prior to the first sublist have been copied to OUT. Print OUT to the terminal.

- **TPRINT/000140** Move the reader back to the first sublist encountered in LST.

- **TPRINT/000150** NEXT is the next transformation rule. Recall that a transformation rule is a list containing a decomposition
   sublist, followed by one or more reassembly sublists. There is also a special-case transformation rule that consists
   of only (= keyword).

- **TPRINT/000160** Is this a special-case (= keyword) form? If so, just print it (TPRINT/000170) and loop back to MORE to print the next
   transformation rule (there shouldn’t be any because (= keyword) always succeeds).

- **TPRINT/000200** If at the end of the list of transformation rules goto DONE.

- **TPRINT/000210** Print a blank line

- **TPRINT/000220** Create a reader to enumerate the sublists in the current transformation rule.

- **TPRINT/000230** TERM is the next cell in the transformation rule.

- **TPRINT/000240** If TERM is a datum it represents the index of the reassembly rule to use next. (ELIZA uses reassembly rules
   in turn. It keeps track of which reassembly rule to use next by inserting an index into the in-memory representation
   of the script.) If TERM is the index, just print it as a decimal integer (TPRINT/000250) and loop back to MEHR.

- **TPRINT/000290** If at the end of the list of reassembly rules goto MORE.

- **TPRINT/000300** Print the current decomposition or reassembly rule.

- **TPRINT/000320** Print the copy of the script rule. In this case the script rule had no associated transformation rules. E.g. (YOURSELF = MYSELF)

- **TPRINT/000330** Return the cells in the OUT list to the list of free cells.

F’N = FUNCTION RETURN; E’N = END OF FUNCTION

Note that line TPRINT/00090 is missing, presumed removed.


### LPRINT.(LST,TAPE)

```code
        LPRINT  MAD
            EXTERNAL FUNCTION (LST,TAPE)                                        006340
            NORMAL MODE IS INTEGER                                              006350
            ENTRY TO LPRINT.                                                    006360
            BLANK = $      $                                                    006370
            EXECUTE PLACE.(TAPE,0)                                              006380
            LEFTP = 606074606060K                                               006390
            RIGHTP= 606034606060K                                               006400
            BOTH  = 607460603460K                                               006410
            EXECUTE NEWTOP.(SEQRDR.(LST),LIST.(STACK))                          006420
            S=POPTOP.(STACK)                                                    006430
BEGIN       EXECUTE PLACE.(LEFTP,1)                                             006440
NEXT        WORD=SEQLR.(S,FLAG)                                                 006450
            W'R FLAG .L. 0                                                      006460
            EXECUTE PLACE.(WORD,1)                                              006470
            W'R S .G. 0, PLACE.(BLANK,1)                                        006480
            T'O NEXT                                                            006490
            OR W'R FLAG .G. 0                                                   006500
            EXECUTE PLACE.(RIGHTP,1)                                            006510
            W'R LISTMT.(STACK) .E. 0, T'O DONE                                  006520
            S=POPTOP.(STACK)                                                    006530
            T'O NEXT                                                            006540
            OTHERWISE                                                           006550
            W'R LISTMT.(WORD) .E. 0                                             006560
            EXECUTE PLACE.(BOTH,1)                                              006570
            T'O NEXT                                                            006580
            OTHERWISE                                                           006590
            EXECUTE NEWTOP.(S,STACK)                                            006600
            S=SEQRDR.(WORD)                                                     006610
            T'O BEGIN                                                           006620
            E'L                                                                 006630
            E'L                                                                 006640
DONE        EXECUTE PLACE.(0,-1)                                                006650
            EXECUTE IRALST.(STACK)                                              006660
            FUNCTION RETURN LST                                                 006670
            END OF FUNCTION                                                     006680
        TESTS   MAD
            EXTERNAL FUNCTION(CAND,S)                                           000010
            NORMAL MODE IS INTEGER                                              000020
            DIMENSION FIRST(5),SECOND(5)                                        000030
            ENTRY TO TESTS.                                                     000040
            STORE=S                                                             000050
            READER=SEQRDR.(CAND)                                                000060
            T'H ONE, FOR I=0,1, I .G. 100                                       000070
            FIRST(I)=SEQLR.(READER,FR)                                          000080
ONE         W'R READER .G. 0, T'O ENDONE                                        000090
ENDONE      SEQLL.(S,F)                                                         000100
            T'H TWO, FOR J=0,1, J .G. 100                                       000110
            SECOND(J)=SEQLR.(S,F)                                               000120
TWO         W'R S .G. 0, T'O ENDTWO                                             000130
ENDTWO      W'R I .NE. J, F'N 0                                                 000140
            T'H LOOK, FOR K=0,1, K.G. J                                         000150
LOOK        W'R FIRST(K) .NE. SECOND(K), F'N 0                                  000170
            EQL=SEQLR.(READER,FR)                                               000180
            W'R EQL .NE. $=$                                                    000190
            SEQLL.(READER,FR)                                                   000200
            F'N READER                                                          000210
            O'E                                                                 000220
            POINT=LNKL.(STORE)                                                  000230
            T'H DELETE , FOR K=0,1, K .G. J                                     000240
            REMOVE.(LSPNTR.(STORE))                                             000250
DELETE      SEQLR.(STORE,F)                                                     000260
INSRT       NEW=SEQLR.(READER,FR)                                               000270
            POINT=NEWTOP.(NEW,POINT)                                            000280
            MRKNEG.(POINT)                                                      000290
            W'R READER .L. 0, T'O INSRT                                         000300
            MRKPOS.(POINT)                                                      000310
            F'N READER                                                          000320
            E'L                                                                 000330
            E'N                                                                 000340
        DOCBCD  MAD
            EXTERNAL FUNCTION (A,B)                                             000010
            NORMAL MODE IS INTEGER                                              000020
            ENTRY TO FRBCD.                                                     000030
            W'R LNKL.(A) .E. 0, T'O NUMBER                                      000040
            B=A                                                                 000050
            F'N 0                                                               000060
NUMBER      K=A*262144                                                          000070
            B=BCDIT.(K)                                                         000080
            F'N 0                                                               000090
            E'N                                                                 000100
```

# ELIZA

This is the top level function that implements ELIZA’s behavior. Broadly, this involves

1. Read the script file. This specifies the keywords and text patterns to be used. It also specifies the opening message.
2. Print the opening message. (e.g. “...PLEASE TELL ME YOUR PROBLEM”)
3. Wait for the user to type something.
4. Look for keywords in the user’s input. If a keyword has a substitute word specified in the script,
   the word is immediately swapped for the substitute.
6. Select a keyword and generate the response. One of the keywords found in the user input is selected.
   The text pattern matching rules associated with that keyword are used to first break the input text into parts,
   then reassemble those parts into a response message.
8. Print the response and then go back to step 3 to await the next user input.

```code
        ELIZA   MAD
            NORMAL MODE IS INTEGER                                              000010
            DIMENSION KEY(32),MYTRAN(4)                                         000020
            INITAS.(0)                                                          000030
            PRINT COMMENT $WHICH SCRIPT DO YOU WISH TO PLAY$                    000060
            READ FORMAT SNUMB,SCRIPT                                            000070
            LIST.(TEST)                                                         000080
            LIST.(INPUT)                                                        000090
            LIST.(OUTPUT)                                                       000100
            LIST.(JUNK)                                                         000110
            LIMIT=1                                                             000120
            LSSCPY.(TREAD.(INPUT,SCRIPT),JUNK)                                  000130
            MTLIST.(INPUT)                                                      000140
            T'H MLST, FOR I=1,1, I .G. 4                                        000150
MLST        LIST.(MYTRAN(I))                                                    000160
            MINE=0                                                              000170
            LIST.(MYLIST)                                                       000180
            T'H KEYLST, FOR I=0,1, I .G. 32                                     000220
KEYLST      LIST.(KEY(I))                                                       000230
```
First declare and initialize some variables.

- **ELIZA/000020** declare two arrays, KEY and MYTRAN:
  - KEY(0) ... KEY(31) is used as a hashmap of keywords and their associated transformation rules.
     KEY(32) is the special case "NONE" transformation rule.
  - MYTRAN(1) ... MYTRAN(4) is used as a hashmap for the four MEMORY rules. (MYTRAN(0) is unused.)

- **ELIZA/000030** INITAS “[...] forms the list of available space from all core storage not otherwise used.
   This must be the first executable statement in all programs using the SLIP functions.”

- **ELIZA/000070** SNUMB is the format specification and is declared on line 410 to be “I3 * ”; this allows the user
   to enter a decimal integer of up to three digits. The number the user enters is stored in the variable SCRIPT.
   When this version of ELIZA starts it asks the user “WHICH SCRIPT DO YOU WISH TO PLAY”. The user must enter
   the id of the tape unit where the ELIZA script has been mounted.

- **ELIZA/000080...000110** declare four variables that will be used as lists.

- **ELIZA/000120** declare LIMIT to be an integer variable with the initial value 1.

- **ELIZA/000130** call the SLIP function TREAD to read the text in the first list in the script (from the tape unit
   with the id specified by the user above). The text is stored in the list called INPUT and then copied to the list
   called JUNK. (The first list in a script is the message ELIZA is to print at the start of a conversation,
   e.g. “HOW DO YOU DO.  PLEASE TELL ME YOUR PROBLEM.”)

- **ELIZA/000140** delete the cells in the INPUT list.

- **ELIZA/000220...000230** initialize KEY(0) ... KEY(32) as lists.

The effect of line ELIZA/000130 is to read the opening remarks text from the script into the list variable called JUNK.
The name is a bit odd, but once the opening remarks have been printed (line ELIZA/000290) the list storage is recovered (line ELIZA/000300),
and the JUNK variable is reused later for some other purpose – it’s a temporary scratch pad. Reusing variables is a little
frowned upon today but would have been common practice when memory was tight. An IBM 7094, used by Weizenbaum to develop ELIZA,
had 32,768 36-bit words of core memory (RAM).
'm not sure why the text is read into INPUT and then copied to JUNK instead of being read straight into JUNK.

Note that lines ELIZA/000040, ELIZA/000050, ELIZA/000190, ELIZA/000200 and ELIZA/000210 are not present in the listing.
Also, there is no ENTRY TO ELIZA statement. Every other function has an ENTRY TO <function name> statement.


```code
           R* * * * * * * * * * READ NEW SCRIPT                                 000240
BEGIN       MTLIST.(INPUT)                                                      000250
            NODLST.(INPUT)                                                      000260
            LISTRD.(INPUT,SCRIPT)                                               000270
            W'R LISTMT.(INPUT) .E. 0                                            000280
                TXTPRT.(JUNK,0)                                                 000290
                MTLIST.(JUNK)                                                   000300
                T'O START                                                       000310
            E'L                                                                 000320
            W'R TOP.(INPUT) .E. $NONE$                                          000330
                NEWTOP.(LSSCPY.(INPUT,LIST.(9)),KEY(32))                        000340
                T'O BEGIN                                                       000350
               OR W'R TOP.(INPUT) .E. $MEMORY$                                  000360
                POPTOP.(INPUT)                                                  000370
                MEMORY=POPTOP.(INPUT)                                           000380
                T'H MEM, FOR I=1,1, I .G. 4                                     000390
MEM             LSSCPY.(POPTOP.(INPUT),MYTRAN(I))                               000400
                T'O BEGIN                                                       000410
               O'E                                                              000420
                NEWBOT.(LSSCPY.(INPUT,LIST.(9)),KEY(HASH.                       000430
           1    (TOP.(INPUT),5)))                                               000440
                T'O BEGIN                                                       000450
            E'L                                                                 000460
           R* * * * * * * * * * BEGIN MAJOR LOOP                                000470
START       TREAD.(MTLIST.(INPUT),0)                                            000480
            KEYWRD=0                                                            000490
            PREDNC=0                                                            000500
            LIMIT=LIMIT+1                                                       000510
            W'R LIMIT .E. 5, LIMIT=1                                            000520
            W'R LISTMT.(INPUT) .E. 0, T'O ENDPLA                                000530
            IT=0                                                                000540
            W'R TOP.(INPUT) .E. $+$                                             000550
                CHANGE.(KEY,MYTRAN)                                             000560
                T'O START                                                       000570
            E'L                                                                 000580
            W'R TOP.(INPUT) .E. $*$, T'O NEWLST                                 000590
            S=SEQRDR.(INPUT)                                                    000600
NOTYET      W'R S .L. 0                                                         000610
                SEQLR.(S,F)                                                     000620
                T'O NOTYET                                                      000630
               O'E                                                              000640
                WORD=SEQLR.(S,F)                                                000650
                W'R WORD .E. $.$ .OR. WORD .E. $,$ .OR. WORD .E. $BUT$          000660
                    W'R IT .E. 0                                                000670
                        NULSTL.(INPUT,LSPNTR.(S),JUNK)                          000680
                        MTLIST.(JUNK)                                           000690
                        T'O NOTYET                                              000700
                       O'E                                                      000710
                        NULSTR.(INPUT,LSPNTR.(S),JUNK)                          000720
                        MTLIST.(JUNK)                                           000730
                        T'O ENDTXT                                              000740
                       E'L                                                      000750
                    E'L                                                         000760
                E'L                                                             000770
                W'R F .G. 0, T'O ENDTXT                                         000780
                I=HASH.(WORD,5)                                                 000790
                SCANER=SEQRDR.(KEY(I))                                          000800
                SF=0                                                            000810
                T'H SEARCH, FOR J=0,0, SF .G. 0                                 000820
                CAND= SEQLR.(SCANER,SF)                                         000830
                W'R SF .G. 0, T'O NOTYET                                        000840
SEARCH          W'R TOP.(CAND) .E. WORD, T'O KEYFND                             000850
KEYFND          READER=TESTS.(CAND,S)                                           000860
                W'R READER .E. 0, T'O NOTYET                                    000870
                W'R LSTNAM.(CAND) .NE. 0                                        000880
                    DL=LSTNAM.(CAND)                                            000890
SEQ                 W'R S .L. 0                                                 000900
                        SEQLR.(S,F)                                             000910
                        T'O SEQ                                                 000920
                       O'E                                                      000930
                        NEWTOP.(DL,LSPNTR.(S))                                  000940
                    E'L                                                         000950
                   O'E                                                          000960
                E'L                                                             000970
                NEXT=SEQLR.(READER,FR)                                          000980
                W'R FR .G. 0, T'O NOTYET                                        000990
                W'R IT .E. 0 .AND. FR .E. 0                                     001000
PLCKEY              IT=READER                                                   001010
                    KEYWRD=WORD                                                 001020
                   OR W'R FR .L. 0 .AND. NEXT .G. PREDNC                        001030
                    PREDNC=NEXT                                                 001040
                    NEXT=SEQLR.(READER,FR)                                      001050
                    T'O PLCKEY                                                  001060
                   0'E                                                          001070
                    T'O NOTYET                                                  001080
                E'L                                                             001090
                T'O NOTYET                                                      001100
               R* * * * * * * * * * END OF MAJOR LOOP                           001110
ENDTXT          W'R IT .E. 0                                                    001120
                    W'R LIMIT .E. 4 .AND. LISTMT.(MYLIST) .NE. 0                001130
                        OUT=POPTOP.(MYLIST)                                     001140
                        TXTPRT.(OUT,0)                                          001150
                        IRALST.(OUT)                                            001160
                        T'O START                                               001170
                       O'E                                                      001180
                        ES=BOT.(TOP.(KEY(32)))                                  001190
                        T'O TRY                                                 001200
                    E'L                                                         001210
                   OR W'R KEYWRD .E. MEMORY                                     001220
                    I=HASH.(BOT.(INPUT),2)+1                                    001230
                    NEWBOT.(REGEL.(MYTRAN(I),INPUT,LIST.(MINE)),MYLIST)         001240
                    SEQLL.(IT,FR)                                               001250
                    T'O MATCH                                                   001260
                   O'E                                                          001270
                    SEQLL.(IT,FR)                                               001280
               R* * * * * * * * * * MATCHING ROUTINE                            001290
MATCH               ES=SEQLR.(IT,FR)                                            001300
                    W'R TOP.(ES) .E. $=$                                        001310
                        S=SEQRDR.(ES)                                           001320
                        SEQLR.(S,F)                                             001330
                        WORD=SEQLR.(S,F)                                        001340
                        I=HASH.(WORD,5)                                         001350
                        SCANER=SEQRDR.(KEY(I))                                  001360
SCAN                    ITS=SEQLR.(SCANER,F)                                    001370
                        W'R F .G. 0, T'O NOMATCH(LIMIT)                         001380
                        W'R WORD .E. TOP.(ITS)                                  001390
                            S=SEQRDR.(ITS)                                      001400
SCANI                       ES=SEQLR.(S,F)                                      001410
                            W'R F .NE.0, T'O SCANI                              001420
                            IT=S                                                001430
                            T'O TRY                                             001440
                        O'E                                                     001450
                            T'O SCAN                                            001460
                        E'L                                                     001470
                    E'L                                                         001480
                    W'R FR .G. 0, T'O NOMATCH(LIMIT)                            001490
TRY                 W'R YMATCH.(TOP.(ES),INPUT,MTLIST.(TEST)) .E. 0,T'O MATCH   001500
                    ESRDR=SEQRDR.(ES)                                           001510
                    SEQLR.(ESRDR,ESF)                                           001520
                    POINT=SEQLR.(ESRDR,ESF)                                     001530
                    POINTR=LSPNTR.(ESRDR)                                       001540
                    W'R ESF .E. 0                                               001550
                        NEWBOT.(1,POINTR)                                       001560
                        TRANS=POINT                                             001570
                        T'O HIT                                                 001580
                       O'E                                                      001590
                        T'H FNDHIT,FOR I=0,1, I .G. POINT                       001600
FNDHIT                  TRANS=SEQLR.(ESRDR,ESF)                                 001610
                        W'R ESF .G. 0                                           001620
                            SEQLR.(ESRDR,ESF)                                   001630
                            SEQLR.(ESRDR,ESF)                                   001640
                            TRANS=SEQLR.(ESRDR,ESF)                             001650
                            SUBST.(1,POINTR)                                    001660
                            T'O HIT                                             001670
                           0'E                                                  001680
                            SUBST.(POINT+1,POINTR)                              001690
                            T'O HIT                                             001700
                        E'L                                                     001710
                    E'L                                                         001720
HIT                 TXTPRT.(ASSMBL.(TRANS,TEST,MTLIST.(OUTPUT)),0)              001730
                    T'O START                                                   001740
                E'L                                                             001750
               R* * * * * * * * * * INSERT NEW KEYWORD LIST                     001760
NEWLST          POPTOP.(INPUT)                                                  001770
                NEWBOT.(LSSCPY.(INPUT,LIST.(9)),KEY(HASH.                       001780
               1(TOP.(INPUT),5)))                                               001790
                T'O START                                                       001800
               R* * * * * * * * * * DUMP REVISED SCRIPT                         001810
ENDPLA          PRINT COMMENT $WHAT IS TO BE THE NUMBER OF THE NEW SCRIPT$      001820
                READ FORMAT SNUMB,SCRIPT                                        001830
                LPRINT.(INPUT,SCRIPT)                                           001840
                NEWTOP.(MEMORY,MTLIST.(OUTPUT))                                 001850
                NEWTOP.($MEMORY$,OUTPUT)                                        001860
                T'H DUMP, FOR I=1,1, I .G. 4                                    001870
DUMP            NEWBOT.(MYTRAN(I),OUTPUT)                                       001880
                LPRINT.(OUTPUT,SCRIPT)                                          001890
                MTLIST.(OUTPUT)                                                 001900
                T'H WRITE, FOR I=0,1, I .G. 32                                  001910
POPMOR          W'R LISTMT.(KEY(I)) .E. 0, T'O WRITE                            001920
                LPRINT.(POPTOP.(KEY(I)),SCRIPT)                                 001930
                T'O POPMOR                                                      001940
WRITE           CONTINUE                                                        001950
                LPRINT.(MTLIST.(INPUT),SCRIPT)                                  001960
                EXIT.                                                           001970
               R* * * * * * * * * * SCRIPT ERROR EXIT                           001980
NOMATCH(1)      PRINT COMMENT $PLEASE CONTINUE $                                002200
                T'O START                                                       002210
NOMATCH(2)      PRINT COMMENT $HMMM $                                           002220
                T'O START                                                       002230
NOMATCH(3)      PRINT COMMENT $GO ON , PLEASE $                                 002240
                T'O START                                                       002250
NOMATCH(4)      PRINT COMMENT $I SEE $                                          002260
                T'O START                                                       002270
                VECTOR VALUES SNUMB= $I3 * $                                    002280
                E'M                                                             002290
```
