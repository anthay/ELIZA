# The HASH algorithm used by ELIZA

One of the responses ELIZA gives is partly determined by a pseudo-random selection from a list of sentence decomposition/reassembly rules. These notes describe the mechanism used to make that selection.

The key part of the mechanism that was not public was the algorithm used in the SLIP function `HASH(D,N)`.


## HASH found

In April 2022 Jeff Shrager returned (remotely) to Joseph Weizenbaum's MIT archives and viewed a number of documents. A folder labelled "FAP" contained a listing of various SLIP functions, including this one

![Blurry image of line printer listing showing assembly code](https://github.com/anthay/ELIZA/blob/master/doc/JW_MIT_archive_Slip_HASH_function.jpg)

This is the code that implements the SLIP `HASH(D,N)` function used in ELIZA. It’s written in FAP[^Sherman1], or FORTRAN Assembly Program, for the IBM 7094.

[^Sherman1]: "An assembly language very commonly used for the IBM 7090 computer is FAP (FORTRAN Assembly Program). FAP is a modification of SAP (Symbolic Assembly Program), written by United Aircraft for the IBM 704 computer.", Sherman, 1963, Page 14


Weizenbaum developed SLIP, so he may have written `HASH`. However, another folder in the archive contained a listing with the handwritten title "MAY 21, 1966 / LISTING OF 1620 SLIP SYSTEM / DICK SITES / DAVID P. KELLEHER", so they may be the `HASH` authors.

At the time of writing, the archive has not been made public. If and when it is, it is likely to be available through [elizagen.org](https://sites.google.com/view/elizagen-org/original-eliza).



### Transcription

I’ve transcribed and annotated the code here

```text
             HASH   FAP
            ENTRY   HASH
    HASH    LDQ*    1,4                 load D into the MQ register
            CLA*    2,4                 load N into the AC (accumulator)
            STA     SHIFT               store N for later use by LLS instruction
            ARS     1                   AC = N/2
            STA     *+2                 store N/2 for use by next LLS instruction
            MPY*    1,4                 AC/MQ = D * D
            LLS     **                  shift AC/MQ left by N/2 bits
            STA     TEMP                store bits 21-35 of hash result in TEMP
            LDQ     =O777777777777      set all 36 bits in MQ (=O means octal)
            ZAC                         clear all 36 bits in accumulator
    SHIFT   LLS     **                  shift N bits from MQ into accumulator
            ANA     TEMP                and hashed value with accumulator
            TRA     3,4                 return to calling code; result in AC
    TEMP    PZE                         reserve one word for temp storage
            END
```


### Reading this code

There is documentation on FAP in Sherman[^Sherman] and Michigan[^Michigan]. The following notes summarise information relevant to the `HASH` code.

[^Sherman]: Philip M. Sherman, Programming and Coding the IBM 709-7090-7094 Computers, John Wiley and Sons, 1963, (Web search for ibm709.pdf)

[^Michigan]: University of Michigan Executive System for the IBM 7090 Computer, September 1964, In section THE 'UNIVERSITY OF MICHIGAN ASSEMBLY PROGRAM' ('UMAP'), (Available online from Google Books.)

The mnemonics used in the `HASH` function

```text
    ANA = and to accumulator
    ARS = accumulator right shift
    CLA = clear AC and add (AC is the accumulator)
    LDQ = load MQ (MQ is the multiplier-quotient register)
    LLS = long left shift (bits 1-35 of AC and bits 1-35 of MQ are
          shifted as if they were one register; bit 1 of MQ shifts
          into bit 35 of AC)
    MPY = multiply bits 1-35 of MQ with bits 1-35 of specified value
          to give a 70-bit result in AC/MQ (P and Q bits set to zero)
    PZE = prefix of plus zero (assembles a single machine word with a
          plus zero as its prefix)
    STA = store address; copy bits 21-35 in the AC (the 15 least-
          significant bits) to the specified location, leaving the
          other bits in the destination unchanged
    TRA = transfer to specified address (aka jump)
    ZAC = zero the accumulator
```

FAP lines have the format

```text
    <op>    <address>,<tag>,<decrement>
```

In the `<address>` field, `*` means "present location," so `STA *+2` means store accumulator to present location + 2.

Also in the `<address>` field `**` means value provided at run time. This code [modifies itself](https://en.wikipedia.org/wiki/Self-modifying_code) as it runs.

An `<op>` mnemonic followed by an asterisk indicates indirect addressing is to be used. The `<tag>` field specifies which of the 7090's three index registers, numbered 1, 2 and 4, are to be used. In this code it looks like index register 4 is some kind of stack frame pointer, so the parameters D and N and the return address are at offsets 1, 2 and 3 respectively to the address stored in this index register.



### How does HASH work?

The SLIP `HASH(D,N)` function returns the middle N bits of D squared. This kind of hash is known as mid-square[^wikihash].

[^wikihash]: "A mid-squares hash code is produced by squaring the input and extracting an appropriate number of middle digits or bits. For example, if the input is 123,456,789 and the hash table size 10,000, squaring the key produces 15,241,578,750,190,521, so the hash code is taken as the middle 4 digits of the 17-digit number (ignoring the high digit) 8750. The mid-squares method produces a reasonable hash code if there is not a lot of leading or trailing zeros in the key. This is a variant of multiplicative hashing, but not as good because an arbitrary key is not a good multiplier.", <https://en.wikipedia.org/wiki/Hash_function>


The IBM 7094 uses sign-magnitude representation of integers: in a 36-bit integer, the most-significant bit is assumed to be the sign of the integer, and the least-significant 35-bits are assumed to be the magnitude of the integer. Therefore, in the SLIP HASH implementation only the least-significant 35-bits of D are squared. When the datum holds six 6-bit characters the top bit of the first character in the given D will be assumed to be a sign bit and will not be part of the 35-bit multiplication (except as a sign). There is more on this below.

The STA instruction copies only the least-significant 15 bits of the accumulator to the destination[^Sherman3]. Therefore, in this implementation any bits beyond the least-significant 15 will always be 0. So 15 is the upper limit for N. 

[^Sherman3]: "STORE ADDRESS (STA Y) (+0621); 2 cycles. The contents of the address field of the AC, i.e., bits 21-35, replaces the contents of the address field of  location Y. The C(AC) and the other bits in Y are unchanged.", Sherman, 1963, Page 25


Here is SLIP `HASH` reimplemented in C++

```cpp
// recreate the SLIP HASH function: return an n-bit hash value for
// the given 36-bit datum d, for values of n in range 0..15
int hash(uint_least64_t d, int n)
{
    d &= 0x7FFFFFFFFull;         // clear the "sign" bit
    d *= d;                      // square it
    d >>= 35 - n / 2;            // move middle n bits to least sig. bits
    return d & (1ull << n) - 1;  // mask off all but n least sig. bits
}
```

On the IBM 7094 multiplying two 35-bit numbers produces a 70 bit result. In the C++ code above, the result will be truncated to 64-bits. (The code uses unsigned arithmetic. In C++ unsigned arithmetic overflow is not undefined behaviour, as it is for signed arithmetic.) If n is 15 the middle 15 bits of a 70-bit number are bits 42-28 (bit 0 least significant), which is well within our 64-bit calculation.



## How is HASH used in ELIZA?

`HASH` is used in several places in the ELIZA code, but the only place where it has an effect on the behaviour is when it is used to select a rule from the MEMORY list. This is the code

```text
    OR W'R KEYWRD .E. MEMORY                                     001220
     I=HASH.(BOT.(INPUT),2)+1                                    001230
     NEWBOT.(REGEL.(MYTRAN(I),INPUT,LIST.(MINE)),MYLIST)         001240
```

`INPUT` is the user's processed input text. On line 1230 `I` becomes the 2-bit `HASH` of the `BOT` (last word) of the user's `INPUT` sentence, plus 1. `I` is then used on line 1240 as an index into `MYTRAN`, the 4-entry array of MEMORY rules.

Actually, `BOT` returns the last *cell* in `INPUT`, which is not necessarily the whole of the last word. We need to understand a bit more about how ELIZA stores the user's input text...



### SLIP strings

ELIZA stores strings in SLIP lists, which are doubly-linked lists of cells[^SLIP]. A SLIP cell consists of two adjacent machine words. The first word contains a 2-bit cell type field and two addresses, one pointing to the previous cell and the other pointing to the next cell in the list. (The IBM 7094 had a 32,768 word core store, so only 15 bits are required for an address. So two addresses and a 2-bit type field fit into one 36-bit word with 4 bits spare.) The second word of the cell may carry the "datum." This is where the characters are stored.

[^SLIP]: Symmetric list processor. Communications of the ACM, Volume 6, Number 9, September 1963, Pages 524-536.

The IBM 7094 peripherals use Hollerith character encoding. Hollerith encodes each character in 6 bits. The IBM 7094 machine word size is 36-bits.

The datum part of each SLIP cell can store up to six Hollerith-encoded characters. If a string has six or fewer characters it is stored in a single SLIP cell, left-justified and space padded to the right.

If a string has more than six characters, it is stored in successive SLIP cells. Each cell except the last has the sign bit set in the first word of the cell pair to indicate the string is continued in the next cell. As only 32 bits are used in the first word, setting the sign bit does not interfere with the cell-type bits or addresses.

So the word `"INVENTED"` would be stored in two SLIP cells, `"INVENT"` in the first and `"ED˽˽˽˽"` in the second.

In ELIZA, the user's input text is read into a SLIP list, each word in the sentence is in its own cell, unless a word needs to be continued in the next cell because it's more than six characters long.

When ELIZA chooses a MEMORY rule it hashes the last cell in the input sentence. That will be the last word in the sentence, or the last chunk of the last word, if the last word is more than six characters long.

To recreate the behaviour of ELIZA we must hash the exact same part of the last word. In addition, the characters in that word must be left-justified, space padded and Hollerith encoded.



### Hollerith character encoding

![A table of character glyphs and their corresponding octal character code from a book.](https://github.com/anthay/ELIZA/blob/master/doc/Hollerith_Char_Set_IBM709.jpg)

>"The 7090 BCD character codes are given in the accompanying table. Six bits are used for each character. [...] The code is generally termed binary-coded-decimal or BCD. For compactness, the codes are generally expressed as 2-digit octal numbers, as in the table. The term Hollerith is used synonymously with BCD." [^Sherman2]

[^Sherman2]: Sherman, 1963, Page 62


(Where is the carriage return, line feed or any kind of new line character code?)

There is a double quote, but no single quote (prime). We know ELIZA uses single quotes as they are present in the published script.

Here's another Hollerith table

![A table of character glyphs and their corresponding octal character code from a different book.](https://github.com/anthay/ELIZA/blob/master/doc/Hollerith_Char_Set_Michigan.jpg)

This table is a superset of the previous table, with three additional characters +0 (plus zero), -0 (minus zero) and what looks like ≠ (not equal). Also, code 14 (octal) is -(DASH), not double quote as it was shown in the previous table. The note says that this character may be displayed as prime (') on some hardware.

---

*We now know enough to locate the required part of the last word in the user's input text, pad it out to six characters with spaces if necessary and convert it to Hollerith encoding. The resulting 36-bit number is then hashed and the result used to select the MEMORY.*




## How do we know this is the HASH function used in ELIZA? 

We have four test cases.

### 1. Weizenbaum gives a test case in the 1966 CACM paper

In that paper[^Weizenbaum], Weizenbaum says on page 38

[^Weizenbaum]: ELIZA: A computer program for the study of natural language communication between man and machine. Communications of the ACM, 9, 36-45. Currently available here: <https://web.stanford.edu/class/linguist238/p36-weizenabaum.pdf>


>"As a particular key list structure is read the keyword K at its top is randomized (hashed) by a procedure that produces (currently) a 7 bit integer "i". The word "always", for example, yields the integer 14."

(Side note: In the version of the ELIZA source code we have, keywords are hashed to a 5-bit integer; the keyword hash table has only 32 buckets, not 128.)

So the first test is that a 7-bit hash of `"ALWAYS"` is 14. Here is the calculation

```text
text                                    "ALWAYS"
left-justified, space padded to 6 chars "ALWAYS"
Hollerith encoded (octal)               21 43 66 21 70 62
test is                                 HASH(0214366217062, 7) == 14
0214366217062 (octal) =                 0x463D91E32 (hexadecimal)
zero bit 35 (NB: bit 35 is already 0)   0x463D91E32
squared                                 0x1345BA970EE053C1C4
shift left by N/2 bits (3 bits)         0x9A2DD4B877029E0E20
discard the least significant 35 bits   0x1345BA970E
least significant N bits (7 bits)       0xE (= 14 decimal) (correct)
```


### 2. The CACM published script and conversation provide a test case

In the CACM published script, the MEMORY rule is

```text
    (MEMORY MY
        (0 YOUR 0 = LETS DISCUSS FURTHER WHY YOUR 3)
        (0 YOUR 0 = EARLIER YOU SAID YOUR 3)
        (0 YOUR 0 = BUT YOUR 3)
        (0 YOUR 0 = DOES THAT HAVE ANYTHING TO DO WITH THE FACT THAT YOUR 3))
```

In the CACM published conversation, the memory is formed by the input sentence

    "Well, my boyfriend made me come here."

In the ELIZA code, these lines choose the MEMORY rule

```text
            OR W'R KEYWRD .E. MEMORY                                     001220
             I=HASH.(BOT.(INPUT),2)+1                                    001230
             NEWBOT.(REGEL.(MYTRAN(I),INPUT,LIST.(MINE)),MYLIST)         001240
```

That's the 2-bit `HASH` of the `BOT` (last cell) of the user's `INPUT` sentence. `MYTRAN` (containing the 4 MEMORY rules) is indexed on the value returned by `HASH` plus 1.

Later, ELIZA says

    "DOES THAT HAVE ANYTHING TO DO WITH THE FACT THAT YOUR BOYFRIEND MADE YOU COME HERE"

So, the value returned by HASH in this case must have been 3 in order to select the correct rule...

```text
            (0 YOUR 0 = DOES THAT HAVE ANYTHING TO DO WITH THE FACT THAT YOUR 3)
```

So the test is that a 2-bit hash of `"HERE"` is 3. Here is the calculation

```text
text                                    "HERE"
left-justified, space padded to 6 chars "HERE  "
Hollerith encoded (octal)               30 25 51 25 60 60
test is                                 HASH(0302551256060, 2) == 3
0302551256060 (octal) =                 0x615A55C30 (hexadecimal)
zero bit 35 (NB: bit 35 is already 0)   0x615A55C30
squared                                 0x250594DE2FD7128900
shift left by N/2 bits (1 bit)          0x4A0B29BC5FAE251200
discard the least significant 35 bits   0x94165378B
least significant N bits (2 bits)       0x3 (correct)
```

### 3. and 4. Test cases deduced from another conversation

In an unpublished conversation from Weizenbaum’s MIT archive, dated 5 March 1965, memories are recalled in two of ELIZA's responses

            "EARLIER YOU SAID YOUR WIFE WANTS KIDS"
        and
            "LETS DISCUSS FURTHER WHY YOUR FATHER TALKS ABOUT GRANDCHILDREN ALL THE TIME"

We don’t have the script used for this conversation. But assuming the MEMORY rule in this missing script was the same as for the CACM published script shown above, we get these two tests

(3) A 2-bit hash of `"KIDS"` is 1.

```text
text                                    "KIDS"
left-justified, space padded to 6 chars "KIDS  "
Hollerith encoded (octal)               42 31 24 62 60 60
test is                                 HASH(0423124626060, 2) == 1
0423124626060 (octal) =                 0x899532C30 (hexadecimal)
zero bit 35                             0x99532C30
squared                                 0x5BD485D70EC08900
shift left by N/2 bits (1 bit)          0xB7A90BAE1D811200
discard the least significant 35 bits   0x16F52175
least significant N bits (2 bits)       0x1 (correct)
```

(4) A 2-bit hash of `"TIME"` is 0.

```text
text                                    "TIME"
left-justified, space padded to 6 chars "TIME  "
Hollerith encoded (octal)               63 31 44 25 60 60
test is                                 HASH(0633144256060, 2) == 0
0633144256060 (octal) =                 0xCD9915C30 (hexadecimal)
zero bit 35                             0x4D9915C30
squared                                 0x178572A252EF928900
shift left by N/2 bits (1 bit)          0x2F0AE544A5DF251200
discard the least significant 35 bits   0x5E15CA894
least significant N bits (2 bits)       0 (correct)
```


The implementation of hash in C++ shown earlier gives the correct answer in all four cases. It is incorporated into [eliza.cpp](https://github.com/anthay/ELIZA/blob/master/src/eliza.cpp), along with the the Hollerith encoding and so on described above.



## A rose by any other name

>"ELIZA is a program which makes natural language conversation with a computer possible. Its present implementation is on the MAC time-sharing system at MIT. It is written in MAD-SLIP for the IBM 7094. Its name was chosen to emphasize that it may be incrementally improved by its users, since its language abilities may be continually improved by a \"teacher\". Like the Eliza of Pygmalion fame, it can be made to appear even more civilized, the relation of appearance to reality, however, remaining in the domain of the playwright."[^Weizenbaum1]

[^Weizenbaum1]: ELIZA: A computer program for the study of natural language communication between man and machine. Communications of the ACM, 9, Page 36

I'm inclined to take at face value this explanation of where the name ELIZA came from, but I'm no scholar. And I don't know what he means by the relation of appearance to reality remaining in the domain of the playwright, unless it's that ELIZA's civilisation is just an illusion; Weizenbaum may be acknowledging that there is no understanding behind the words ELIZA uses.

>HIGGINS: "You see, we’re all savages, more or less. We’re supposed to be civilized and cultured—to know all about poetry and philosophy and art and science, and so on; but how many of us know even the meanings of these names? [To Miss Hill] What do you know of poetry? [To Mrs. Hill] What do you know of science? [Indicating Freddy] What does he know of art or science or anything else? What the devil do you imagine I know of philosophy?"[^Play]

[^Play]: <https://www.gutenberg.org/files/3825/3825-h/3825-h.htm> Act III


I don't know what Weizenbaum knew of the Greek myth of Pygmalion the sculptor, who fell in love with his own creation (he didn't like real women). In Bernard Shaw's play of the same name the female protagonist, Eliza, differs from Pygmalion's sculpture in that, from the beginning, the former has a mind of her own and is very outspoken. In the play, Henry Higgins teaches Eliza to speak like a lady. Near the end of the play Eliza says

>LIZA: "You see, really and truly, apart from the things anyone can pick up (the dressing and the proper way of speaking, and so on), the difference between a lady and a flower girl is not how she behaves, but how she’s treated. I shall always be a flower girl to Professor Higgins, because he always treats me as a flower girl, and always will; but I know I can be a lady to you, because you always treat me as a lady, and always will."[^Play1]

[^Play1]: <https://www.gutenberg.org/files/3825/3825-h/3825-h.htm> Act V


Weizenbaum worked on ELIZA from 1964-1966. He was obviously aware of Shaw's Pygmalion. He may have seen the very popular 1956 Broadway musical My Fair Lady, or the film of the same name released in 1964.



## ELIZA, climate science denier

>"I was startled to see how quickly and how very deeply people conversing with DOCTOR became emotionally involved with the computer and how unequivocally they anthropomorphized it. Once my secretary, who had watched me work on the program for many months and therefore surely knew it to be merely a computer program, started conversing with it. After only a few interchanges with it, she asked me to leave the room. Another time, I suggested I might rig the system so that I could examine all conversations anyone had had with it, say, overnight. I was promptly bombarded with accusations that what I proposed amounted to spying on people’s most intimate thoughts; clear evidence that people were conversing with the computer as if it were a person who could be appropriately and usefully addressed in intimate terms. I knew of course that people form all sorts of emotional bonds to machines, for example, to musical instruments, motorcycles and cars. And I knew from long experience that the strong emotional ties many programmers have to their computers are often formed after only short exposures to their machines. What I had not realized is that extremely short exposures to a relatively simple computer program could induce powerful delusional thinking in quite normal people."[^ComputerPower]

[^ComputerPower]: Joseph Weizenbaum, Computer Power and Human Reason, W. H. Freeman and Co, 1976, Page 6

Anthropomorphisation is part of normal human psychology[^Anthro]. But I find it hard to believe Weizenbaum’s claim that people thought they were talking to a real person. Perhaps if the conversation is limited to just a *few* exchanges, and ELIZA doesn’t make any obviously idiotic responses, you could be forgiven for assuming you were talking to a real person. But after no more than half a dozen exchanges, ELIZA will have made some kind of odd response that will raise doubts in your mind. But this was apparently not Weizenbaum’s experience. Perhaps something related to the Pygmalion effect[^PygmalionEffect] meant people’s high expectations affected their judgement, though their expectations obviously would not have affected ELIZA's performance.

[^Anthro]: "Anthropomorphism is the attribution of human traits, emotions, or intentions to non-human entities. It is considered to be an innate tendency of human psychology."<https://en.wikipedia.org/wiki/Anthropomorphism>


[^PygmalionEffect]: "The Pygmalion effect, or Rosenthal effect, is a psychological phenomenon in which high expectations lead to improved performance in a given area." <https://en.wikipedia.org/wiki/Pygmalion_effect>

I’ve had several conversations with my recreation of Weizenbaum’s ELIZA running his DOCTOR script. Often the responses remind me of the auto-generated subtitles on Youtube live-streams: they are mostly good, but sometimes the voice recognition software produces text that is jarringly wrong. You can sort of see how what the speaker said could maybe phonetically match the text, but you are reminded that the thing producing the text has no real understanding of what is being said.

The following conversation is the best I’ve had so far. I tried to approach the conversation as seriously as I could, as if I was talking to a real person, about something that genuinely matters to me. The conversation is not rehearsed or carefully planned, this is how it went. I tried to generously interpret ELIZA’s odd responses. I think the conversation is mildly amusing. She was a bit sarcastic to begin with, dropped a bombshell in the middle and didn’t want to say goodbye at the end. But she didn’t fool me. No siree.

```text
-----------------------------------------------------------------
      ELIZA -- A Computer Program for the Study of Natural
         Language Communication Between Man and Machine
            DOCTOR script (c) Joseph Weizenbaum, 1966
This implementation by Anthony Hay, 2022  (CC0 1.0) Public Domain
-----------------------------------------------------------------
ELIZA /help for usage.
Using Weizenbaum's 1966 DOCTOR script.
Enter a blank line to quit.


HOW DO YOU DO. PLEASE TELL ME YOUR PROBLEM

hi
I AM NOT SURE I UNDERSTAND YOU FULLY

I was just saying hello.
WERE YOU REALLY

So I'll just jump straight in with my problem.
YOUR PROBLEM

It's something that's on my mind every day.
WHY DO YOU SAY YOUR MIND EVERY DAY

It's very serious. It affects everybody. I think about it a lot.
REALLY, EVERYBODY

Yes. I'm talking about the climate and ecological crisis.
YOU SEEM QUITE POSITIVE

Nobody's life will be untouched by the climate crisis.
DOES THAT HAVE ANYTHING TO DO WITH THE FACT THAT YOUR PROBLEM

I'm not sure what you mean.
IS IT BECAUSE YOU ARE NOT SURE WHAT I MEAN THAT YOU CAME TO ME

Not really. I'm here because I want a serious conversation with you
 about something that matters deeply to me. 
WHAT WOULD IT MEAN TO YOU IF YOU GOT A SERIOUS CONVERSATION WITH I ABOUT
 SOMETHING THAT MATTERS DEEPLY TO YOU

It would mean I could believe what Weizenbaum said about you in his book.
WE WERE DISCUSSING YOU - NOT ME

ok
DOES THAT HAVE ANYTHING TO DO WITH THE FACT THAT YOUR MIND EVERY DAY

No. It's on my mind every day because it's such a huge problem.
ARE YOU SAYING 'NO' JUST TO BE NEGATIVE

No, I'm trying to explain how I feel about the climate crisis.
YOU ARE BEING A BIT NEGATIVE

Do you deny there is a climate crisis?
OH, I DENY THERE IS A CLIMATE CRISIS

Wow! I really didn't expect that. Haven't you seen the IPCC reports and
 heard scientists and the UN Secretary-General talking about the problem?
YOU SAY YOU REALLY DIDN'T EXPECT THAT

I didn't because I didn't expect you to voice your own beliefs so strongly.
 I thought I'd be doing all the talking.
PERHAPS IN YOUR FANTASY WE DIDN'T BECAUSE YOU DIDN'T EXPECT EACH OTHER

This isn't a fantasy. It's very real. Millions of people will die, are
 dying, becuase the world is heating.
DID YOU THINK THEY MIGHT NOT BE DYING

I know they are dying. Peer-reviewed science says so, like the Lancet
 Planetary Health report from 2021 that said 5 million people are dying
 every year because of extreme heat.
CAN YOU ELABORATE ON THAT

19 of the 20 hottest years ever recorded have occured since the year 2000.
PLEASE GO ON

The UN Secretary-General says governments and business leaders are lying
 to us, saying one thing but doing another.
WOULD YOU LIKE IT IF THEY WERE NOT LYING TO US

Absolutely. Their own scientists knew about the problem decades ago, but
 they actively hid the truth and spread false information. And they are
 still doing it.
WHAT IF THEY WERE NOT STILL DOING IT

That would change everything. Government would be less distorted by the
 powerful fossil fuel lobby and could act as if they believed their own
 speeches.
DO YOU THINK ITS LIKELY THAT THEY BELIEVED THEIR OWN SPEECHES

Words are cheap. Our Prime Minister said at COP26 it was one minute to
 midnight. A few months later he has given the goahead for new oil and
 gas fields. He is a liar.
POSSIBLY THEY ARE CHEAP

It's easy to say what you think people want to hear when you couldn't care
 less if what you are saying is true. If a gentleman's word is his bond,
 our PM is no gentleman.
DO YOU WISH THAT WHAT I ARE SAYING IS TRUE

Well, I wish what I am saying was wrong and there was no climate crisis.
HOW LONG HAVE YOU BEEN SAYING WAS WRONG AND THERE WAS NO CLIMATE CRISIS

I'm going to have to say goodbye now.
DO YOU BELIEVE IT NORMAL TO BE GOING TO HAVE TO SAY GOODBYE NOW

I hope I'm not being rude. Thank you for talking to me.
DO YOU SAY YOU HOPE YOU'RE NOT BEING RUDE FOR SOME SPECIAL REASON

You asked if I thought it was normal to say goodbye and I thought I might
 have offended you.
WHAT DO YOU THINK ABOUT YOU THOUGHT IT WAS NORMAL TO SAY GOODBYE AND YOU
 THOUGHT YOU MIGHT HAVE OFFENDED I

I think you made a rhyme out of my sentence!
DOES THAT SUGGEST ANYTHING ELSE WHICH BELONGS TO YOU

We had a good chat and I'd like you to just let me go now.
YOU'RE NOT REALLY TALKING ABOUT ME - ARE YOU

Goodbye.
LETS DISCUSS FURTHER WHY YOUR SENTENCE

```



## What’s the point?

The point was to satisfy my curiosity to know the answers to admittedly trivial questions from the opposite end of my adult life: how does ELIZA work and what would it be like to talk to ELIZA?

I was not interested in talking to *a* chatbot, or even *my* chatbot. I wanted to talk to Joseph Weizenbaum’s ELIZA.

I didn’t have an IBM 7094 running ELIZA, I only had Weizenbaum’s detailed description of how ELIZA worked from his 1966 CACM paper. I tried to create something functionally equivalent to that description. The parts where Weizenbaum was deliberately vague were frustrating. That, and the random element, meant any conversation I had with my recreation was only *potentially* the same as a conversation with the real ELIZA.

After the source code to (an early version of) ELIZA was found, *a certain counting mechanism* was no longer a mystery. And now that the source code for the hash function has been found the random element can be eliminated. Whilst there may be differences between Weizenbaum's description of ELIZA and his implementation (I noted some potential bugs in the ELIZA source [here](https://github.com/anthay/ELIZA/blob/master/doc/ELIZA_transcription_annotated.txt)), and there may be bugs in my implementation, I have some confidence that, given the same script and user input, the responses produced by my code will be the same as those produced by the original ELIZA.

This is play. An end in itself. I’m not ashamed of this.

Along the way I learned some things and met some interesting people. I’ve tried to share what I learned.

I’m aware that I’m privileged to have the space to play like this.

Anthony Hay, May 2022, Devon, England




## Acknowledgements

Thank you [Jeff Shrager](https://sites.google.com/view/jeffshrager-org/home) for finding the [original ELIZA source code](https://sites.google.com/view/elizagen-org/original-eliza) and for taking an interest in my own efforts to recreate ELIZA.

Jeff also found a group of people who share an interest in ELIZA and I’ve enjoyed talking to and learning from them

- [David Berry](https://en.wikipedia.org/wiki/David_M._Berry) ([Critical code studies 2016 - ELIZA](https://www.youtube.com/watch?v=cNBjVNU69AI))
- [Mark Marino](http://markcmarino.com/wordpress/) ([10 PRINT CHR$(205.5+RND(1)); : GOTO 10](https://mitpress.mit.edu/books/10-print-chr2055rnd1-goto-10) and [Critical code studies](https://criticalcodestudies.com/))
- [Peter Millican](https://en.wikipedia.org/wiki/Peter_Millican) ([Elizabeth](https://www.philocomp.net/ai/elizabeth.htm))
- [Arthur Schwarz](https://slipbits.com/) ([GNU Slip](https://www.gnu.org/software/gslip/))





![Black and white photo of a young woman in dark nineteenth-century style hat and coat looking straight at the camera, holding a small bouquet of flowers near her pretty smiling face.](https://github.com/anthay/ELIZA/blob/master/doc/Audrey_Hepburn_as_Eliza_Doolittle_1963.jpg)
"So deliciously low — so horribly dirty."  
Audrey Hepburn as Eliza Doolittle, by Cecil Beaton, 1963.  
Publicity for the 1964 film My Fair Lady.

THE END
