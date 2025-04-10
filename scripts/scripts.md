# ELIZA Scripts

## Contemporaneous scripts compatible with 1966 CACM ELIZA

I am aware of just three scripts from the 1960s that are compatible with 1966 CACM ELIZA. These are...

- [ELIZA-script-DOCTOR-original-1966-CACM-appendix.txt](https://github.com/anthay/ELIZA/blob/master/scripts/ELIZA-script-DOCTOR-original-1966-CACM-appendix.txt) is
a verbatim transcription (with added whitespace) of the script published as an appendix to Joseph Weizenbaum's ELIZA paper published
in the January 1966 Communications of the Association of Computing Machinery (CACM). This script is built-in to both
[eliza.cpp](https://github.com/anthay/ELIZA/blob/master/src/eliza.cpp) and [eliza.html](https://github.com/anthay/ELIZA/blob/master/src/eliza.html).
It has 66 keywords.

- [ELIZA-script-02-000311051-1965-03-06-TAPE-100.txt](https://github.com/anthay/ELIZA/blob/master/scripts/ELIZA-script-02-000311051-1965-03-06-TAPE-100.txt) is
a verbatim transcription (with added whitespace) of the script following the ELIZA source code in a computer listing from Joseph Weizenbaum's MIT
archive ([02-000311051.pdf](https://hdl.handle.net/1721.3/201699)). It has 35 keywords.

- [ELIZA-script-02-000311051-1965-03-06-TAPE-102.txt](https://github.com/anthay/ELIZA/blob/master/scripts/ELIZA-script-02-000311051-1965-03-06-TAPE-102.txt) is 
a verbatim transcription (with added whitespace) of the script preceeding the ELIZA source code in that listing. It won't run as-is because the
line (DON'T YOU KNOW) is out-of-place. If you remove this line, or move it into the PERHAPS rule where it was probably meant to be, the script is valid.
It has 33 keywords and is, perhaps unsurprisingly, very similar to the script-02-000311051-1965-03-06-TAPE-100.txt mentioned above.

I hope that some day more scripts and conversations may be found in the archives of the people who used ELIZA.


## Contemporaneous scripts incompatible with 1966 CACM ELIZA

[ELIZA-script-YAPYAP-original.txt](https://github.com/anthay/ELIZA/blob/master/scripts/ELIZA-script-YAPYAP-original.txt) is
a verbatim transcription (with added whitespace) of a script used in a 1965 pilot study by Quarton,
McGuire and Lorch titled

```text
MAN-MACHINE NATURAL LANGUAGE EXCHANGES BASED ON SELECTED FEATURES OF
                     UNRESTRICTED INPUT-I.
          THE DEVELOPMENT OF THE TIME-SHARED COMPUTER
      AS A RESEARCH TOOL IN STUDYING DYADIC COMMUNICATION
```

This script will not work with the 1966 CACM ELIZA.
(But [ELIZA-script-YAPYAP-modified-for-1966-CACM-ELIZA.txt](https://github.com/anthay/ELIZA/blob/master/scripts/ELIZA-script-YAPYAP-modified-for-1966-CACM-ELIZA.txt), which has had
the non-1966 CACM ELIZA parts in YAPYAP removed, will.)


## Modern scripts for 1966 CACM ELIZA

In the 1966 CACM paper Weizenbaum says "ELIZA scripts exist (at this writing) in Welsh and Gernman as well as in English." So far only English scripts have been found in Weizenbaum's archives. This is a modern translation of the DOCTOR script into French: [ELIZA-script-DOCTOR-French-Jeu-de-Paume.txt](https://github.com/anthay/ELIZA/blob/master/scripts/ELIZA-script-DOCTOR-French-Jeu-de-Paume.txt)

Peter Millican wrote a [paper](https://sites.google.com/view/elizagen-org/blog/eliza-is-turing-complete) explaining why 1966 CACM ELIZA is Turing complete.
Here are some ELIZA scripts I wrote that demonstrate this...

[ELIZA-script-equal-number-Turing-machine.txt](https://github.com/anthay/ELIZA/blob/master/scripts/ELIZA-script-equal-number-Turing-machine.txt) is
a script that implements a Turing machine for 1966 CACM ELIZA to determine if an input contains an equal number of As and Bs. The input `EQUAL A B B A`
will produce the response `YES`, and `EQUAL A A A B B` wil produce `NO`.

[ELIZA-script-palindrome-Turing-machine.txt](https://github.com/anthay/ELIZA/blob/master/scripts/ELIZA-script-palindrome-Turing-machine.txt) is
a script that implements a Turing machine for 1966 CACM ELIZA to determine if an input is a palindrome of As and Bs. The input `PALP A B B A`
will produce the response `TRUE`, and `PALP A A A B B` wil produce `FALSE`.

[ELIZA-script-Turing-example1.txt](https://github.com/anthay/ELIZA/blob/master/scripts/ELIZA-script-Turing-example1.txt) is
a script that implements Turing's Example I in his On Computable Numbers (Proceedings of the London Mathematical Society, Nov. 12, 1936, page 233):
"I. A machine can be constructed to compute the sequence 010101... ."

[ELIZA-script-Turing-example2.txt](https://github.com/anthay/ELIZA/blob/master/scripts/ELIZA-script-Turing-example2.txt) is
a script that implements Turing's Example II in his On Computable Numbers (Proceedings of the London Mathematical Society, Nov. 12, 1936, page 234):
"II. As a slightly more difficult example we can construct a machine to compute the sequence 001011011101111011111... ."


#### To run 1966 CACM-compatible scripts on ELIZA

Having compiled eliza.cpp, run it with the script path on the command line. E.g.:

macOS

```text
./eliza ELIZA-script-equal-number-Turing-machine.txt
```

Windows

```text
eliza ELIZA-script-equal-number-Turing-machine.txt
```

If no script is specified on the command line the built-in DOCTOR script is used.

For the Javascript version open eliza.html as a webpage, then type `*load` and hit return.
You can then select the script you wish to run in the file selector dialogue box.


#### Watching Turing machines run

When running Turing machine scripts it may be of interest to see the "tape" at each change of state. In both eliza.cpp and eliza.html the
`*tracepre` command will show this. For example, here the equal-number script was selected in the file-selection dialog invoked by the
`*load` command. Then the `*tracepre` command was given to turn on tracing. The Turing machine was started with the input `equal a a b b b a a b`.
Each line of the trace shows the text input (the tape), a colon and the currently selected keyword (the state) just prior to executing
the script rule associated with the selected keyword.

```text
*load

Loading 'ELIZA-script-equal-number-Turing-machine.txt'...

ELIZA CAN DECIDE IF A SEQUENCE CONTAINS AN EQUAL NUMBER OF THE LETTERS A AND B.
TYPE THE WORD EQUAL FOLLOWED BY A STRING OF A AND B LETTERS, WITH SPACES BETWEEN
EACH LETTER. ELIZA WILL RESPOND YES IF THERE ARE THE SAME NUMBER OF A LETTERS AS
THERE ARE OF B LETTERS, OTHERWISE ELIZA SAYS NO. E.G. TYPE 'EQUAL A A B B' AND
ELIZA WILL RESPOND YES. USE THE *TRACEPRE COMMAND TO WATCH THE 'READ-WRITE HEAD'
- THE CELL BETWEEN APOSTROPHES - MOVE OVER THE 'TAPE'. BLANK CELLS ARE REPRESENTED
BY PERIODS.

*tracepre

minimal pre-transformation tracing enabled

equal a a b b b a a b

EQUAL A A B B B A A B :EQUAL
. . ' A ' A B B B A A B . . :Q0
. . X ' A ' B B B A A B . . :Q1
. . X A ' B ' B B A A B . . :Q1
. . X ' A ' X B B A A B . . :Q3
. . ' X ' A X B B A A B . . :Q3
. ' . ' X A X B B A A B . . :Q3
. . ' X ' A X B B A A B . . :Q0
. . X ' A ' X B B A A B . . :Q0
. . X X ' X ' B B A A B . . :Q1
. . X X X ' B ' B A A B . . :Q1
. . X X ' X ' X B A A B . . :Q3
. . X ' X ' X X B A A B . . :Q3
. . ' X ' X X X B A A B . . :Q3
. ' . ' X X X X B A A B . . :Q3
. . ' X ' X X X B A A B . . :Q0
. . X ' X ' X X B A A B . . :Q0
. . X X ' X ' X B A A B . . :Q0
. . X X X ' X ' B A A B . . :Q0
. . X X X X ' B ' A A B . . :Q0
. . X X X X X ' A ' A B . . :Q2
. . X X X X ' X ' X A B . . :Q3
. . X X X ' X ' X X A B . . :Q3
. . X X ' X ' X X X A B . . :Q3
. . X ' X ' X X X X A B . . :Q3
. . ' X ' X X X X X A B . . :Q3
. ' . ' X X X X X X A B . . :Q3
. . ' X ' X X X X X A B . . :Q0
. . X ' X ' X X X X A B . . :Q0
. . X X ' X ' X X X A B . . :Q0
. . X X X ' X ' X X A B . . :Q0
. . X X X X ' X ' X A B . . :Q0
. . X X X X X ' X ' A B . . :Q0
. . X X X X X X ' A ' B . . :Q0
. . X X X X X X X ' B ' . . :Q1
. . X X X X X X ' X ' X . . :Q3
. . X X X X X ' X ' X X . . :Q3
. . X X X X ' X ' X X X . . :Q3
. . X X X ' X ' X X X X . . :Q3
. . X X ' X ' X X X X X . . :Q3
. . X ' X ' X X X X X X . . :Q3
. . ' X ' X X X X X X X . . :Q3
. ' . ' X X X X X X X X . . :Q3
. . ' X ' X X X X X X X . . :Q0
. . X ' X ' X X X X X X . . :Q0
. . X X ' X ' X X X X X . . :Q0
. . X X X ' X ' X X X X . . :Q0
. . X X X X ' X ' X X X . . :Q0
. . X X X X X ' X ' X X . . :Q0
. . X X X X X X ' X ' X . . :Q0
. . X X X X X X X ' X ' . . :Q0
. . X X X X X X X X ' . ' . :Q0
. . X X X X X X X X ' . ' . :QACCEPT
YES, THERE ARE THE SAME NUMBER OF A AND B LETTERS
```


