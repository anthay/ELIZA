# Joseph Weizenbaum’s 1966 ELIZA recreated in C++

I’ve made in C++ what I think is an accurate simulation of the original ELIZA. It is a console application that takes as input the original format script file, which looks like a series of S-expressions, and then waits for the user to type a line of text before responding with a line of text of its own.

[![A simulated blurry CRT terminal screen showing ELIZA running and the start of the famous "men are all alike" conversation.](https://github.com/anthay/ELIZA/blob/master/doc/action-shot.jpg)](https://anthay.github.io/eliza.html)

ELIZA is a chatbot—the first chatbot—written between 1964-1966 by Joseph Weizenbaum, Professor of Electrical Engineering at MIT, and part-supported by the US Department of Defence Project MAC. It is configured via a "script," the most famous of which is called DOCTOR. The script contains simple pattern matching rules that ELIZA uses to reflect back parts of the text typed in by the user. Weizenbaum's purpose was not to advance our understanding of intelligence, but to create the illusion that ELIZA understood what had been said to it in order to demonstrate how easily people can be fooled.

I made this before the ELIZA source code had been found, and wrote about it in [part 1](https://github.com/anthay/ELIZA/blob/master/doc/Eliza_part_1.md).

[Part 2](https://github.com/anthay/ELIZA/blob/master/doc/Eliza_part_2.md) describes changes I made after the ELIZA source code was found.

[Part 3](https://github.com/anthay/ELIZA/blob/master/doc/Eliza_part_3.md) is about the HASH function, now that too has been found. 

In a [footnote](https://github.com/anthay/ELIZA/blob/master/doc/Trying_to_recreate_RFC439.md) I document trying to recreate the PARRY/DOCTOR conversation from RFC439.

My son Max Hay and I recreated ELIZA in JavaScript [here](https://github.com/anthay/ELIZA/blob/master/src/eliza.html). Try it [here](https://anthay.github.io/eliza.html).

I added [serial I/O](https://github.com/anthay/ELIZA/blob/master/doc/serial_io.md) to run ELIZA on an ASR 33 teletype.

I helped show that 1966 CACM ELIZA is [Turing complete](https://sites.google.com/view/elizagen-org/blog/eliza-is-turing-complete).
There are several Turing machine ELIZA scripts [here](https://github.com/anthay/ELIZA/blob/master/scripts/scripts.md).

There is a huge collection of ELIZA-related information at Jeff Shrager's [elizagen.org](https://elizagen.org).

Along with Jeff and others I am contributing to a book about ELIZA. The website is [findingeliza.org](https://findingeliza.org).

Thanks to Rupert Lane, you can now [run the original MAD ELIZA code](https://github.com/rupertl/eliza-ctss). (Jeff's [ELIZA Reanimated paper](https://doi.org/10.48550/arXiv.2501.06707).)

---

### To build and run ELIZA

Note that the whole of ELIZA is in the one file [eliza.cpp](https://github.com/anthay/ELIZA/blob/master/src/eliza.cpp) (unless you wish to also use the serial I/O code mentioned above).

POSIX (e.g. macOS) (I used Apple clang version 15.0.0 that came with Xcode):

```text
clang++ -std=c++20 -pedantic -o eliza eliza.cpp
./eliza
```

Windows (I used Microsoft Visual Studio 2019 Community Edition Command Prompt):

```text
cl /EHsc /W4 /std:c++20 eliza.cpp
eliza
```
