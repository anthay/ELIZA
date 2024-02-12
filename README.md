# A Simulation in C++ of Joseph Weizenbaum’s 1966 ELIZA

I’ve made in C++ what I think is an accurate simulation of the original ELIZA. It is a console application that takes as input the original format script file, which looks like a series of S-expressions, and then waits for the user to type a line of text before responding with a line of text of its own.

I made this before the ELIZA source code had been found, and wrote about it in [part 1](https://github.com/anthay/ELIZA/blob/master/doc/Eliza_part_1.md).

[Part 2](https://github.com/anthay/ELIZA/blob/master/doc/Eliza_part_2.md) describes changes I made after the ELIZA source code was found.

[Part 3](https://github.com/anthay/ELIZA/blob/master/doc/Eliza_part_3.md) is about the HASH function, now that too has been found. 

In a [footnote](https://github.com/anthay/ELIZA/blob/master/doc/Trying_to_recreate_RFC439.md) I document trying to recreate the PARRY/DOCTOR conversation from RFC439.

My son Max Hay and I recreated ELIZA in JavaScript [here](https://github.com/anthay/ELIZA/blob/master/src/eliza.html). Try it [here](https://anthay.github.io/eliza.html).

I added [serial I/O](https://github.com/anthay/ELIZA/blob/master/doc/serial_io.md) to run ELIZA on an ASR 33 teletype. 
