# ELIZA on a Teletype Model 33 ASR

Here are some notes about how I got to interact with ELIZA through an ASR 33 teletype connected to a serial port on my computer.

Joseph Wezenbaum developed ELIZA between 1964 and 1966 on an IBM 7094 under an operating system called CTSS (Compatible Time-Sharing System). Since ELIZA is an interactive program we can assume he was using some kind of computer terminal (rather than batch processing and card decks). We know he had an IBM 2741 teletypewriter at home at about this time, which prints text onto paper at 14 characters per second. We also know that the participants in the 1965 Project MAC ELIZA pilot study were talking to ELIZA using similar teletypewriters.

The Teletype Model 33 ASR was made at about the same time as the 2741 and was in widespread use. What would it be like to talk to ELIZA over one of those teletypes? Here is a [short video](https://drive.google.com/file/d/1G7_Bi9aviEWdeeQR4nbug3BbfkSEWMRw).

My father, who worked for the UK General Post Office, gave me a GPO surplus ASR 33 teletype in 1978 to use with my 77-68 home computer. The teletype sends and receives data through a serial cable using a protocol called RS-232. I replaced the worn out DB-25 male connector on this cable with a new DB-9 male connector. For this project it was sufficient to connect only DB-9 pins 2 (receive data, RXD), 3 (transmit data, TXD) and 5 (signal ground).

Note that RS-232 was designed to connect Data Terminal Equipment (DTE), such as the ASR 33 teletype or a computer, to Data Communications Equipment (DCE), such as a modem. The connector pins are named with respect to the DTE. So for example, pin 3 TXD is an output at the DTE end and an input at the DCE end. (See [wikipedia on RS-232](https://en.wikipedia.org/wiki/RS-232).) Because I want to connect two DTEs together, my ASR 33 to my computer, I need to use a null-modem cable, which swaps connections such as pins 2 and 3.

My Windows computer dates from 2008 and comes with two RS-232 COM ports built in. So using my null-modem cable I can connect the ASR 33 directly to my PC. Most modern personal computers don’t have RS-232 ports. USB to RS-232 cables are available and I’ll say more about them later.

When you press a key on the ASR 33 keyboard, the machine transmits eleven bits: 1 start bit, 7 ASCII data bits, 1 even parity bit and 2 stop bits. It transmits these at 110 baud, which means it can transmit at a maximum of 10 characters per second. It receives data at the same rate.

I ran the [PuTTY terminal emulator](https://www.chiark.greenend.org.uk/~sgtatham/putty/) on my Windows 10 computer and configured the serial settings baud=110 data=7 parity=e stop=2 for the COM port where the teletype was connected. Using this I could send characters to and receive characters from the teletype. I added code to my recreation of Weizenbaum’s ELIZA to read and write to a serial port under Windows. (Mainly [here](https://github.com/anthay/ELIZA/blob/master/src/win_serial_io.cpp).)

My teletype is a 50 year-old electromechanical wonder from a time before software ate the world and it’s good to see it run again. I’ve successfully made simple repairs to it, such as replacing the crumbling rubber pad on the typewheel hammer with a plastic tube. I also mended a snapped platen shaft. But I couldn’t get through the MEN ARE ALL ALIKE dialog without either me or the teletype mistyping something, or something seizing up.  (The connection seemed a little more reliable if I configured the serial port to send and receive 8 data bits, no parity and 2 stop bits, and zeroed the top bit in the serial I/O code.)

What if your PC doesn’t have a COM port? As I mentioned before, there are USB to RS-232 adapter cables available. I tried a StarTech ICUSB232FTN USB to RS-232 null modem cable, which contained an FTDI chip. This adapter did not work because it does not support the 110 baud rate that my ASR 33 requires. The lowest speed it supports appears to be 300 baud. I also tried a Plugable PL2303-DB9 USB to Serial Adapter cable, which contained a Prolific PL2303GT chip. This presents as DTE so I used it together with a null modem cable. This adapter does support 110 baud. Windows 10 detected the cable and downloaded and installed a suitable device driver without me having to do anything. It created a new COM port that I could use to successfully communicate with the teletype.

What about platforms other than Windows? Using the [CoolTerm terminal emulator](https://freeware.the-meiers.org/) I confirmed that my Plugable PL2303-DB9 cable will successfully connect a USB port on my Apple M1 Mac running macOS 10.14 Sonoma to my ASR 33, but only after I had installed the “[prolific_pl2303] mac usb serial adapter V2.2” device driver from this [StarTech webpage](https://www.startech.com/en-gb/cards-adapters/icusb232v2). (Yes, the drivers on the Plugable website did not support Apple silicon but there was a driver on the StarTech website that did and it worked for the Plugable adapter.)  I added code to ELIZA to support serial devices on macOS. (Mainly [here](https://github.com/anthay/ELIZA/blob/master/src/posix_serial_io.cpp).)

This project was sufficient to answer the question: can I talk to ELIZA on my ASR 33? There are other considerations that a more complete and robust solution might support. For example:

- The teletype needs the linefeed (LF) to be sent after the carriage return (CR) to give the carriage time to return to the first column. But the carriage might still be returning when the linefeed is complete, in which case the first character on the next line might be printed in the wrong place. It was common practice to send a non-printing ASCII NUL character after the linefeed to give the carriage that additional tenth of a second to get home. I found I needed to send two NULs to be sure the carriage was home. This isn’t necessary for glass teletypes, i.e. Visual Display Units. A more complete solution might make this configurable.
- When the carriage gets to the end of the line it simply stops moving right. Every character sent to it by the computer is printed in the last column on top of the previous one. I added code to automatically return the carriage to the start of the next line once column 72 is reached. This should be configurable.
- My ASR 33 has a 3-position knob marked SIMPLEX, LOCAL and DUPLEX. When in simplex mode a keypress is both printed and transmitted. When in duplex mode a keypress is transmitted but not printed. In this mode only characters received through the serial cable are printed. In my implementation I assume the teletype is in full duplex mode and echo back characters so the user can see what they have typed. This should be configurable.
- There are some RS 232 control signals I just ignored because they weren’t necessary to make this project work. They may be important in some setups.
- If you mistype a character on the teletype, what are you going to do? There is no backspace. There is a RUB OUT key that transmits ASCII DEL. This could be used to virtually delete the previous character. (Once the character is printed on the paper the ASR 33 has no means of erasing it.)

Finally, is this useful? Talking to ELIZA on a slow, noisy mechanical teletype felt like interactive performance art. I could make the machine print words on the paper. ELIZA could make the machine print words on the paper. It was evocative, emotional and exciting; an experience, a happening. (On a moderate scale.) Maybe this has helped me come a little closer to understanding how people may have felt when talking to ELIZA in the 1960s?



### POSIX (e.g. macOS)

Build and run without serial I/O

```text
clang++ -std=c++20 -pedantic -o eliza eliza.cpp
./eliza
```
Build and run with serial I/O

```text
clang++ -std=c++20 -pedantic -D SUPPORT_SERIAL_IO -o eliza eliza.cpp posix_serial_io.cpp
./eliza --port /dev/cu.PL2303G-USBtoUART10
```

(The ASR 33 should be in full duplex mode on POSIX.)

### Windows

Build and run without serial I/O

```text
cl /EHsc /W4 /std:c++20 eliza.cpp
eliza
```

Build and run with serial I/O

```text
cl /EHsc /W4 /std:c++20 /D SUPPORT_SERIAL_IO eliza.cpp win_serial_io.cpp
eliza /port COM3
```

(The ASR 33 should be in Simplex mode on Windows.)

