# What I learned from reading the original ELIZA source code

This is an update to a previous project: [A Simulation in C++ of Joseph Weizenbaum’s 1966 ELIZA](https://github.com/anthay/ELIZA/blob/master/doc/Eliza_part_1.md).

In April 2021 Jeff Shrager located a listing of Joseph Weizenbaum's ELIZA program in the MIT archives. The listing appears to be dated 03/06 and was in a folder labeled "COMPUTER CONVERSATIONS (1965)". Jeff wrote about this on [elizagen.org](https://sites.google.com/view/elizagen-org/the-original-eliza). It is now in the public domain and for the first time anyone who cares to can read the code for the program Weizenbaum describes in his 1966 CACM paper[^CACM-paper].

I've read it and discovered a few things about how ELIZA works that were not (or not fully) described in the CACM paper. I updated my C++ simulation of Eliza to take account of what I learned. That code is here: [eliza.cpp](https://github.com/anthay/ELIZA/blob/master/src/eliza.cpp)

One piece of information remains missing: the HASH algorithm used. It turns out that ELIZA uses HASH to select certain messages. Without the HASH algorithm a recreation of ELIZA won't make the exact same choices the original would have made. I hope the HASH code is also somewhere in the MIT archives, waiting to be discovered.





## Information not in the CACM paper


### 1. The word "BUT" is a delimiter

In the CACM paper on page 37 Weizenbaum says

> ...the procedure recognizes a comma or a period as a delimiter. Whenever either one is encountered and a keyword has already been found, all subsequent text is deleted from the input message. If no key had yet been found the phrase or sentence to the left of the delimiter (as well as the delimiter itself) is deleted. As a result, only single phrases or sentences are ever transformed.

So ELIZA recognises exactly two characters as delimiters, a period and a comma.

In the example conversation in the CACM paper on the same page there is this exchange

```text
You are not very aggressive but I think you don't want me to notice that.
WHAT MAKES YOU THINK I AM NOT VERY AGGRESSIVE
```

Weizenbaum's description suggests that ELIZA's response should have been

```text
WHAT MAKES YOU THINK I AM NOT VERY AGGRESSIVE BUT YOU THINK I DON'T WANT YOU TO NOTICE THAT
```

Here is the line of code that detects delimiters

```text
    W'R WORD .E. $.$ .OR. WORD .E. $,$ .OR. WORD .E. $BUT$    000660
```

`W'R` is short for `WHENEVER`, which is like `if` in C++. `.E.` means equals, strings are delimited by `$` and the trailing `000660` is the line number.

So contrary to Weizenbaum's description, ELIZA actually recognises three delimiters: a period, a comma and the word BUT.

(In my original implementation I followed the description in the CACM paper and made the assumption that there should have been a comma after the word "aggressive" in the example conversation, which had somehow got lost in publication.)



### 2. How "a certain counting mechanism" works

In the CACM article on page 41 Weizenbaum says

>Consider the following structure:
>
```    (MEMORY MY       (0 YOUR 0 = LETS DISCUSS FURTHER WHY YOUR 3)
       (0 YOUR 0 = EARLIER YOU SAID YOUR 3)
        :
```
>The word "MY" (which must be an ordinary keyword as well) has been selected to serve a special function. Whenever it is the highest ranking keyword of a text one of the transformations on the MEMORY list is randomly selected, and a copy of the text is transformed accordingly. This transformation is stored on a first-in-first-out stack for later use. The ordinary processes already described are then carried out. When a text without keywords is encountered later and a certain counting mechanism is in a particular state and the stack in question is not empty, then the transformed text is printed out as the reply. It is, of course, also deleted from the stack of such transformations.
>The current version of ELIZA requires that one keyword be associated with MEMORY and that exactly four transformations accompany that word in that context.

No further information is given in the paper about _a certain counting mechanism_ or _a particular state_.

In the MAD-SLIP code the relevant mechanism relates to a variable called LIMIT. Before entering the main program loop LIMIT is given the value 1. It is  incremented each time round the loop, just after reading the user's input text. If after incrementing LIMIT its value is 5 it is returned to 1 and the cycle repeats.

Only when LIMIT has the value 4 is the previously saved memory retrieved.

Here are the relevant parts of the ELIZA source

```text
           LIMIT = 1                                                000120
                                                                    :
          R* * * * * * * * * * BEGIN MAJOR LOOP                     000470
                                                                    :
           LIMIT=LIMIT+1                                            000510
           W'R LIMIT .E. 5, LIMIT=1                                 000520
                                                                    :
          R* * * * * * * * * * END OF MAJOR LOOP                    001110     
ENDTXT     W'R IT .E. 0                                             001120
               W'R LIMIT .E. 4 .AND. LISTMT.(MYLIST) .NE. 0         001130
                 OUT=POPTOP.(MYLIST)                                001140
                 TXTPRT.(OUT,0)                                     001150
                 IRALST.(OUT)                                       001160
                 T'O START                                          001170
                O'E                                                 001180
                 ES=BOT.(TOP.(KEY(32)))                             001190
                 T'O TRY                                            001200
               E'L                                                  001210
              OR W'R KEYWORD .E. MEMORY                             001220
               I=HASH.(BOT.(INPUT),2)+1                             001230
               NEWBOT.(REGEL.(MYTRAN(I),INPUT,LIST.(MINE)),MYLIST)  001240
               SEQLL.(IT,FR)                                        001250
               T'O MATCH                                            001260
              O'E                                                   001270
               SEQLL.(IT,FR)                                        001280
          R* * * * * * * * * * MATCHING ROUTINE                     001290

```



In the above quoted text from the CACM paper Weizenbaum says that a rule from the MEMORY list is “randomly selected.” But from the above code on line 001230 we can see that in fact the selection is not random but based on a hash function of the user’s input text, `I=HASH.(BOT.(INPUT),2)+1`. _Therefore, conversations with Eliza do not in fact have a random element and are reproducible._

We do not yet have the implementation of the HASH function.



### 3. There are 'hard-coded' responses in the code

When Eliza detects that the script is not well-formed it will display one of four hard-coded messages.


```text
                   R* * * * * * * * * * SCRIPT ERROR EXIT           001980
    NOMATCH(1)      PRINT COMMENT $PLEASE CONTINUE $                002200
                    T'O START                                       002210
    NOMATCH(2)      PRINT COMMENT $HMMM $                           002220
                    T'O START                                       002230
    NOMATCH(3)      PRINT COMMENT $GO ON , PLEASE $                 002240
                    T'O START                                       002250
    NOMATCH(4)      PRINT COMMENT $I SEE $                          002260
                    T'O START                                       002270
```

There are two places in the code where these `NOMATCH` labels are jumped to via `T'O NOMATCH(LIMIT)`. (`T'O` is an abreviation of `TRANSFER TO`.)

Under normal circumstances, if no keyword is found in the user's input and no memory is available, or it's not time for the recall of a memory, ELIZA selects one of the messages from the `NONE` list in the script. But there are circumstances where a keyword has been identified, but the patterns associated with that keyword do not match the user's input. This probably shouldn't happen if the script is correctly designed. If it does happen one of these hard-coded messages is displayed. Which one is displayed depends on the value of the variable `LIMIT` at the time of the error.

Rather than display a message such as "script error", Weizenbaum chooses to hide the problem from the user. Although, I'm sure he would have recognised a _HMMM_ for what it really meant.



## Notes


### 1. Reading the original source code

Weizenbaum says ELIZA is written in a language called MAD-SLIP for the IBM 7094.[^CACM-paper-ibid] MAD is a language designed in 1959 and called The Michigan Algorithm Decoder. In the early 1960s Weizenbaum developed a system for managing linked lists, which he called Symetric List Processor, or SLIP. ELIZA makes heavy use of this system.

Photographs of the ELIZA source code listing and reference material about MAD and SLIP can be found on the [elizagen.org](https://sites.google.com/view/elizagen-org/the-original-eliza) website.

My transcript of the CC0 public domain ELIZA source code: [ELIZA_transcription.txt](https://github.com/anthay/ELIZA/blob/master/doc/ELIZA_transcription.txt).

My annotated transcript of the code: [ELIZA\_transcription\_annotated.txt](https://github.com/anthay/ELIZA/blob/master/doc/ELIZA_transcription_annotated.txt).

My updated, CC0 public domain C++ implementation of ELIZA: [eliza.cpp](https://github.com/anthay/ELIZA/blob/master/src/eliza.cpp).

Here is a very quick intro to MAD-SLIP...


#### Abbreviations used in the code

    .E.             equal
    .NE.            not equal
    .L.             less than
    .LE.            less than or equal to
    .G.             greater than
    .GE.            greater than or equal to

    W'R             WHENEVER                (if)
    OR W'R          OR WHENEVER             (else if)
    O'E             OTHERWISE               (else)
    E'L             END OF CONDITIONAL      (endif)

    T'O             TRANSFER TO             (goto)

    T'H <label>     THROUGH <label>         (loop until <label>)
    F'N <val>       FUNCTION RETURN <val>   (return <val>)
    E'N             END OF FUNCTION


#### MAD arrays

MAD arrays are declared with a `DIMENSION` statement. `DIMENSION D(N)` allocates `N + 1` contiguous machine-words of core memory, which are accessed using `D(0) .. D(N)` (where `D` is the array name).


#### Character strings

Weizenbaum developed ELIZA on an IBM 7094, which has a 36-bit word size. Characters on this machine were encoded into 6 bits, so each machine word could contain up to 6 characters. In SLIP a string is recorded in one or more SLIP cells. If a string contained more than 6 characters it would be continued into the following SLIP cell. Each cell, except the last, has its sign bit set to indicate the string is continued. (I don't understand how the sign bit doesn't interfere with the first character in the cell.)





### 2. This listing is not the final version of ELIZA

In the annotated ELIZA source I've noted things that do not correspond to the description in the CACM paper. 

For example, the _keystack_ Weizenbaum describes on page 39 is not implemented. Instead, only the highest ranking keyword is retained. There is consequently no support for the NEWKEY mechanism.

Another difference: on page 42 of the CACM paper

>Editing of an ELIZA script is achieved via appeal to a contextual editing program (ED) which is part of the MAC library. This program is called whenever the input text to ELIZA consists of the single word "EDIT". ELIZA then puts itself in a so-called dormant state and presents the then stored script for editing.

There is no code to implement this "EDIT" functionality. Instead ELIZA recognises user input `+` to invoke a CHANGE function, and `*` to add a new transformation rule.

Presumably, Weizenbaum continued to work on ELIZA after this listing was printed. In his book Computer Power and Human Reason, Weizenbaum says he worked on ELIZA from 1964 to 1966.



### 3. Quirks

In the annotated transcription I've noted a few quirks. For example, it seems to me that keywords must differ in the first six characters. If two or more keywords have the same first six characters only one of the rules will be used.

Please note that I am no MAD-SLIP expert. My knowledge is limited to trying to puzzle out how this one program, ELIZA, works. I've almost certainly misunderstood things and would be very happy to be corrected.



### 4. My ELIZA

I've changed my implementation to reflect the new information described above. In my previous version, `eliza()` was a standalone function. It is now a member function of a class called `eliza`, but is very little changed. `eliza` is now a class, mainly just because of the `LIMIT` state.

```text
**********************************************************************
** Visual Studio 2019 Developer Command Prompt v16.11.10
** Copyright (c) 2021 Microsoft Corporation
**********************************************************************
[vcvarsall.bat] Environment initialized for: 'x64'

C:\Program Files (x86)\Microsoft Visual Studio\2019\Community>cd \Eliza

C:\Eliza>cl /EHsc /W4 eliza.cpp
Microsoft (R) C/C++ Optimizing Compiler Version 19.29.30140 for x64
Copyright (C) Microsoft Corporation.  All rights reserved.

eliza.cpp
Microsoft (R) Incremental Linker Version 14.29.30140.0
Copyright (C) Microsoft Corporation.  All rights reserved.

/out:eliza.exe
eliza.obj

C:\>eliza
-----------------------------------------------------------------
      ELIZA -- A Computer Program for the Study of Natural
         Language Communication Between Man and Machine
DOCTOR script by Joseph Weizenbaum, 1966  (CC0 1.0) Public Domain
This implementation by Anthony Hay, 2022  (CC0 1.0) Public Domain
-----------------------------------------------------------------
ELIZA /help for usage.
Using Weizenbaum's 1966 DOCTOR script.
Enter a blank line to quit.


HOW DO YOU DO. PLEASE TELL ME YOUR PROBLEM

Mother I tried please believe me
YOU SAY MOTHER YOU TRIED PLEASE BELIEVE YOU

I'm doing the best that I can.
IS IT BECAUSE YOU ARE DOING THE BEST THAT YOU CAN THAT YOU CAME TO ME

I'm ashamed of the things I've been put through
HOW LONG HAVE YOU BEEN ASHAMED OF THE THINGS I'VE BEEN PUT THROUGH

I'm ashamed of the person I am.
DO YOU BELIEVE IT NORMAL TO BE ASHAMED OF THE PERSON YOU ARE

Isolation, isolation, isolation.
I AM NOT SURE I UNDERSTAND YOU FULLY

But if you could just see the beauty
DO YOU THINK ITS LIKELY THAT I COULD JUST SEE THE BEAUTY

These things I could never describe
CAN YOU ELABORATE ON THAT

These pleasures a wayward distraction
PLEASE GO ON

This is my one lucky prize.
YOUR ONE LUCKY PRIZE



C:\Eliza>
```

Anthony Hay, March 2022, Devon.




[^CACM-paper]: ELIZA: A computer program for the study of natural language communication between man and machine. Communications of the ACM, 9, 36-45. Currently available here: <https://web.stanford.edu/class/linguist238/p36-weizenabaum.pdf>

[^CACM-paper-ibid]: Ibid. page 36
