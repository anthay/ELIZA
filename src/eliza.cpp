/*  This is a simulation of Joseph Weizenbaum's 1966 ELIZA. The code was
    written from scratch using only the description in the article on page
    36 of the January 1966 edition of Communications of the ACM as a guide.
    If given the same S-expression-like script from the appendix of that
    article, and given the same prompt text, the conversation it generates
    is identical to the conversation shown in that article.

    I made this for my amusement and hereby place it in the public domain
    or, if you prefer, I release it under either CC0 1.0 Universal or the
    MIT License.
    -- Anthony Hay, 2021, Devon, UK

    Update: In April 2021 Jeff Shrager obtained a listing of ELIZA from
    Weizenbaum's MIT archive. In May 2021 he got permission from
    Weizenbaum's estate to open source the code under CC0 1.0. Seeing
    his original code clarified some behavior that was not fully
    documented in the CACM paper. (Although the listing was not dated
    and appears to differ from the functionality described in the CACM
    paper.) I made changes to this code to reflect what I learned. These
    changes are documented in the code.
    -- Anthony Hay, 2022, Devon, UK

    Update: In April 2022 Jeff Shrager located the source code to SLIP,
    including the HASH function. I made hash used in this code use the
    same algorithm.


    Note: In the code below there are occasional references to
    Joseph Weizenbaum's 1966 CACM article. A reference like
    [page X (Y)] refers to paragraph Y on page X of that publication.
*/


#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <chrono>
#include <ctime>
#include <cassert>
#include <vector>
#include <memory>
#include <map>
#include <chrono>
#include <algorithm>
#include <deque>
#include <cctype>
#include <thread>
#include <array>
#include <cstdint>



// stringlist provides enough of the functionality present
// in Wiezenbaum's SLIP to implement ELIZA
typedef std::deque<std::string> stringlist;

// (needed for unit test purposes only)
std::ostream& operator<<(std::ostream & os, const stringlist & list)
{
    os << '(';
    for (auto s : list)
        os << '"' << s << '"' << ' ';
    os << ')';
    return os;
}



namespace micro_test_library {

unsigned test_count;      // total number of tests executed
unsigned fault_count;     // total number of tests that fail


// write a message to std::cout if !(value == expected_value)
template<typename A, typename B>
void test_equal(const A & value, const B & expected_value,
    const char * filename, const size_t linenum, const char * functionname)
{
    ++test_count;
    if (!(value == expected_value)) {
        ++fault_count;
        // e.g. love.cpp(2021) : in proposal() expected 'Yes!', but got 'Hahaha'
        std::cout
            << filename << '(' << linenum
            << ") : in " << functionname
            << "() expected '" << expected_value
            << "', but got '" << value
            << "'\n";
    }
}

#define TEST_EQUAL(value, expected_value)                   \
{                                                           \
    micro_test_library::test_equal(value, expected_value,   \
        __FILE__, __LINE__, __FUNCTION__);                  \
}


std::vector<void (*)()> test_routines;

size_t add_test(void (*f)())
{
    test_routines.push_back(f);
    return test_routines.size();
}

#define DEF_TEST_FUNC(test_func)                                   \
void test_func();                                                        \
size_t micro_test_##test_func = micro_test_library::add_test(test_func); \
void test_func()

void run_tests()
{
    for (auto t : test_routines)
        t();
}
#define RUN_TESTS() micro_test_library::run_tests()

} //namespace micro_test_library






// remove front element of given 'container' and return it
template<typename T>
auto pop_front(T & container)
{
    auto v(container.front());
    container.pop_front();
    return v;
}

template<>
auto pop_front(std::string & container)
{
    auto v(container.front());
    container.erase(0, 1);
    return v;
}


// join given words into one space separated string
// e.g. join(["one", "two", ",", "3", "."]) -> "one two , 3 ."
// (ELIZA doesn't output punctuation)
std::string join(const stringlist & words)
{
    std::string result;
    for (const auto & word : words) {
        if (!word.empty()) {
            if (!result.empty())
                result += ' ';
            result += word;
        }
    }
    return result;
}

DEF_TEST_FUNC(join_test)
{
    TEST_EQUAL(join(stringlist{ "one", "", "two", ",", "3", "." }), "one two , 3 .");
}



//////// //       //// ////////    ///    //        ///////   //////   ////  //////  
//       //        //       //    // //   //       //     // //    //   //  //    // 
//       //        //      //    //   //  //       //     // //         //  //       
//////   //        //     //    //     // //       //     // //   ////  //  //       
//       //        //    //     ///////// //       //     // //    //   //  //       
//       //        //   //      //     // //       //     // //    //   //  //    // 
//////// //////// //// //////// //     // ////////  ///////   //////   ////  //////  


namespace elizalogic { // the core ELIZA algorithm

// map tag -> associated words, e.g. "BELIEF" -> ("BELIEVE" "FEEL" "THINK" "WISH")
typedef std::map<std::string, stringlist> tagmap;


constexpr unsigned char hollerith_undefined = 0xFFu; // (must be > 63)

// This table maps ordinary character code units to their Hollerith
// encoding, or hollerith_undefined if that character does not exist
// in the Hollerith character set.
const std::array<unsigned char, 256> hollerith_encoding{ []{

    /*  "The 7090 BCD character codes are given in the accompanying table.
        Six bits are used for each character. [...] The code is generally
        termed binary-coded-decimal or BCD. For compactness, the codes are
        generally expressed as 2-digit octal numbers, as in the table. The
        term Hollerith is used synonomously with BCD." [1]

        The following array is derrived from the above mentioned table, with
        one exception: BCD code 14 (octal) is a single quote (prime), not a
        double quote. See [2].

        The Hollerith code is the table offset. 0 means unused code.

        [1] Philip M. Sherman
            Programming and Coding the IBM 709-7090-7094 Computers
            John Wiley and Sons, 1963
            Page 62
        [2] University of Michigan Executive System for the IBM 7090 Computer
            September 1964
            In section THE UNIVERSITY OF MICHIGAN MONITOR
            APPENDIX 2, page 30, TABLE OF BCD--OCTAL EQUIVALENTS
            (Available online from Google Books. Search for PRIME.)

    */
    static constexpr unsigned char bcd[64] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 0, '=', '\'', 0, 0, 0,
        '+', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 0, '.', ')',  0, 0, 0,
        '-', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 0, '$', '*',  0, 0, 0,
        ' ', '/', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 0, ',', '(',  0, 0, 0
    };

    static_assert(std::numeric_limits<unsigned char>::max() == 255);
    std::array<unsigned char, 256> to_bcd;
    to_bcd.fill(hollerith_undefined);
    for (unsigned char c = 0; c < 64; ++c)
        if (bcd[c])
            to_bcd[bcd[c]] = c;
    return to_bcd;
}() };


// return true iff given c is in the Hollerith character set
bool hollerith_defined(char c)
{
    static_assert(std::numeric_limits<unsigned char>::min() == 0);
    static_assert(std::numeric_limits<unsigned char>::max() == 255);

    return hollerith_encoding[static_cast<unsigned char>(c)] != hollerith_undefined;
}


// return given string s with non-Hollerith characters replaced
// by space, and '?' and '!' replaced by '.'
std::string filter_bcd(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
        [](char c) {
            if (c == '?' || c == '!')
                return '.';
            return hollerith_defined(c) ? c : ' ';
        }
    );
    return s;
}

DEF_TEST_FUNC(filter_bcd_test)
{
    TEST_EQUAL(filter_bcd(""), "");
    TEST_EQUAL(filter_bcd("HELLO"), "HELLO");
    TEST_EQUAL(filter_bcd("Hello! How are you?"), "H    . H          .");
    const std::string all_valid_bcd{
        "0123456789=\'+ABCDEFGHI.)-JKLMNOPQR$* /STUVWXYZ,("
    };
    TEST_EQUAL(filter_bcd(all_valid_bcd), all_valid_bcd);
}



bool punctuation(char c)
{
    return c == ',' || c == '.'; // [page 37 (c)]
}


bool delimeter(const std::string & s)
{
    // In the 1966 CACM artical on page 37 Weizenbaum says "the procedure
    // recognizes a comma or a period as a delimiter." However, in the
    // MAD-Slip source code the relavent code is
    //    W'R WORD .E. $.$ .OR. WORD .E. $,$ .OR. WORD .E. $BUT$
    // (W'R means WHENEVER). So "BUT" is also a delimiter.
    return s == "BUT" || (s.size() == 1 && punctuation(s[0]));
}


// split given string s into a list of "words"; punctuation are words
// e.g. split("one   two, three.") -> ["one", "two", ",", "three", "."]
stringlist split(std::string s)
{
    stringlist result;
    std::string word;
    for (auto ch : s) {
        if (punctuation(ch) || ch == ' ') {
            if (!word.empty()) {
                result.push_back(word);
                word.clear();
            }
            if (ch != ' ')
                result.push_back(std::string(1, ch));
        }
        else
            word.push_back(ch);
    }
    if (!word.empty())
        result.push_back(word);
    return result;
}

DEF_TEST_FUNC(split_test)
{
    const stringlist r1{ "one", "two", ",", "three", "." };
    TEST_EQUAL(split("one   two, three."), r1);
}



// return given string 's' in uppercase
std::string to_upper(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); }
    );
    return s;
}


// return numeric value of given 's' or -1, e.g. to_int("2") -> 2, to_int("two") -> -1
int to_int(const std::string & s)
{
    int result = 0;
    for (auto c : s) {
        if (std::isdigit(c))
            result = 10 * result + c - '0';
        else
            return -1;
    }
    return result;
}


// e.g. inlist("DEPRESSED", "(*SAD HAPPY DEPRESSED)") -> true
// e.g. inlist("FATHER", "(/FAMILY)") -> true (assuming tags("FAMILY") -> "... FATHER ...")
bool inlist(const std::string & word, std::string wordlist, const tagmap & tags)
{
    if (wordlist.back() == ')')
        wordlist.pop_back();
    const char * cp = wordlist.data();
    if (*cp == '(')
        ++cp;
    stringlist s;
    if (*cp == '*') {
        ++cp;
        while (*cp == ' ')
            ++cp;
        s = split(std::string(cp));
    }
    else if (*cp == '/') {
        ++cp;
        while (*cp == ' ')
            ++cp;
        const auto t = tags.find(std::string(cp));
        if (t != tags.end())
            s = t->second;
    }
    return std::find(s.begin(), s.end(), word) != s.end();
}


/*  return true iff 'words' match 'pattern'; if they match, 'matching_components'
    are the actual matched words, one for each element of 'pattern'

    e.g. match(tags, [0, YOU, (* WANT NEED), 0], [YOU, NEED, NICE, FOOD], mc) -> true
      with mc = [<empty>, YOU, NEED, NICE FOOD]

    Note that grouped words in pattern, such as (* WANT NEED), must be presented
    as a single stringlist entry.
*/
bool match(const tagmap & tags, stringlist pattern, stringlist words, stringlist & matching_components)
{
    matching_components.clear();
    if (pattern.empty())
        return words.empty();

    auto patword = pop_front(pattern);
    int n = to_int(patword);
    if (n < 0) { // patword is e.g. "ARE" or "(*SAD HAPPY DEPRESSED)"
        if (words.empty())
            return false; // patword cannot match nothing
        std::string current_word = pop_front(words);
        if (patword.front() == '(') {
            // patword is a group, is current_word in that group?
            if (!inlist(current_word, patword, tags))
                return false;
        }
        else if (patword != current_word)
            return false; // patword is a single word and it doesn't match

        // so far so good; can we match remainder of pattern with remainder of words?
        stringlist mc;
        if (match(tags, pattern, words, mc)) {
            matching_components.push_back(current_word);
            matching_components.insert(matching_components.end(), mc.begin(), mc.end());
            return true;
        }
    }
    else if (n == 0) { // 0 matches zero or more of any words
        stringlist component;
        stringlist mc;
        for (;;) {
            if (match(tags, pattern, words, mc)) {
                matching_components.push_back(join(component));
                matching_components.insert(matching_components.end(), mc.begin(), mc.end());
                return true;
            }

            if (words.empty())
                return false;

            component.push_back(pop_front(words));
        }
    }
    else { // match exactly n of any words [page 38 (a)]
        if (words.size() < static_cast<size_t>(n))
            return false;
        stringlist component;
        for (int i = 0; i < n; ++i)
            component.push_back(pop_front(words));
        stringlist mc;
        if (match(tags, pattern, words, mc)) {
            matching_components.push_back(join(component));
            matching_components.insert(matching_components.end(), mc.begin(), mc.end());
            return true;
        }
    }
    return false;
}

DEF_TEST_FUNC(match_test)
{
    // test [0, YOU, (*WANT NEED), 0] matches [YOU, NEED, NICE, FOOD]
    stringlist words{ "YOU", "NEED", "NICE", "FOOD" };
    stringlist pattern{ "0", "YOU", "(*WANT NEED)", "0" };
    stringlist expected{ "", "YOU", "NEED", "NICE FOOD" };
    stringlist matching_components;
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    // test [0, 0, YOU, (*WANT NEED), 0] matches [YOU, WANT, NICE, FOOD]
    words = { "YOU", "WANT", "NICE", "FOOD" };
    pattern = { "0", "0", "YOU", "(*WANT NEED)", "0" };
    expected = {"", "", "YOU", "WANT", "NICE FOOD" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    // test [1, (*WANT NEED), 0] matches [YOU, WANT, NICE, FOOD]
    words = { "YOU", "WANT", "NICE", "FOOD" };
    pattern = { "1", "(*WANT NEED)", "0" };
    expected = { "YOU", "WANT", "NICE FOOD" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    // test [1, (*WANT NEED), 1] doesn't match [YOU, WANT, NICE, FOOD]
    words = { "YOU", "WANT", "NICE", "FOOD" };
    pattern = { "1", "(*WANT NEED)", "1" };
    TEST_EQUAL(match({}, pattern, words, matching_components), false);

    // test [1, (*WANT NEED), 2] matches [YOU, WANT, NICE, FOOD]
    words = { "YOU", "WANT", "NICE", "FOOD" };
    pattern = { "1", "(*WANT NEED)", "2" };
    expected = { "YOU", "WANT", "NICE FOOD" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    // A test pattern from the YMATCH function description in the SLIP manual
    words = { "MARY", "HAD", "A", "LITTLE", "LAMB", "ITS", "PROBABILITY", "WAS", "ZERO" };
    pattern = { "MARY", "2", "2", "ITS", "1", "0" };
    expected = { "MARY", "HAD A", "LITTLE LAMB", "ITS", "PROBABILITY", "WAS ZERO" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);
}



// return words constructed from given 'reassembly_rule' and 'components'
// e.g. reassemble([ARE, YOU, 1], [MAD, ABOUT YOU]) -> [ARE, YOU, MAD]
stringlist reassemble(const stringlist & reassembly_rule, const stringlist & components)
{
    stringlist result;
    for (const auto & r : reassembly_rule) {
        int n = to_int(r);
        if (n < 0)
            result.push_back(r);
        else if (n == 0 || static_cast<size_t>(n) > components.size())
            result.push_back("SCRIPT-ERROR-REASSEMBLY-RULE-INDEX-OUT-OF-RANGE");
        else {
            stringlist expanded = split(components[n - 1]);
            result.insert(result.end(), expanded.begin(), expanded.end());
        }
    }
    return result;
}

DEF_TEST_FUNC(reassemble_test)
{
    // A test pattern from the ASSMBL function description in the SLIP manual
    // (using above matching_components list)
    stringlist matching_components {
        "MARY", "HAD A", "LITTLE LAMB", "ITS", "PROBABILITY", "WAS ZERO"
    };
    stringlist reassembly_rule{ "DID", "1", "HAVE", "A", "3" };
    stringlist expected { "DID", "MARY", "HAVE", "A", "LITTLE", "LAMB" };
    TEST_EQUAL(reassemble(reassembly_rule, matching_components), expected);
}





/*
    "N1 : HASH.(D,N2)

    This function classifies D according to a pseudo-random scheme. The number
    of classes is 2^N2 where N2 is the second argument of the function. The
    value of the function is a pseudo-random number N1 in the range of
    0 to 2^N2-1."
    -- Documentation from University of Michigan Executive System for the
       IBM 7090, SLIP section, page 30


    The FORTRAN Assembly Program implementation of Slip HASH from JW's
    MIT archive, with my comments in lowercase (I'm using 'N' to refer
    to the second HASH parameter rather than 'N2' in the above documentation
    because the latter is confusing):

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

    There is documentation on FAP in
        Philip M. Sherman
        Programming and Coding the IBM 709-7090-7094 Computers
        John Wiley and Sons, 1963
        (Web search for ibm709.pdf)
    and
        University of Michigan Executive System for the IBM 7090 Computer
        September 1964
        In section THE 'UNIVERSITY OF MICHIGAN ASSEMBLY PROGRAM' ('UMAP')
        (Available online from Google Books.)

    The mnemonics used in HASH
        ANA = and to accumulator
        ARS = accumulator right shift
        CLA = clear AC and add (AC is the accumulator)
        LDQ = load MQ (MQ is the multiplier-quotient register)
        LLS = long left shift (AC/MQ shifted as if they were one register)
        MPY = multiply bits 1-35 of MQ with bits 1-35 of specified value
              to give a 72-bit result in AC/MQ and overflow bits P and Q
        PZE = prefix of plus zero (assembles a single machine word with a
              plus zero as its prefix)
        STA = store address; copy bits 21-35 in the AC (the 15 least-
              significant bits) to the specified location, leaving the
              other bits in the destination unchanged
        TRA = transfer to specified address (aka jump)
        ZAC = zero the accumulator

    Bits in an IBM 7090 instruction word are labeled
        S 1 2 3 4 5 6 7 8 9 10 11 12 .. 20 21 22 23 .. 33 34 35
        <--- operation code ---->          <---- address ----->
    Where S is the sign bit and bit 1 is the most-significant data bit

    FAP lines have the format

            <op>    <address>,<tag>,<decrement>

    In the <address> field, * means "present location," so STA *+2 means
    store accumulator to present location + 2.

    Also in the <address> field ** means value provided at run time.
    i.e. HASH uses self-modifying code.

    An <op> mnemonic followed by an asterisk indicates indirect addressing
    is to be used. The <tag> field specifies which of the 7090's three index
    registers, numbered 1, 2 and 4, is to be used. In this code it looks
    like index register 4 is some kind of stack frame pointer, so the
    parameters D and N and the return address are at offsets 1, 2 and 3
    respectively to the address stored in this index register.

    One thing to note: the datum may represent 6 6-bit characters,
    i.e. 36 bits. But the top bit of MQ and AC is the sign bit. So the
    top bit of the first character in the given D will be assumed to
    be a sign bit and will not be part of the 35-bit multiplication,
    except as a sign. The value of the HASH function is taken from the
    bits near the middle of D squared, so while N is small the loss
    of the top bit has no effect on the result.
*/

// recreate the Slip HASH function: return an n-bit hash value for
// the given 36-bit datum d, for values of n in range 0..15
int hash(uint_least64_t d, int n)
{
    /*  This code implements the Slip HASH algorithm from the FAP
        code shown above.

        The function returns the middle n bits of d squared.
        This kind of hash is known as mid-square.

        The IBM 7094 uses sign-magnitude representation of integers:
        in a 36-bit integer, the most-significant bit is assumed to
        be the sign of the integer, and the least-significant 35-bits
        are assumed to be the magnitude of the integer. Therefore,
        in the SLIP HASH implementation only the least-significant
        35-bits of D are squared. When the datum holds six 6-bit
        characters the top bit of the first character in the given D
        will be assumed to be a sign bit and will not be part of
        the 35-bit multiplication (except as a sign).

        On the IBM 7094 multiplying two 35-bit numbers produces a
        70-bit result. In this code that 70-bit result will be
        truncated to 64-bits. (Unsigned arithmetic overflow is not
        undefined behaviour, as it is for signed arithmetic.) If n
        is 15, the middle 15 bits of a 70-bit number are bits 42-28
        (bit 0 least significant), which is well within our 64-bit
        calculation. */
    assert(0 <= n && n <= 15);

    d &= 0x7FFFFFFFFull;        // clear the "sign" bit
    d *= d;                     // square it
    d >>= 35 - n / 2;           // move middle n bits to least sig. bits
    return d & (1ull << n) - 1; // mask off all but n least sig. bits
}

DEF_TEST_FUNC(hash_test)
{
    // The four real-world test cases

    /* 1. Weizenbaum stated this on page 38 of the 1966 CACM paper
    
            "As a particular key list structure is read the keyword K at its
            top is randomized (hashed) by a procedure that produces (currently)
            a 7 bit integer "i". The word "always", for example, yeilds the
            integer 14."

        text                                     "ALWAYS"
        left-justified, space padded to 6 chars  "ALWAYS"
        Hollerith encoded (octal)                21 43 66 21 70 62
        Test is                                  HASH(0214366217062, 7) == 14
        0214366217062 (octal) =                  0x463D91E32 (hexadecimal)
        input                                    0x463D91E32
        zero bit 35 (NB: bit 35 is already 0)    0x463D91E32
        squared                                  0x1345BA970EE053C1C4
        shift left by N / 2 bits (3 bits)        0x9A2DD4B877029E0E20
        discard the least significant 35 bits    0x1345BA970E
        least dsignificant N bits (7 bits)       0xE
    */
    TEST_EQUAL(hash(0214366217062ull, 7), 14ull);

    /* 2. From published conversation in the CACM paper

        In the CACM published script, the MEMORY S-expression is

            (MEMORY MY
                (0 YOUR 0 = LETS DISCUSS FURTHER WHY YOUR 3)
                (0 YOUR 0 = EARLIER YOU SAID YOUR 3)
                (0 YOUR 0 = BUT YOUR 3)
                (0 YOUR 0 = DOES THAT HAVE ANYTHING TO DO WITH THE FACT THAT YOUR 3))

        In the CACM published conversation, the memory is formed
        by the input sentence

            "Well, my boyfriend made me come here."

        In the Eliza code, these lines choose the MEMORY rule

            OR W'R KEYWRD .E. MEMORY                                     001220
             I=HASH.(BOT.(INPUT),2)+1                                    001230
             NEWBOT.(REGEL.(MYTRAN(I),INPUT,LIST.(MINE)),MYLIST)         001240

        That's the HASH of the BOT (last cell) of the user's INPUT sentence,
        with a 2-bit result. MYTRAN (containing the 4 MEMORY rules) is indexed
        on the value returned by HASH plus 1.

        Later, Eliza says

            "DOES THAT HAVE ANYTHING TO DO WITH THE FACT THAT YOUR BOYFRIEND MADE YOU COME HERE"

        So, the value returned by HASH in this case must have been 3 in order
        to select the correct rule...

            (0 YOUR 0 = DOES THAT HAVE ANYTHING TO DO WITH THE FACT THAT YOUR 3)

        Hence this test: HASH of "HERE" must be 3.

        text                                     "HERE"
        left-justified, space padded to 6 chars  "HERE  "
        Hollerith encoded (octal)                30 25 51 25 60 60
        Test is                                  HASH(0302551256060, 2) == 3
        0302551256060 (octal) =                  0x615A55C30 (hexadecimal)
        zero bit 35 (NB: bit 35 is already 0)    0x615A55C30
        squared                                  0x250594DE2FD7128900
        shift left by N / 2 bits(1 bit)          0x4A0B29BC5FAE251200
        discard the least significant 35 bits    0x94165378B
        least significant N bits(2 bits)         0x3
    */
    TEST_EQUAL(hash(0302551256060ull, 2), 3ull);

    /* 3. Assumed from unpublished conversation dated 5 March 1965

        In an unpublished conversation, marked 5 March 1965, there are two memories
        recalled for Eliza's responses

            "EARLIER YOU SAID YOUR WIFE WANTS KIDS"
        and
            "LETS DISCUSS FURTHER WHY YOUR FATHER TALKS ABOUT GRANDCHILDREN ALL THE TIME"

        The next two tests come from these via an analysis similar to the
        above for "HERE", plus some assumptions that the script used to
        generate these responses, which we don't have, is very similar to
        the published script.

        text                                     "KIDS"
        left-justified, space padded to 6 chars  "KIDS  "
        Hollerith encoded (octal)                42 31 24 62 60 60
        Test is                                  HASH(0423124626060, 2) == 1
        0423124626060 (octal) =                  0x899532C30 (hexadecimal)
        zero bit 35                              0x99532C30
        squared                                  0x5BD485D70EC08900
        shift left by N / 2 bits(1 bit)          0xB7A90BAE1D811200
        discard the least significant 35 bits    0x16F52175
        least significant N bits(2 bits)         0x1

        text                                     "TIME"
        left-justified, space padded to 6 chars  "TIME  "
        Hollerith encoded(octal)                 63 31 44 25 60 60
        Test is                                  HASH(0633144256060, 2) == 0
        0633144256060 (octal) =                  0xCD9915C30 (hexadecimal)
        zero bit 35                              0x4D9915C30
        squared                                  0x178572A252EF928900
        shift left by N / 2 bits (1 bit)         0x2F0AE544A5DF251200
        discard the least significant 35 bits    0x5E15CA894
        least dsignificant N bits (2 bits)       0
    */
    TEST_EQUAL(hash(0423124626060ull, 2), 1ull);
    TEST_EQUAL(hash(0633144256060ull, 2), 0ull);


    // The rest are made up

    // HASH(0777777777777, 7)
    // input                                   0xFFFFFFFFF (36 significant bits)
    // zero bit 35                             0x7FFFFFFFF (35 significant bits)
    // squared                                 0x3FFFFFFFF000000001 (70 significant bits)
    // shift left by N / 2 bits (3 bits)       0x1FFFFFFFF8000000008 (73 significant bits)
    // discard the least significant 35 bits   0x3FFFFFFFF0 (38 significant bits)
    // least significant N bits (7 bits)       0x70
    TEST_EQUAL(hash(0777777777777ull, 7), 0x70ull);

    // HASH(0777777777777, 15)
    // input                                   0xFFFFFFFFF
    // zero bit 35                             0x7FFFFFFFF
    // squared                                 0x3FFFFFFFF000000001
    // shift left by N / 2 bits (7 bits)       0X1FFFFFFFF80000000080
    // discard the least significant 35 bits   0x3FFFFFFFF00
    // least significant N bits (15 bits)      0x7F00
    TEST_EQUAL(hash(0777777777777ull, 15), 0x7F00ull);

    // HASH(0x555555555, 15)
    // input                                   0x555555555
    // zero bit 35                             0x555555555
    // squared                                 0x1C71C71C6E38E38E39
    // shift left by N / 2 bits (7 bits)       0xE38E38E371C71C71C80
    // discard the least significant 35 bits   0x1C71C71C6E3
    // least significant N bits (15 bits)      0x46E3
    TEST_EQUAL(hash(0x555555555ull, 15), 0x46E3ull);

    // HASH(0xF0F0F0F0F, 15)
    // input                                   0xF0F0F0F0F
    // zero bit 35                             0x70F0F0F0F
    // squared                                 0x31D3B5977886A4C2E1
    // shift left by N / 2 bits (7 bits)       0x18E9DACBBC4352617080
    // discard the least significant 35 bits   0x31D3B597788
    // least significant N bits (15 bits)      0x7788
    TEST_EQUAL(hash(0xF0F0F0F0Full, 15), 0x7788ull);

    // HASH("ALWAYS", 15) = HASH(0214366217062, 15)
    // input                                   0x463D91E32
    // zero bit 35 (NB: bit 35 is already 0)   0x463D91E32
    // squared                                 0x1345BA970EE053C1C4
    // shift left by N / 2 bits (7 bits)       0x9A2DD4B877029E0E200
    // discard the least significant 35 bits   0x1345BA970EE
    // least dsignificant N bits (15 bits)     0x70EE
    TEST_EQUAL(hash(0214366217062ull, 15), 0x70EEull);

    // HASH("TIME  ", 15) = HASH(0633144256060, 15)
    // input                                   0xCD9915C30
    // zero bit 35                             0x4D9915C30
    // squared                                 0x178572A252EF928900
    // shift left by N / 2 bits (7 bit)        0xBC2B9512977C9448000
    // discard the least significant 35 bits   0x178572A252E
    // least dsignificant N bits (15 bits)     0x252E
    TEST_EQUAL(hash(0633144256060ull, 15), 0x252Eull);

    TEST_EQUAL(hash(0ull, 7), 0);
}





/*  last_chunk_as_bcd() -- What the heck?

    Very quick overview:

    Eliza was written in Slip for an IBM 7094. The character encoding
    used on the 7094 is called Hollerith. The Hollerith encoding
    uses 6 bits per character. The IBM 7094 machine word size is
    36-bits.

    Slip stores strings in Slip cells. A Slip cell consists of two
    adjacent machine words. The first word contains some type bits
    and two addresses, one pointing to the previous Slip cell and
    the other pointing to the next Slip cell. (The IBM 7094 had a
    32,768 word core store, so only 15 bits are required for an
    address. So two addresses fit into one 36-bit word with 6 bits
    spare.) The second word may carry the "datum." This is where
    the characters are stored.
    
    Each Slip cell can store up to 6 6-bit Hollerith characters.

    If a string has fewer than 6 characters, the string is stored left-
    justified and space padded to the right.

    So for example, the string "HERE" would be storred in one Slip cell,
    which would have the octal value 30 25 51 25 60 60.

    If a string has more than 6 characters, it is storred in successive
    Slip cells. Each cell except the last has the sign bit set in the
    first word to indicated the string is continued in the next cell.

    So the word "INVENTED" would be storred in two Slip cells, "INVENT"
    in the first and "ED    " in the second.

    In Eliza, the user's input text is read into a Slip list, each word
    in the sentence is in it's own cell, unless a word needs to be
    continued in the next cell because it's more than 6 characters long.

    When Eliza chooses a MEMORY rule it hashes the last cell in the
    input sentence. That will be the last word in the sentence, or
    the last chunk of the last word, if the last word is more than
    6 characters long.

    This code doesn't use Slip cells. A std::deque of std::string
    provided enough functionality to manage without Slip. In this
    code, every word is contained in one std::string, no matter
    how long.

    Given the last word in a sentence, the last_chunk_as_bcd function
    will return the 36-bit Hollerith encoding of the word, appropriately
    space padded, or the last chunk of the word if over 6 characters long.
*/
uint_least64_t last_chunk_as_bcd(std::string s)
{
    uint_least64_t result = 0;

    auto append = [&](char c) {
        result <<= 6;
        result |= hollerith_encoding[static_cast<unsigned char>(c)];
    };

    int count = 0;
    if (!s.empty()) {
        for (auto c = std::next(s.begin(), ((s.length() - 1) / 6) * 6);
                c != std::end(s); ++c, ++count) {
            append(*c);
        }
    }
    while (count++ < 6)
        append(' ');

    return result;
}

DEF_TEST_FUNC(last_chunk_as_bcd_test)
{
                                                //  _ _ _ _ _ _
    TEST_EQUAL(last_chunk_as_bcd(""),             0606060606060ull);
                                                //  _ _ _ _ _ _
    TEST_EQUAL(last_chunk_as_bcd("X"),            0676060606060ull);
                                                //  H E R E _ _
    TEST_EQUAL(last_chunk_as_bcd("HERE"),         0302551256060ull);
                                                //  A L W A Y S
    TEST_EQUAL(last_chunk_as_bcd("ALWAYS"),       0214366217062ull);
                                                //  E D _ _ _ _
    TEST_EQUAL(last_chunk_as_bcd("INVENTED"),     0252460606060ull);
                                                //  A B C D E F
    TEST_EQUAL(last_chunk_as_bcd("123456ABCDEF"), 0212223242526ull);
}




/*  This is my understanding of the ELIZA script file from the explanation
    in the CACM article and observation of the example script given in the
    APPENDIX to the CACM article:

    The script is a series of S-expressions. The first S-expression is a list
    of words that ELIZA will emit at startup [page 42 (a)]. This is followed
    by the atom START. Following that are the pattern-matching and message
    assembly Rules. Finally, an empty list.

    e.g.
        (HOW DO YOU DO.  PLEASE TELL ME YOUR PROBLEM)
        START
        <Rules>
        ()
    
    Each <Rule> has one of the following six forms:

    R1. Plain vanilla transformation rule. [page 38 (a)]
        (keyword [= keyword_substitution] [precedence]
            [ ((decomposition_pattern) (reassembly_rule) (reassembly_rule) ... )
               (decomposition_pattern) (reassembly_rule) (reassembly_rule) ... )
               :
               (decomposition_pattern) (reassembly_rule) (reassembly_rule) ... )) ] )
      e.g.
        (MY = YOUR 2
            ((0 YOUR 0 (/FAMILY) 0)
                (TELL ME MORE ABOUT YOUR FAMILY)
                (WHO ELSE IN YOUR FAMILY 5)
                (=WHAT)
                (WHAT ELSE COMES TO MIND WHEN YOU THINK OF YOUR 4))
            ((0 YOUR 0 (*SAD UNHAPPY DEPRESSED SICK ) 0)
                (CAN YOU EXPLAIN WHAT MADE YOU 5))
            ((0)
                (NEWKEY)))


    R2. Simple word substitution with no further transformation rules. [page 39 (a)]
        (keyword = keyword_substitution)
      e.g.
        (DONT = DON'T)
        (ME = YOU)


    R3. Allow words to be given tags, with optional word substitution. [page 41 (j)]
        (keyword [= keyword_substitution]
            DLIST (/ <word> ... <word>))
      e.g.
            (FEEL               DLIST(/BELIEF))
            (MOTHER             DLIST(/NOUN FAMILY))
            (MOM = MOTHER       DLIST(/ FAMILY))


    R4. Link to another keyword transformation rule. [page 40 (c)]
        (keyword [= keyword_substitution] [precedence]
            (= equivalence_class))
      e.g.
            (HOW                (=WHAT))
            (WERE = WAS         (=WAS))
            (DREAMED = DREAMT 4 (=DREAMT))
            (ALIKE 10           (=DIT))


    R5. As for R4 but allow pre-transformation before link. [page 40 (f)]
        (keyword [= keyword_substitution]
            ((decomposition_pattern)
                (PRE (reassembly_rule) (=equivalence_class))))
      e.g.
        (YOU'RE = I'M
            ((0 I'M 0)
                (PRE (I ARE 3) (=YOU))))


    R6. Rule to 'pre-record' responses for later use. [page 41 (f)]
        (MEMORY keyword
            (decomposition_pattern_1 = reassembly_rule_1)
            (decomposition_pattern_2 = reassembly_rule_2)
            (decomposition_pattern_3 = reassembly_rule_3)
            (decomposition_pattern_4 = reassembly_rule_4))
      e.g.
        (MEMORY MY
            (0 YOUR 0 = LETS DISCUSS FURTHER WHY YOUR 3)
            (0 YOUR 0 = EARLIER YOU SAID YOUR 3)
            (0 YOUR 0 = BUT YOUR 3)
            (0 YOUR 0 = DOES THAT HAVE ANYTHING TO DO WITH THE FACT THAT YOUR 3))


    In addition, there must be a NONE rule with the same form as R1. [page 41 (d)]
        (NONE
            ((0)
                (reassembly_rule)
                (reassembly_rule)
                :
                (reassembly_rule)) )
      e.g.
        (NONE
            ((0)
                (I AM NOT SURE I UNDERSTAND YOU FULLY)
                (PLEASE GO ON)
                (WHAT DOES THAT SUGGEST TO YOU)
                (DO YOU FEEL STRONGLY ABOUT DISCUSSING SUCH THINGS)))


    Each rule type may perform some sort of transformation on a
    suitable user input sentence. The ELIZA script is just a collection
    of these rules. Rules are generally triggered by the appearance of
    a particular keyword in the user input text. So one way to model
    the script is to std::map the keyword to the associated rule.

    In this implementation of ELIZA, the high-level core algorithm is
    encoded in the function eliza(), the transformation of the user's
    text into ELIZA's response is via polymorphic objects derived from
    rule_base, and the script is conveniently represented by a map of
    keywords to these rule_base objects.
 */


// interface and data shared by all rules
class rule_base {
public:
    rule_base() {}

    rule_base(const std::string & keyword, const std::string & word_substitution, int precedence)
        : keyword_(keyword), word_substitution_(word_substitution), precedence_(precedence)
    {}

    virtual ~rule_base() {}


    void add_transformation_rule(const stringlist & decomposition,
        const std::vector<stringlist> & reassembly_rules)
    {
        trans_.push_back(transform(decomposition, reassembly_rules));
    }

    int precedence() const { return precedence_; }
    std::string keyword() const { return keyword_; }


    enum action {
        inapplicable,   // no transformation could be performed
        complete,       // transformation of input is complete
        newkey,         // request caller try next keyword in keystack
        linkkey         // request caller try returned keyword
    };

    // replace 'word' with substitute specified by script rule, if any
    virtual action apply_word_substitution(std::string & word)
    {
        if (word_substitution_.empty() || word != keyword_)
            return inapplicable;
        word = word_substitution_;
        return complete;
    }

    // return true iff this rule has whole-sentence transformation rules associated with it
    virtual bool has_transformation() const { return false; }

    // use this rule's decomposition/reassembly rules to transform given 'words'
    virtual action apply_transformation(stringlist & /*words*/,
        const tagmap & /*tags*/, std::string & /*link_keyword*/)
    {
        return inapplicable;
    }

    virtual stringlist dlist_tags() const { return stringlist(); }

    virtual std::string to_string() const = 0;

protected:
    std::string keyword_;           // the word that triggers this rule
    std::string word_substitution_; // the word that is to replace the keyword, if any
    int precedence_{ 0 };           // the highest priority rule is selected first

    struct transform {              // decomposition and associated reassembly rules
        stringlist decomposition;
        std::vector<stringlist> reassembly_rules;
        unsigned next_reassembly_rule{ 0 };
        transform() {}
        transform(const stringlist & decomposition,
            const std::vector<stringlist> & reassembly_rules)
            : decomposition(decomposition), reassembly_rules(reassembly_rules)
        {}
    };
    std::vector<transform> trans_;  // transformations associated with this rule
};

std::string rule_base::to_string() const
{
    return std::string();
}


// map keyword -> transformation rule
typedef std::map<std::string, std::shared_ptr<rule_base>> rulemap;

// in the rules map, the special-cases NONE and MEMORY have keys
// that cannot match any user input text
#define SPECIAL_RULE_NONE   "zNONE"
#define SPECIAL_RULE_MEMORY "zMEMORY"



// e.g. (ME = YOU)
class rule_unconditional_substitution : public rule_base {
public:
    rule_unconditional_substitution() {}

    rule_unconditional_substitution(const std::string & keyword, const std::string & word_substitution)
        : rule_base(keyword, word_substitution, 0)
    {}

    virtual std::string to_string() const
    {
        return "(" + keyword_ + " = " + word_substitution_ + ")\n";
    }
};


// e.g. (MOM = MOTHER DLIST(/ FAMILY))
class rule_dlist : public rule_base {
public:
    rule_dlist() {}

    rule_dlist(const std::string & keyword, const std::string & word_substitution, const stringlist & tags)
        : rule_base(keyword, word_substitution, 0), tags_(tags)
    {
    }

    stringlist dlist_tags() const { return tags_; }

    virtual std::string to_string() const
    {
        std::string sexp("(");
        sexp += keyword_;
        if (!word_substitution_.empty())
            sexp += " = " + word_substitution_;
        sexp += " DLIST(" + join(tags_) + "))\n";
        return sexp;
    }

private:
    stringlist tags_;
};


// e.g. (DREAMED = DREAMT 4 (=DREAMT))
class rule_equivalence_class : public rule_base {
public:
    rule_equivalence_class() {}

    rule_equivalence_class(const std::string & keyword, const std::string & word_substitution,
        int precedence, const std::string & link_keyword)
        : rule_base(keyword, word_substitution, precedence), link_keyword_(link_keyword)
    {}

    virtual bool has_transformation() const { return true; }

    virtual action apply_transformation(stringlist & /*words*/, const tagmap & /*tags*/, std::string & link_keyword)
    {
        link_keyword = link_keyword_;
        return linkkey;
    }

    virtual std::string to_string() const
    {
        std::string sexp("(");
        sexp += keyword_;
        if (!word_substitution_.empty())
            sexp += " = " + word_substitution_;
        if (precedence_ > 0)
            sexp += " " + std::to_string(precedence_);
        sexp += " (= " + link_keyword_ + "))\n";
        return sexp;
    }

private:
    std::string link_keyword_;
};


/* e.g.
    (YOU'RE = I'M
        ((0 I'M 0)
            (PRE (I ARE 3) (=YOU))))
*/
class rule_pre : public rule_base {
public:
    rule_pre() {}

    rule_pre(std::string keyword, int precedence, std::string word_substitution,
        stringlist decomposition, stringlist reassembly, std::string link_keyword)
        : rule_base(keyword, word_substitution, precedence), link_keyword_(link_keyword)
    {
        std::vector<stringlist> reassembly_rules;
        reassembly_rules.push_back(reassembly);
        add_transformation_rule(decomposition, reassembly_rules);
    }

    virtual bool has_transformation() const { return true; }

    virtual action apply_transformation(stringlist & words, const tagmap & tags, std::string & link_keyword)
    {
        if (trans_.size() != 1)
            return inapplicable; // bad script?

        stringlist constituents;
        if (!match(tags, trans_[0].decomposition, words, constituents))
            return inapplicable; // [page 39 (f)] should not happen?

        if (trans_[0].reassembly_rules.size() != 1)
            return inapplicable; // bad script?
        stringlist & reassembly_rule = trans_[0].reassembly_rules[0];

        // use the selected reassembly rule and decomposition components
        // to construct a response sentence
        words = reassemble(reassembly_rule, constituents);

        link_keyword = link_keyword_;
        return linkkey;
    }

    virtual std::string to_string() const
    {
        std::string sexp("(");
        sexp += keyword_;
        if (!word_substitution_.empty())
            sexp += " = " + word_substitution_;
        sexp += "\n    ((" + join(trans_[0].decomposition) + ")";
        sexp += "\n        (PRE (" + join(trans_[0].reassembly_rules[0]) + ") (=" + link_keyword_ + "))))\n";
        return sexp;
    }

private:
    std::string link_keyword_;
};


/* e.g.
    (MEMORY MY
        (0 YOUR 0 = LETS DISCUSS FURTHER WHY YOUR 3)
        (0 YOUR 0 = EARLIER YOU SAID YOUR 3)
        (0 YOUR 0 = BUT YOUR 3)
        (0 YOUR 0 = DOES THAT HAVE ANYTHING TO DO WITH THE FACT THAT YOUR 3))
*/
class rule_memory : public rule_base {
public:
    rule_memory() {}

    rule_memory(const std::string & keyword)
        : rule_base(keyword, "", 0)
    {}

    void create_memory(const std::string & keyword, const stringlist & words, const tagmap & tags)
    {
        if (keyword != keyword_)
            return;

        // JW says rules are selected at random [page 41 (f)]
        // But the ELIZA code shows that rules are actually selected via a HASH
        // function on the last word of the user's input text.
        assert(trans_.size() == num_transformations);
        const auto & transformation = trans_[hash(last_chunk_as_bcd(words.back()), 2)];

        stringlist constituents;
        if (!match(tags, transformation.decomposition, words, constituents))
            return;

        //std::cout << "Making memory[" << words.back() << "]: " << join(reassemble(transformation.reassembly_rules[0], constituents)) << "\n";
        memories_.push_back(join(reassemble(transformation.reassembly_rules[0], constituents)));
    }

    // return true iff we have at least one saved memory
    bool memory_exists() const
    {
        return !memories_.empty();
    }

    // return the next saved memory in the queue; remove it from the queue
    std::string recall_memory()
    {
        return memories_.empty() ? "" : pop_front(memories_);
    }

    virtual std::string to_string() const
    {
        std::string sexp("(MEMORY ");
        sexp += keyword_;
        for (const auto & k : trans_) {
            sexp += "\n    (" + join(k.decomposition);
            sexp += " = " + join(k.reassembly_rules[0]) + ")";
        }
        sexp += ")\n";
        return sexp;
    }

    // the MEMORY rule must have this number of transformations
    static constexpr int num_transformations = 4;

private:
    stringlist memories_;
 };


/* e.g.
    (MY = YOUR 2
        ((0 YOUR 0 (/FAMILY) 0)
            (TELL ME MORE ABOUT YOUR FAMILY)
            (WHO ELSE IN YOUR FAMILY 5)
            (=WHAT)
            (WHAT ELSE COMES TO MIND WHEN YOU THINK OF YOUR 4))
        ((0 YOUR 0 (*SAD UNHAPPY DEPRESSED SICK ) 0)
            (CAN YOU EXPLAIN WHAT MADE YOU 5))
            ((0)
            (NEWKEY)))
*/
class rule_vanilla : public rule_base {
public:
    rule_vanilla() {}

    rule_vanilla(const std::string & keyword, const std::string & word_substitution, int precedence)
        : rule_base(keyword, word_substitution, precedence)
    {}

    virtual bool has_transformation() const { return true; }

    virtual action apply_transformation(stringlist & words, const tagmap & tags, std::string & link_keyword)
    {
        if (trans_.empty())
            return inapplicable; // bad script?

        stringlist constituents;
        auto rule = trans_.begin();
        while (rule != trans_.end() && !match(tags, rule->decomposition, words, constituents))
            ++rule;
        if (rule == trans_.end())
            return inapplicable; // [page 39 (f)] should not happen?

        // get the next reassembly rule to be used for this decomposition rule
        stringlist & reassembly_rule = rule->reassembly_rules[rule->next_reassembly_rule];

        // update the reassembly rule index so that they all get cycled through
        rule->next_reassembly_rule++;
        if (rule->next_reassembly_rule == rule->reassembly_rules.size())
            rule->next_reassembly_rule = 0;

        // is it the special-case reassembly rule (NEWKEY)?
        if (reassembly_rule.size() == 1 && reassembly_rule[0] == "NEWKEY")
            return newkey; // yes, try the next highest priority keyword, if any

        // is it the special-case reassembly rule (=XXXX)?
        if (reassembly_rule.size() == 1 && reassembly_rule[0].size() > 1 && reassembly_rule[0][0] == '=') {
            link_keyword = reassembly_rule[0];
            pop_front(link_keyword); // pop off the '='
            return linkkey; // yes, try the next highest priority keyword, if any
        }

        // use the selected reassembly rule and decomposition components
        // to construct a response sentence
        words = reassemble(reassembly_rule, constituents);
        return complete;
    }

    virtual std::string to_string() const
    {
        std::string sexp("(");
        sexp += (keyword_ == SPECIAL_RULE_NONE) ? "NONE" : keyword_;
        if (!word_substitution_.empty())
            sexp += " = " + word_substitution_;
        if (precedence_ > 0)
            sexp += " " + std::to_string(precedence_);
        for (const auto & k : trans_) {
            sexp += "\n    ((" + join(k.decomposition) + ")";
            for (const auto & r : k.reassembly_rules)
                sexp += "\n        (" + join(r) + ")";
            sexp += ")";
        }
        sexp += ")\n";
        return sexp;
    }
};



// collect all tags from any of the given 'rules' that have them into a tagmap
tagmap collect_tags(const rulemap & rules)
{
    tagmap tags;
    for (auto & r : rules) {
        stringlist keyword_tags(r.second->dlist_tags());
        for (auto t : keyword_tags) {
            if (t == "/")
                continue;
            if (t.size() > 1 && t.front() == '/')
                pop_front(t);
            tags[t].push_back(r.second->keyword());
        }
    }
    return tags;
}


template<typename T>
auto get_rule(rulemap & rules, const std::string & keyword)
{
    auto rule = rules.find(keyword);
    if (rule == rules.end()) {
        std::string msg("SCRIPT ERROR FOR ");
        msg += keyword;
        throw std::runtime_error(msg);
    }
    auto castrule = std::dynamic_pointer_cast<T>(rule->second);
    if (!castrule) {
        std::string msg("INTERNAL ERROR FOR ");
        msg += keyword;
        throw std::runtime_error(msg);
    }
    return castrule;
}



                //////// //       //// ////////    ///                    
                //       //        //       //    // //                   
                //       //        //      //    //   //                  
///////////     //////   //        //     //    //     //     /////////// 
                //       //        //    //     /////////                 
                //       //        //   //      //     //                 
                //////// //////// //// //////// //     //                 

class eliza {
public:
    eliza(const rulemap & rules)
        : rules_(rules), tags_(collect_tags(rules_))
    {}

    eliza(rulemap && rules)
        : rules_(std::move(rules)), tags_(collect_tags(rules_))
    {}


    // produce a response to the given 'input' using the given 'rules'
    std::string response(const std::string & input)
    {
        // for simplicity, convert the given input string to a list of uppercase words
        // e.g. "Hello, world!" -> (HELLO , WORLD !)
        stringlist words(split(filter_bcd(to_upper(input))));

        // JW's "a certain counting mechanism" is updated for each response
        limit_ = limit_ % 4 + 1;

        // scan for keywords [page 38 (c)]; build the keystack; apply word substitutions
        stringlist keystack;
        int top_rank = 0;
        for (auto word = words.begin(); word != words.end(); ) {
            if (delimeter(*word)) {
                // keep only the first clause to contain a keyword [page 37 (c)]
                if (keystack.empty()) {
                    // discard left of punctuation, continue scanning what remains
                    word = words.erase(words.begin(), ++word);
                    continue;
                }
                else {
                    // discard right of punctuation, scan is complete
                    word = words.erase(word, words.end());
                    break;
                }
            }

            const auto r = rules_.find(*word);
            if (r != rules_.end()) {
                const auto & rule = r->second;
                if (rule->has_transformation()) {
                    if (rule->precedence() > top_rank) {
                        // *word is a keyword with precedence higher than the highest
                        // keyword found previously: it goes top of the keystack [page 39 (d)]
                        keystack.push_front(*word);
                        top_rank = rule->precedence();
                    }
                    else {
                        // *word is a keyword with precedence lower than the highest
                        // keyword found previously: it goes bottom of the keystack
                        keystack.push_back(*word);
                    }
                }
                rule->apply_word_substitution(*word); // [page 39 (a)]
            }

            ++word;
        }

        auto memory_rule = get_rule<rule_memory>(rules_, SPECIAL_RULE_MEMORY);
        if (keystack.empty()) {
            /*  a text without keywords; can we recall a MEMORY ? [page 41 (f)]
                JW's 1966 CACM paper refers to this decision as "a certain counting
                mechanism is in a particular state." The ELIZA code shows that the
                memory is recalled only when LIMIT has the value 4 */
            if (limit_ == 4 && memory_rule->memory_exists())
                return memory_rule->recall_memory();
        }

        // the keystack contains all keywords that occur in the given 'input';
        // apply transformation associated with the top keyword [page 39 (d)]
        while (!keystack.empty()) {
            const std::string top_keyword = pop_front(keystack);

            auto rule = rules_.find(top_keyword);
            if (rule == rules_.end()) {
                // e.g. could happen if a rule links to a non-existent keyword
                return nomatch_msgs_[limit_ - 1];
            }

            // try to lay down a memory for future use
            memory_rule->create_memory(top_keyword, words, tags_);

            // perform the transformation for this rule
            std::string link_keyword;
            auto act = rule->second->apply_transformation(words, tags_, link_keyword);

            if (act == rule_base::complete)
                return join(words); // decomposition/reassembly successfully applied

            if (act == rule_base::inapplicable) {
                // no decomposition rule matched the input words; script error?
                return nomatch_msgs_[limit_ - 1];
            }

            if (act == rule_base::linkkey)
                keystack.push_front(link_keyword); // rule links to another; loop

            // (rule_base::newkey -> rule wants to try next highest keyword, if any)
            assert(act == rule_base::linkkey || act == rule_base::newkey);
        }


        // last resort: the NONE rule never fails to produce a response [page 41 (d)]
        auto none_rule = get_rule<rule_vanilla>(rules_, SPECIAL_RULE_NONE);
        std::string discard;
        none_rule->apply_transformation(words, tags_, discard);
        return join(words);
    }

private:
    // JW's "a certain counting mechanism," LIMIT, cycles through 1..4, then back to 1
    int limit_{ 1 };

    // the ELIZA script in 'rulemap' form
    rulemap rules_;

    // e.g. tags[BELIEF] -> (BELIEVE FEEL THINK WISH)
    // (This is derived from rules_. It's a member so we only need derive it once.)
    const tagmap tags_;

    // script error messages hard-coded in JW's ELIZA, selected by LIMIT (our limit_)
    const char * const nomatch_msgs_[4] = {
        "PLEASE CONTINUE",
        "HMMM",
        "GO ON, PLEASE",
        "I SEE"
    };
};


}//namespace elizalogic





 //////// //       //// ////////    ///     //////   //////  ////////  //// ////////  //////// 
 //       //        //       //    // //   //    // //    // //     //  //  //     //    //    
 //       //        //      //    //   //  //       //       //     //  //  //     //    //    
 //////   //        //     //    //     //  //////  //       ////////   //  ////////     //    
 //       //        //    //     /////////       // //       //   //    //  //           //    
 //       //        //   //      //     // //    // //    // //    //   //  //           //    
 //////// //////// //// //////// //     //  //////   //////  //     // //// //           //    


namespace elizascript { // reader for 1966 ELIZA script file format


struct script {
    // ELIZA's opening remarks e.g. "HOW DO YOU DO.  PLEASE TELL ME YOUR PROBLEM"
    stringlist hello_message;

    // maps keywords -> transformation rules
    elizalogic::rulemap rules;
};



struct token {
    // types of token in an ELIZA script file
    enum class typ { eof, symbol, number, open_bracket, close_bracket };

    typ t{ typ::eof };
    std::string value;

    token() : t(typ::eof) {}
    token(typ t) : t(t) {}
    token(typ t, const std::string & value) : t(t), value(value) {}

    bool symbol()               const { return t == typ::symbol; }
    bool symbol(const char * v) const { return t == typ::symbol && value == v; }
    bool number()               const { return t == typ::number; }
    bool open()                 const { return t == typ::open_bracket; }
    bool close()                const { return t == typ::close_bracket; }
    bool eof()                  const { return t == typ::eof; }

    bool operator==(const token & rhs) const { return t == rhs.t && value == rhs.value; }
};



// this is just good enough to divide the ELIZA script file format
// into tokens useful to eliza_script_reader
template<typename T>
class tokenizer {
public:
    tokenizer(T & script_file) : stream_(script_file) {}

    // return the current token, but don't advance past it
    token peektok()
    {
        if (got_token_)
            return t_;
        got_token_ = true;
        return t_ = readtok();
    }

    // return the current token, advance read point past it
    token nexttok()
    {
        if (got_token_) {
            got_token_ = false;
            return t_;
        }
        return readtok();
    }

private:
    T & stream_;
    token t_;
    bool got_token_{ false };
    const static size_t buflen_ = 512;
    uint8_t buf_[buflen_];
    size_t bufdata_{ 0 };
    size_t bufptr_{ 0 };

    token readtok()
    {
        uint8_t ch;
        for (;;) {
            do { // skip whitespace
                if (!nextch(ch))
                    return (token(token::typ::eof));
            } while (is_whitespace(ch));
            if (ch != ';')
                break;
            do { // skip comment
                if (!nextch(ch))
                    return (token(token::typ::eof));
            } while (!is_newline(ch));
        }

        if (ch == '(')
            return (token(token::typ::open_bracket));

        if (ch == ')')
            return (token(token::typ::close_bracket));

        if (is_digit(ch)) {
            token t(token::typ::number);
            t.value.push_back(ch);
            while (peekch(ch) && is_digit(ch)) {
                t.value.push_back(ch);
                nextch(ch);
            }
            return t;
        }

        // anything else is a symbol
        token t(token::typ::symbol);
        t.value.push_back(ch);
        while (peekch(ch) && !non_symbol(ch)) {
            t.value.push_back(ch);
            nextch(ch);
        }
        return t;
    }

    bool nextch(uint8_t & ch)
    {
        if (peekch(ch)) {
            ++bufptr_;
            return true;
        }
        return false;
    }

    bool peekch(uint8_t & ch)
    {
        if (bufptr_ == bufdata_)
            refilbuf();
        if (bufptr_ == bufdata_)
            return false;
        ch = buf_[bufptr_];
        return true;
    }
    
    void refilbuf()
    {
        bufptr_ = bufdata_ = 0;
        if (!stream_.eof()) {
            stream_.read(reinterpret_cast<char *>(buf_), buflen_);
            bufdata_ = static_cast<size_t>(stream_.gcount());
        }
    }

    inline bool is_whitespace(uint8_t ch)
    {
        return ch <= 0x20 || ch == 0x7F;
        // this must hold: is_newline(ch) => is_whitespace(ch)
    }

    inline bool is_newline(uint8_t ch)
    {
        return ch == '\x0A'     // LF
            || ch == '\x0B'     // VT
            || ch == '\x0C'     // FF
            || ch == '\x0D';    // CR
    }

    inline bool is_digit(uint8_t ch)
    {
        return unsigned(ch) - '0' < 10;
    }

    inline bool non_symbol(uint8_t ch)
    {
        return ch == '(' || ch == ')' || ch == ';' || is_whitespace(ch);
    }
};





template<typename T>
class eliza_script_reader {
public:

    eliza_script_reader(T & script_file, script & s)
        : tok_(script_file), script_(s)
    {
        script_.hello_message = rdlist();
        if (tok_.peektok().symbol("START"))
            tok_.nexttok(); // skip over START, if present

        while (read_rule())
            ;
    }

private:
    tokenizer<T> tok_;
    script & script_;


    // in the following comments, @ = position in symbol stream on function entry

    // return words between opening and closing brackets
    // if prior is true nexttok() should be the opening bracket, e.g. @(WORD WORD 0 WORD)
    // if prior is false nexttok() should be the first symbol following the
    // opening bracket, e.g. (@WORD WORD 0 WORD)
    stringlist rdlist(bool prior = true)
    {
        stringlist s;

        token t = tok_.nexttok();
        if (prior) {
            if (!t.open())
                throw std::runtime_error("expected '('");
            t = tok_.nexttok();
        }
        while (!t.close()) {
            if (t.t == token::typ::symbol)
                s.emplace_back(t.value);
            else if (t.number())
                s.emplace_back(t.value);
            else if (t.open()) {
                // embed entire sublist in one sub-string
                std::string sublist;
                t = tok_.nexttok();
                while (!t.close()) {
                    if (!t.symbol())
                        throw std::runtime_error("expected symbol");
                    if (!sublist.empty())
                        sublist += ' ';
                    sublist += t.value;
                    t = tok_.nexttok();
                }
                s.emplace_back("(" + sublist + ")");
            }
            else
                throw std::runtime_error("expected ')'");
            t = tok_.nexttok();
        }

        return s;
    }


    /* e.g.
        (YOU'RE = I'M
            ((0 I'M 0)
                (@PRE (I ARE 3) (=YOU))))
    */
    bool read_pre_rule(const std::string & keyword,
        int precedence,
        const std::string & keyword_substitution,
        const stringlist & decomposition)
    {
        tok_.nexttok(); // skip PRE
        stringlist reassembly = rdlist();

        if (!tok_.nexttok().open())
            throw std::runtime_error("expected '('");
        token t = tok_.nexttok();
        std::string link_keyword;
        if (t.symbol("=")) {
            t = tok_.nexttok();
            if (!t.symbol())
                throw std::runtime_error("expected equivalence class name");
            link_keyword = t.value;
        }
        else if (t.symbol() && t.value.size() > 1 && t.value[0] == '=')
            link_keyword = std::string(reinterpret_cast<const char *>(&t.value[1]), t.value.size() - 1);
        else
            throw std::runtime_error("expected equivalence class name");

        if (!tok_.nexttok().close())
            throw std::runtime_error("expected ')'");
        if (!tok_.nexttok().close())
            throw std::runtime_error("expected ')'");
        if (!tok_.nexttok().close())
            throw std::runtime_error("expected ')'");
        if (!tok_.nexttok().close())
            throw std::runtime_error("expected ')'");

        script_.rules[keyword] = std::make_shared<elizalogic::rule_pre>(keyword, precedence,
            keyword_substitution, decomposition, reassembly, link_keyword);

        return true;
    }


    /* e.g.
        (MY = YOUR 2
            (@(0 YOUR 0 (/FAMILY) 0)
                (TELL ME MORE ABOUT YOUR FAMILY)
                (WHO ELSE IN YOUR FAMILY 5)
                (YOUR 4)
                (WHAT ELSE COMES TO MIND WHEN YOU THINK OF YOUR 4))
            ((0 YOUR 0 (*SAD UNHAPPY DEPRESSED SICK ) 0)
                (CAN YOU EXPLAIN WHAT MADE YOU 5))
            ((0)
                (NEWKEY)))
    */
    bool read_vanilla_rule(const std::string & keyword,
        const std::string & keyword_substitution, int precedence) // or PRE
    {
        auto r = std::make_shared<elizalogic::rule_vanilla>(
            keyword, keyword_substitution, precedence);

        for (;;) {
            const stringlist decomposition = rdlist();
            std::vector<stringlist> reassembly_rules;

            token t = tok_.nexttok();
            for (;;) {
                if (!t.open())
                    throw std::runtime_error("expected '('");
                if (tok_.peektok().symbol("PRE"))
                    return read_pre_rule(keyword, precedence, keyword_substitution, decomposition);
                reassembly_rules.push_back(rdlist(false));
                t = tok_.nexttok();
                if (t.close())
                    break;
            }
            r->add_transformation_rule(decomposition, reassembly_rules);

            t = tok_.nexttok();
            if (t.close())
                break;
            if (!t.open())
                throw std::runtime_error("expected '('");
        }
        script_.rules[keyword] = r;

        return true;
    }


    /* e.g.
        (NONE@
            ((0)
                (I AM NOT SURE I UNDERSTAND YOU FULLY)
                (PLEASE GO ON)
                (WHAT DOES THAT SUGGEST TO YOU)
                (DO YOU FEEL STRONGLY ABOUT DISCUSSING SUCH THINGS)))
    */
    bool read_none()
    {
        tok_.nexttok();
        return read_vanilla_rule(SPECIAL_RULE_NONE, "", 0);
    }


    /* e.g.
        (MEMORY@ MY
            (0 YOUR 0 = LETS DISCUSS FURTHER WHY YOUR 3)
            (0 YOUR 0 = EARLIER YOU SAID YOUR 3)
            (0 YOUR 0 = BUT YOUR 3)
            (0 YOUR 0 = DOES THAT HAVE ANYTHING TO DO WITH THE FACT THAT YOUR 3))
    */
    bool read_memory()
    {
        token t = tok_.nexttok();
        if (!t.symbol())
            throw std::runtime_error("expected keyword");
        auto r = std::make_shared<elizalogic::rule_memory>(t.value);

        for (int i = 0; i < elizalogic::rule_memory::num_transformations; ++i) {
            stringlist decomposition;
            std::vector<stringlist> reassembly_rules;

            if (!tok_.nexttok().open())
                throw std::runtime_error("expected '('");
            for (t = tok_.nexttok(); !t.symbol("=") && !t.eof(); t = tok_.nexttok())
                decomposition.push_back(t.value);
            stringlist reassembly;
            for (t = tok_.nexttok(); !t.close() && !t.eof(); t = tok_.nexttok())
                reassembly.push_back(t.value);
            reassembly_rules.push_back(reassembly);

            r->add_transformation_rule(decomposition, reassembly_rules);
        }

        if (!tok_.nexttok().close())
            throw std::runtime_error("expected ')'");

        script_.rules[SPECIAL_RULE_MEMORY] = r;
        return true;
    }


    // e.g. (MOM = MOTHER DLIST@(/ FAMILY))
    bool dlist(const std::string & keyword,
        const std::string & keyword_substitution)
    {
        stringlist tags;
        tags = rdlist();
        token t = tok_.nexttok();
        if (!t.close())
            throw std::runtime_error("expected ')'");
        script_.rules[keyword] = std::make_shared<elizalogic::rule_dlist>(keyword, keyword_substitution, tags);
        return true;
    }


    // e.g. (DREAMED = DREAMT 4 (@=DREAMT))
    bool read_equivalence_class(const std::string & keyword,
        const std::string & keyword_substitution, int precedence)
    {
        std::string class_name;
        token t = tok_.nexttok();
        if (t.symbol("=")) {
            t = tok_.nexttok();
            if (!t.symbol())
                throw std::runtime_error("expected equivalence class name");
            class_name = t.value;
        }
        else if (t.symbol() && t.value.size() > 1 && t.value[0] == '=')
            class_name = std::string(reinterpret_cast<const char *>(&t.value[1]), t.value.size() - 1);
        else
            throw std::runtime_error("expected equivalence class name");

        script_.rules[keyword] = std::make_shared<elizalogic::rule_equivalence_class>(
            keyword, keyword_substitution, precedence, class_name);

        if (!tok_.nexttok().close())
            throw std::runtime_error("expected ')'");
        if (!tok_.nexttok().close())
            throw std::runtime_error("expected ')'");

        return true;
    }


    // read one rule of any type; return false => end of file reached
    bool read_rule()
    {
        std::string keyword, keyword_substitution;
        int precedence = 0;

        token t = tok_.nexttok();
        if (t.t == token::typ::eof)
            return false;
        if (!t.open())
            throw std::runtime_error("expected '('");

        t = tok_.nexttok();
        if (t.t == token::typ::close_bracket)
            return true; // ignore empty rule list, if present
        if (t.t != token::typ::symbol)
            throw std::runtime_error("expected keyword|MEMORY|NONE");
        if (t.value == "MEMORY")
            return read_memory();
        else if (t.value == "NONE")
            return read_none();
        keyword = t.value;

        for (;;) {
            t = tok_.nexttok();
            if (t.symbol("=")) {
                t = tok_.nexttok();
                if (t.t != token::typ::symbol)
                    throw std::runtime_error("expected keyword");
                keyword_substitution = t.value;
            }
            else if (t.number())
                precedence = std::stoi(t.value);
            else if (t.symbol("DLIST"))
                return dlist(keyword, keyword_substitution);
            else if (t.open()) {
                t = tok_.peektok();
                if (t.symbol() && t.value[0] == '=')
                    return read_equivalence_class(keyword, keyword_substitution, precedence);
                return read_vanilla_rule(keyword, keyword_substitution, precedence);
            }
            else if (t.t == token::typ::close_bracket) {
                // e.g. (ME = YOU)
                script_.rules[keyword] = std::make_shared<elizalogic::rule_unconditional_substitution>(
                    keyword, keyword_substitution);
                return true;
            }
            else
                throw std::runtime_error("malformed rule");
        }

        return true;
    }
};


template<typename T>
void read(T & script_file, script & s)
{
    eliza_script_reader<T> reader(script_file, s);
}



const char * CACM_1966_01_DOCTOR_script =
    ";\n"
    "; APPENDIX. An Eliza Script\n"
    ";\n"
    "; Transcribed from Joseph Weizenbaum's article on page 36 of the January\n"
    "; 1966 edition of Communications of the ACM titled 'ELIZA - A Computer\n"
    "; Program For the Study of Natural Language Communication Between Man And\n"
    "; Machine'.\n"
    ";\n"
    "; \"Keywords and their associated transformation rules constitute the\n"
    "; SCRIPT for a particular class of conversation. An important property of\n"
    "; ELIZA is that a script is data; i.e., it is not part of the program\n"
    "; itself.\" -- From the above mentioned article.\n"
    ";\n"
    "; Transcribed by Anthony Hay, December 2020\n"
    ";\n"
    ";\n"
    "; Notes\n"
    ";\n"
    "; This is a verbatim transcription of the ELIZA script in the above\n"
    "; mentioned CACM article, with the following caveats:\n"
    "; a) Whitespace has been added to help reveal the structure of the\n"
    ";    script.\n"
    "; b) In the appendix six lines were printed twice adjacent to each other\n"
    ";    (with exactly 34 lines between each duplicate), making the structure\n"
    ";    nonsensical. These duplicates have been commented out of this\n"
    ";    transcription.\n"
    "; c) One closing bracket has been added and noted in a comment.\n"
    "; d) There were no comments in the script in the CACM article.\n"
    ";\n"
    ";\n"
    "; The script has the form of a series of S-expressions of varying\n"
    "; composition. (Weizenbaum says \"An ELIZA script consists mainly of a set\n"
    "; of list structures...\", but nowhere in the article are S-expressions or\n"
    "; LISP mentioned. Perhaps it was too obvious to be noted.) Weizenbaum says\n"
    "; ELIZA was written in MAD-Slip. It seems his original source code has\n"
    "; been lost. Weizenbaum developed a library of FORTRAN functions for\n"
    "; manipulating doubly-linked lists, which he called Slip (for Symmetric\n"
    "; list processor).\n"
    ";\n"
    "; The most common transformation rule has the form:\n"
    ";\n"
    ";   (keyword [= replacement-keyword] [precedence]\n"
    ";     [ ((decomposition-rule-0) (reassembly-rule-00) (reassembly-rule-01) ...)\n"
    ";       ((decomposition-rule-1) (reassembly-rule-10) (reassembly-rule-11) ...)\n"
    ";       ... ] )\n"
    ";\n"
    "; where [] denotes optional parts. Initially, ELIZA tries to match the\n"
    "; decomposition rules against the input text only for the highest ranked\n"
    "; keyword found in the input text. If a decomposition rule matches the\n"
    "; input text the first associated reassembly rule is used to generate\n"
    "; the output text. If there is more than one reassembly rule they are\n"
    "; used in turn on successive matches.\n"
    ";\n"
    "; In the decomposition rules '0' matches zero or more words in the input.\n"
    "; So (0 IF 0) matches \"IF POSSIBLE\" and \"WHAT IF YOU DIE\". Numbers in\n"
    "; the reassembly rules refer to the parts of the decomposition rule\n"
    "; match. 1 <empty>, 2 \"IF\", 3 \"POSSIBLE\" and 1 \"WHAT\", 2 \"IF\", 3 \"YOU DIE\"\n"
    "; in the above examples. If the selected reassembly rule was (DO YOU THINK\n"
    "; ITS LIKELY THAT 3) the text output would be \"DO YOU THINK ITS LIKELY\n"
    "; THAT YOU DIE\".\n"
    ";\n"
    ";\n"
    "; Each rule has one of the following six forms:\n"
    ";\n"
    "; R1. Plain vanilla transformation rule. [page 38 (a)]\n"
    ";     (keyword [= keyword_substitution] [precedence]\n"
    ";         [ ((decomposition_pattern) (reassembly_rule) (reassembly_rule) ... )\n"
    ";            (decomposition_pattern) (reassembly_rule) (reassembly_rule) ... )\n"
    ";            :\n"
    ";            (decomposition_pattern) (reassembly_rule) (reassembly_rule) ... )) ] )\n"
    ";   e.g.\n"
    ";     (MY = YOUR 2\n"
    ";         ((0 YOUR 0 (/FAMILY) 0)\n"
    ";             (TELL ME MORE ABOUT YOUR FAMILY)\n"
    ";             (WHO ELSE IN YOUR FAMILY 5)\n"
    ";             (=WHAT)\n"
    ";             (WHAT ELSE COMES TO MIND WHEN YOU THINK OF YOUR 4))\n"
    ";         ((0 YOUR 0 (*SAD UNHAPPY DEPRESSED SICK ) 0)\n"
    ";             (CAN YOU EXPLAIN WHAT MADE YOU 5))\n"
    ";         ((0)\n"
    ";             (NEWKEY)))\n"
    ";\n"
    ";\n"
    "; R2. Simple word substitution with no further transformation rules. [page 39 (a)]\n"
    ";     (keyword = keyword_substitution)\n"
    ";   e.g.\n"
    ";     (DONT = DON'T)\n"
    ";     (ME = YOU)\n"
    ";\n"
    ";\n"
    "; R3. Allow words to be given tags, with optional word substitution. [page 41 (j)]\n"
    ";     (keyword [= keyword_substitution]\n"
    ";         DLIST (/ <word> ... <word>))\n"
    ";   e.g.\n"
    ";         (FEEL               DLIST(/BELIEF))\n"
    ";         (MOTHER             DLIST(/NOUN FAMILY))\n"
    ";         (MOM = MOTHER       DLIST(/ FAMILY))\n"
    ";\n"
    ";\n"
    "; R4. Link to another keyword transformation rule. [page 40 (c)]\n"
    ";     (keyword [= keyword_substitution] [precedence]\n"
    ";         (= equivalence_class))\n"
    ";   e.g.\n"
    ";         (HOW                (=WHAT))\n"
    ";         (WERE = WAS         (=WAS))\n"
    ";         (DREAMED = DREAMT 4 (=DREAMT))\n"
    ";         (ALIKE 10           (=DIT))\n"
    ";\n"
    ";\n"
    "; R5. As for R4 but allow pre-transformation before link. [page 40 (f)]\n"
    ";     (keyword [= keyword_substitution]\n"
    ";         ((decomposition_pattern)\n"
    ";             (PRE (reassembly_rule) (=equivalence_class))))\n"
    ";   e.g.\n"
    ";     (YOU'RE = I'M\n"
    ";         ((0 I'M 0)\n"
    ";             (PRE (I ARE 3) (=YOU))))\n"
    ";\n"
    ";\n"
    "; R6. Rule to 'pre-record' responses for later use. [page 41 (f)]\n"
    ";     (MEMORY keyword\n"
    ";         (decomposition_pattern_1 = reassembly_rule_1)\n"
    ";         (decomposition_pattern_2 = reassembly_rule_2)\n"
    ";         (decomposition_pattern_3 = reassembly_rule_3)\n"
    ";         (decomposition_pattern_4 = reassembly_rule_4))\n"
    ";   e.g.\n"
    ";     (MEMORY MY\n"
    ";         (0 YOUR 0 = LETS DISCUSS FURTHER WHY YOUR 3)\n"
    ";         (0 YOUR 0 = EARLIER YOU SAID YOUR 3)\n"
    ";         (0 YOUR 0 = BUT YOUR 3)\n"
    ";         (0 YOUR 0 = DOES THAT HAVE ANYTHING TO DO WITH THE FACT THAT YOUR 3))\n"
    ";\n"
    ";\n"
    "; In addition, there must be a NONE rule with the same form as R1. [page 41 (d)]\n"
    ";     (NONE\n"
    ";         ((0)\n"
    ";             (reassembly_rule)\n"
    ";             (reassembly_rule)\n"
    ";             :\n"
    ";             (reassembly_rule)) )\n"
    ";   e.g.\n"
    ";     (NONE\n"
    ";         ((0)\n"
    ";             (I AM NOT SURE I UNDERSTAND YOU FULLY)\n"
    ";             (PLEASE GO ON)\n"
    ";             (WHAT DOES THAT SUGGEST TO YOU)\n"
    ";             (DO YOU FEEL STRONGLY ABOUT DISCUSSING SUCH THINGS)))\n"
    ";\n"
    ";\n"
    "; For further details see Weizenbaum's article, or look at eliza.cpp.\n"
    ";\n"
    "\n"
    "\n"
    "(HOW DO YOU DO.  PLEASE TELL ME YOUR PROBLEM)\n"
    "\n"
    "START\n"
    "\n"
    "(SORRY\n"
    "    ((0)\n"
    "        (PLEASE DON'T APOLIGIZE)\n"
    "        (APOLOGIES ARE NOT NECESSARY)\n"
    "        (WHAT FEELINGS DO YOU HAVE WHEN YOU APOLOGIZE)\n"
    "        (I'VE TOLD YOU THAT APOLOGIES ARE NOT REQUIRED)))\n"
    "\n"
    "(DONT = DON'T)\n"
    "(CANT = CAN'T)\n"
    "(WONT = WON'T)\n"
    "\n"
    "(REMEMBER 5\n"
    "    ((0 YOU REMEMBER 0)\n"
    "         (DO YOU OFTEN THINK OF 4)\n"
    "         (DOES THINKING OF 4 BRING ANYTHING ELSE TO MIND)\n"
    "         (WHAT ELSE DO YOU REMEMBER)\n"
    "         (WHY DO YOU REMEMBER 4 JUST NOW)\n"
    "         (WHAT IN THE PRESENT SITUATION REMINDS YOU OF 4)\n"
    "         (WHAT IS THE CONNECTION BETWEEN ME AND 4))\n"
    "    ((0 DO I REMEMBER 0)\n"
    "         (DID YOU THINK I WOULD FORGET 5)\n"
    "         (WHY DO YOU THINK I SHOULD RECALL 5 NOW)\n"
    "         (WHAT ABOUT 5)\n"
    "         (=WHAT)\n"
    "         (YOU MENTIONED 5))\n"
    "    ((0)\n"
    "         (NEWKEY)))\n"
    "\n"
    "(IF 3\n"
    "    ((0 IF 0)\n"
    "        (DO YOU THINK ITS LIKELY THAT 3)\n"
    "        (DO YOU WISH THAT 3)\n"
    "        (WHAT DO YOU THINK ABOUT 3)\n"
    "        (REALLY, 2 3)))\n"
    "; duplicate line removed: (WHAT DO YOU THINK ABOUT 3) (REALLY, 2 3)))\n"
    "\n"
    "(DREAMT 4\n"
    "    ((0 YOU DREAMT 0)\n"
    "        (REALLY, 4)\n"
    "        (HAVE YOU EVER FANTASIED 4 WHILE YOU WERE AWAKE)\n"
    "        (HAVE YOU DREAMT 4 BEFORE)\n"
    "        (=DREAM)\n"
    "        (NEWKEY)))\n"
    "\n"
    "(DREAMED = DREAMT 4 (=DREAMT))\n"
    "\n"
    "(DREAM 3\n"
    "    ((0)\n"
    "        (WHAT DOES THAT DREAM SUGGEST TO YOU)\n"
    "        (DO YOU DREAM OFTEN)\n"
    "        (WHAT PERSONS APPEAR IN YOUR DREAMS)\n"
    "        (DON'T YOU BELIEVE THAT DREAM HAS SOMETHING TO DO WITH YOUR PROBLEM)\n"
    "        (NEWKEY)))\n"
    "\n"
    "(DREAMS = DREAM 3 (=DREAM))\n"
    "\n"
    "(HOW (=WHAT))\n"
    "(WHEN (=WHAT))\n"
    "(ALIKE 10 (=DIT))\n"
    "(SAME 10 (=DIT))\n"
    "(CERTAINLY (=YES))\n"
    "\n"
    "(FEEL DLIST(/BELIEF))\n"
    "(THINK DLIST(/BELIEF))\n"
    "(BELIEVE DLIST(/BELIEF))\n"
    "(WISH DLIST(/BELIEF))\n"
    "\n"
    "(MEMORY MY\n"
    "    (0 YOUR 0 = LETS DISCUSS FURTHER WHY YOUR 3)\n"
    "    (0 YOUR 0 = EARLIER YOU SAID YOUR 3)\n"
    "    (0 YOUR 0 = BUT YOUR 3)\n"
    "    (0 YOUR 0 = DOES THAT HAVE ANYTHING TO DO WITH THE FACT THAT YOUR 3))\n"
    "\n"
    "(NONE\n"
    "    ((0)\n"
    "        (I AM NOT SURE I UNDERSTAND YOU FULLY)\n"
    "        (PLEASE GO ON)\n"
    "        (WHAT DOES THAT SUGGEST TO YOU)\n"
    "        (DO YOU FEEL STRONGLY ABOUT DISCUSSING SUCH THINGS)))\n"
    "\n"
    "(PERHAPS\n"
    "    ((0)\n"
    "        (YOU DON'T SEEM QUITE CERTAIN)\n"
    "        (WHY THE UNCERTAIN TONE)\n"
    "        (CAN'T YOU BE MORE POSITIVE)\n"
    "        (YOU AREN'T SURE)\n"
    "        (DON'T YOU KNOW)))\n"
    "\n"
    "(MAYBE (=PERHAPS))\n"
    "\n"
    "(NAME 15\n"
    "    ((0)\n"
    "        (I AM NOT INTERESTED IN NAMES)\n"
    "        (I'VE TOLD YOU BEFORE, I DON'T CARE ABOUT NAMES - PLEASE CONTINUE)))\n"
    "; duplicate line removed: PLEASE CONTINUE)) )\n"
    "\n"
    "(DEUTSCH (=XFREMD))\n"
    "(FRANCAIS (=XFREMD))\n"
    "(ITALIANO (=XFREMD))\n"
    "(ESPANOL (=XFREMD))\n"
    "\n"
    "(XFREMD\n"
    "    ((0)\n"
    "        (I AM SORRY, I SPEAK ONLY ENGLISH)))\n"
    "\n"
    "(HELLO\n"
    "    ((0)\n"
    "        (HOW DO YOU DO.  PLEASE STATE YOUR PROBLEM)))\n"
    "\n"
    "(COMPUTER 50\n"
    "    ((0)\n"
    "        (DO COMPUTERS WORRY YOU)\n"
    "        (WHY DO YOU MENTION COMPUTERS)\n"
    "        (WHAT DO YOU THINK MACHINES HAVE TO DO WITH YOUR PROBLEM)\n"
    "        (DON'T YOU THINK COMPUTERS CAN HELP PEOPLE)\n"
    "        (WHAT ABOUT MACHINES WORRIES YOU)\n"
    "        (WHAT DO YOU THINK ABOUT MACHINES)))\n"
    "\n"
    "(MACHINE 50 (=COMPUTER))\n"
    "(MACHINES 50 (=COMPUTER))\n"
    "(COMPUTERS 50 (=COMPUTER))\n"
    "\n"
    "(AM = ARE\n"
    "    ((0 ARE YOU 0)\n"
    "        (DO YOU BELIEVE YOU ARE 4)\n"
    "        (WOULD YOU WANT TO BE 4)\n"
    "        (YOU WISH I WOULD TELL YOU YOU ARE 4)\n"
    "        (WHAT WOULD IT MEAN IF YOU WERE 4)\n"
    "        (=WHAT))\n"
    "    ((0)\n"
    "        (WHY DO YOU SAY 'AM')\n"
    "        (I DON'T UNDERSTAND THAT)))\n"
    "\n"
    "(ARE\n"
    "    ((0 ARE I 0)\n"
    "        (WHY ARE YOU INTERESTED IN WHETHER I AM 4 OR NOT)\n"
    "        (WOULD YOU PREFER IF I WEREN'T 4)\n"
    "        (PERHAPS I AM 4 IN YOUR FANTASIES)\n"
    "        (DO YOU SOMETIMES THINK I AM 4)\n"
    "        (=WHAT))\n"
    "    ((0 ARE 0)\n"
    "        (DID YOU THINK THEY MIGHT NOT BE 3)\n"
    "        (WOULD YOU LIKE IT IF THEY WERE NOT 3)\n"
    "        (WHAT IF THEY WERE NOT 3)\n"
    "        (POSSIBLY THEY ARE 3)))\n"
    "\n"
    "(YOUR = MY\n"
    "    ((0 MY 0)\n"
    "        (WHY ARE YOU CONCERNED OVER MY 3)\n"
    "        (WHAT ABOUT YOUR OWN 3)\n"
    "        (ARE YOU WORRIED ABOUT SOMEONE ELSES 3)\n"
    "        (REALLY, MY 3)))\n"
    "\n"
    "(WAS 2\n"
    "    ((0 WAS YOU 0)\n"
    "        (WHAT IF YOU WERE 4)\n"
    "        (DO YOU THINK YOU WERE 4)\n"
    "        (WERE YOU 4)\n"
    "        (WHAT WOULD IT MEAN IF YOU WERE 4)\n"
    "        (WHAT DOES ' 4 ' SUGGEST TO YOU)\n"
    "        (=WHAT))\n"
    "    ((0 YOU WAS 0)\n"
    "        (WERE YOU REALLY)\n"
    "        (WHY DO YOU TELL ME YOU WERE 4 NOW)\n"
    "; duplicate line removed: (WERE YOU REALLY) (WHY DO YOU TELL ME YOU WERE 4 NOW)\n"
    "        (PERHAPS I ALREADY KNEW YOU WERE 4))\n"
    "    ((0 WAS I 0)\n"
    "        (WOULD YOU LIKE TO BELIEVE I WAS 4)\n"
    "        (WHAT SUGGESTS THAT I WAS 4)\n"
    "        (WHAT DO YOU THINK)\n"
    "        (PERHAPS I WAS 4)\n"
    "        (WHAT IF I HAD BEEN 4))\n"
    "    ((0)\n"
    "        (NEWKEY)))\n"
    "\n"
    "(WERE = WAS (=WAS))\n"
    "(ME = YOU)\n"
    "\n"
    "(YOU'RE = I'M\n"
    "    ((0 I'M 0)\n"
    "        (PRE (I ARE 3) (=YOU))))\n"
    "\n"
    "(I'M = YOU'RE\n"
    "    ((0 YOU'RE 0)\n"
    "        (PRE (YOU ARE 3) (=I))))\n"
    "\n"
    "(MYSELF = YOURSELF)\n"
    "(YOURSELF = MYSELF)\n"
    "\n"
    "(MOTHER DLIST(/NOUN FAMILY))\n"
    "(MOM = MOTHER DLIST(/ FAMILY))\n"
    "(DAD = FATHER DLIST(/ FAMILY))\n"
    "(FATHER DLIST(/NOUN FAMILY))\n"
    "(SISTER DLIST(/FAMILY))\n"
    "(BROTHER DLIST(/FAMILY))\n"
    "(WIFE DLIST(/FAMILY))\n"
    "(CHILDREN DLIST(/FAMILY))\n"
    "\n"
    "(I = YOU\n"
    "    ((0 YOU (* WANT NEED) 0)\n"
    "        (WHAT WOULD IT MEAN TO YOU IF YOU GOT 4)\n"
    "        (WHY DO YOU WANT 4)\n"
    "        (SUPPOSE YOU GOT 4 SOON)\n"
    "        (WHAT IF YOU NEVER GOT 4)\n"
    "        (WHAT WOULD GETTING 4 MEAN TO YOU)\n"
    "        (WHAT DOES WANTING 4 HAVE TO DO WITH THIS DISCUSSION))\n"
    "    ((0 YOU ARE 0 (*SAD UNHAPPY DEPRESSED SICK ) 0)\n"
    "        (I AM SORRY TO HEAR YOU ARE 5)\n"
    "        (DO YOU THINK COMING HERE WILL HELP YOU NOT TO BE 5)\n"
    "        (I'M SURE ITS NOT PLEASANT TO BE 5)\n"
    "        (CAN YOU EXPLAIN WHAT MADE YOU 5))\n"
    "    ((0 YOU ARE 0 (*HAPPY ELATED GLAD BETTER) 0)\n"
    "        (HOW HAVE I HELPED YOU TO BE 5)\n"
    "        (HAS YOUR TREATMENT MADE YOU 5)\n"
    "        (WHAT MAKES YOU 5 JUST NOW)\n"
    "        (CAN YOU EXPLAIN WHY YOU ARE SUDDENLY 5))\n"
    "    ((0 YOU WAS 0)\n"
    "        (=WAS))\n"
    "; duplicate line removed: ((0 YOU WAS 0) (=WAS))\n"
    "    ((0 YOU (/BELIEF) YOU 0)\n"
    "        (DO YOU REALLY THINK SO)\n"
    "        (BUT YOU ARE NOT SURE YOU 5)\n"
    "        (DO YOU REALLY DOUBT YOU 5))\n"
    "    ((0 YOU 0 (/BELIEF) 0 I 0)\n"
    "        (=YOU))\n"
    "    ((0 YOU ARE 0)\n"
    "        (IS IT BECAUSE YOU ARE 4 THAT YOU CAME TO ME)\n"
    "        (HOW LONG HAVE YOU BEEN 4)\n"
    "        (DO YOU BELIEVE IT NORMAL TO BE 4)\n"
    "        (DO YOU ENJOY BEING 4))\n"
    "    ((0 YOU (* CAN'T CANNOT) 0)\n"
    "        (HOW DO YOU KNOW YOU CAN'T 4)\n"
    "        (HAVE YOU TRIED)\n"
    "        (PERHAPS YOU COULD 4 NOW)\n"
    "        (DO YOU REALLY WANT TO BE ABLE TO 4))\n"
    "    ((0 YOU DON'T 0)\n"
    "        (DON'T YOU REALLY 4)\n"
    "        (WHY DON'T YOU 4)\n"
    "        (DO YOU WISH TO BE ABLE TO 4)\n"
    "        (DOES THAT TROUBLE YOU))\n"
    "    ((0 YOU FEEL 0)\n"
    "        (TELL ME MORE ABOUT SUCH FEELINGS)\n"
    "        (DO YOU OFTEN FEEL 4)\n"
    "        (DO YOU ENJOY FEELING 4)\n"
    "        (OF WHAT DOES FEELING 4 REMIND YOU))\n"
    "    ((0 YOU 0 I 0)\n"
    "        (PERHAPS IN YOUR FANTASY WE 3 EACH OTHER)\n"
    "        (DO YOU WISH TO 3 ME)\n"
    "        (YOU SEEM TO NEED TO 3 ME)\n"
    "        (DO YOU 3 ANYONE ELSE))\n"
    "    ((0)\n"
    "        (YOU SAY 1)\n"
    "        (CAN YOU ELABORATE ON THAT)\n"
    "        (DO YOU SAY 1 FOR SOME SPECIAL REASON)\n"
    "        (THAT'S QUITE INTERESTING)))\n"
    "\n"
    "(YOU = I\n"
    "    ((0 I REMIND YOU OF 0)\n"
    "        (=DIT))\n"
    "    ((0 I ARE 0)\n"
    "        (WHAT MAKES YOU THINK I AM 4)\n"
    "        (DOES IT PLEASE YOU TO BELIEVE I AM 4)\n"
    "        (DO YOU SOMETIMES WISH YOU WERE 4)\n"
    "        (PERHAPS YOU WOULD LIKE TO BE 4))\n"
    "    ((0 I 0 YOU)\n"
    "        (WHY DO YOU THINK I 3 YOU)\n"
    "        (YOU LIKE TO THINK I 3 YOU - DON'T YOU)\n"
    "        (WHAT MAKES YOU THINK I 3 YOU)\n"
    "        (REALLY, I 3 YOU)\n"
    "        (DO YOU WISH TO BELIEVE I 3 YOU)\n"
    "; duplicate line removed: (REALLY, I 3 YOU) (DO YOU WISH TO BELIEVE I 3 YOU)\n"
    "        (SUPPOSE I DID 3 YOU - WHAT WOULD THAT MEAN)\n"
    "        (DOES SOMEONE ELSE BELIEVE I 3 YOU))\n"
    "    ((0 I 0)\n"
    "        (WE WERE DISCUSSING YOU - NOT ME)\n"
    "        (OH, I 3)\n"
    "        (YOU'RE NOT REALLY TALKING ABOUT ME - ARE YOU)\n"
    "        (WHAT ARE YOUR FEELINGS NOW)))\n"
    "\n"
    "(YES\n"
    "    ((0)\n"
    "        (YOU SEEM QUITE POSITIVE)\n"
    "        (YOU ARE SURE)\n"
    "        (I SEE)\n"
    "        (I UNDERSTAND)))\n"
    "\n"
    "(NO\n"
    "    ((0)\n"
    "        (ARE YOU SAYING 'NO' JUST TO BE NEGATIVE)\n"
    "        (YOU ARE BEING A BIT NEGATIVE)\n"
    "        (WHY NOT)\n"
    "        (WHY 'NO')))\n"
    "\n"
    "(MY = YOUR 2\n"
    "    ((0 YOUR 0 (/FAMILY) 0)\n"
    "        (TELL ME MORE ABOUT YOUR FAMILY)\n"
    "        (WHO ELSE IN YOUR FAMILY 5)\n"
    "        (YOUR 4)\n"
    "        (WHAT ELSE COMES TO MIND WHEN YOU THINK OF YOUR 4))\n"
    "    ((0 YOUR 0)\n"
    "        (YOUR 3)\n"
    "        (WHY DO YOU SAY YOUR 3)\n"
    "        (DOES THAT SUGGEST ANYTHING ELSE WHICH BELONGS TO YOU)\n"
    "        (IS IT IMPORTANT TO YOU THAT 2 3)))\n"
    "\n"
    "(CAN\n"
    "    ((0 CAN I 0)\n"
    "        (YOU BELIEVE I CAN 4 DON'T YOU)\n"
    "        (=WHAT)\n"
    "        (YOU WANT ME TO BE ABLE TO 4)\n"
    "        (PERHAPS YOU WOULD LIKE TO BE ABLE TO 4 YOURSELF))\n"
    "    ((0 CAN YOU 0)\n"
    "        (WHETHER OR NOT YOU CAN 4 DEPENDS ON YOU MORE THAN ON ME)\n"
    "        (DO YOU WANT TO BE ABLE TO 4)\n"
    "        (PERHAPS YOU DON'T WANT TO 4)\n"
    "        (=WHAT)))\n"
    "\n"
    "(WHAT\n"
    "    ((0)\n"
    "        (WHY DO YOU ASK)\n"
    "        (DOES THAT QUESTION INTEREST YOU)\n"
    "        (WHAT IS IT YOU REALLY WANT TO KNOW)\n"
    "        (ARE SUCH QUESTIONS MUCH ON YOUR MIND)\n"
    "        (WHAT ANSWER WOULD PLEASE YOU MOST)\n"
    "        (WHAT DO YOU THINK)\n"
    "        (WHAT COMES TO YOUR MIND WHEN YOU ASK THAT)\n"
    "        (HAVE YOU ASKED SUCH QUESTIONS BEFORE)\n"
    "        (HAVE YOU ASKED ANYONE ELSE)))\n"
    "\n"
    "(BECAUSE\n"
    "    ((0)\n"
    "        (IS THAT THE REAL REASON)\n"
    "        (DON'T ANY OTHER REASONS COME TO MIND)\n"
    "        (DOES THAT REASON SEEM TO EXPLAIN ANYTHING ELSE)\n"
    "        (WHAT OTHER REASONS MIGHT THERE BE)))\n"
    "\n"
    "(WHY\n"
    "    ((0 WHY DON'T I 0)\n"
    "        (DO YOU BELIEVE I DON'T 5)\n"
    "        (PERHAPS I WILL 5 IN GOOD TIME)\n"
    "        (SHOULD YOU 5 YOURSELF)\n"
    "        (YOU WANT ME TO 5)\n"
    "        (=WHAT))\n"
    "; duplicate line removed: (=WHAT))\n"
    "    ((0 WHY CAN'T YOU 0)\n"
    "        (DO YOU THINK YOU SHOULD BE ABLE TO 5)\n"
    "        (DO YOU WANT TO BE ABLE TO 5)\n"
    "        (DO YOU BELIEVE THIS WILL HELP YOU TO 5)\n"
    "        (HAVE YOU ANY IDEA WHY YOU CAN'T 5)\n"
    "        (=WHAT)))\n"
    "; extraneous (=WHAT)) removed\n"
    "; and missing final ')' added\n"
    "\n"
    "(EVERYONE 2\n"
    "    ((0 (* EVERYONE EVERYBODY NOBODY NOONE) 0)\n"
    "        (REALLY, 2)\n"
    "        (SURELY NOT 2)\n"
    "        (CAN YOU THINK OF ANYONE IN PARTICULAR)\n"
    "        (WHO, FOR EXAMPLE)\n"
    "        (YOU ARE THINKING OF A VERY SPECIAL PERSON)\n"
    "        (WHO, MAY I ASK)\n"
    "        (SOMEONE SPECIAL PERHAPS)\n"
    "        (YOU HAVE A PARTICULAR PERSON IN MIND, DON'T YOU)\n"
    "        (WHO DO YOU THINK YOU'RE TALKING ABOUT)))\n"
    "\n"
    "(EVERYBODY 2 (= EVERYONE))\n"
    "(NOBODY 2 (= EVERYONE))\n"
    "(NOONE 2 (= EVERYONE))\n"
    "\n"
    "(ALWAYS 1\n"
    "    ((0)\n"
    "        (CAN YOU THINK OF A SPECIFIC EXAMPLE)\n"
    "        (WHEN)\n"
    "        (WHAT INCIDENT ARE YOU THINKING OF)\n"
    "        (REALLY, ALWAYS)))\n"
    "\n"
    "(LIKE 10\n"
    "    ((0 (*AM IS ARE WAS) 0 LIKE 0)\n"
    "        (=DIT))\n"
    "    ((0)\n"
    "        (NEWKEY)))\n"
    "\n"
    "(DIT\n"
    "    ((0)\n"
    "        (IN WHAT WAY)\n"
    "        (WHAT RESEMBLANCE DO YOU SEE)\n"
    "        (WHAT DOES THAT SIMILARITY SUGGEST TO YOU)\n"
    "        (WHAT OTHER CONNECTIONS DO YOU SEE)\n"
    "        (WHAT DO YOU SUPPOSE THAT RESEMBLANCE MEANS)\n"
    "        (WHAT IS THE CONNECTION, DO YOU SUPPOSE)\n"
    "        (COULD THERE REALLY BE SOME CONNECTION)\n"
    "        (HOW)))\n"
    "\n"
    "()\n"
    "\n"
    "; --- End of ELIZA script ---\n";




}//namespace elizascript






 //////// //       //// ////////    ///    //////// ////////  //////  //////// 
 //       //        //       //    // //      //    //       //    //    //    
 //       //        //      //    //   //     //    //       //          //    
 //////   //        //     //    //     //    //    //////    //////     //    
 //       //        //    //     /////////    //    //             //    //    
 //       //        //   //      //     //    //    //       //    //    //    
 //////// //////// //// //////// //     //    //    ////////  //////     //    


namespace elizatest { // basic test of whether this simulation is accurate


// return a string in ELIZA script format representing given script 's'
std::string to_string(const elizascript::script & s)
{
    std::string result;
    result += "(" + join(s.hello_message) + ")\n";
    result += "START\n";
    for (const auto & r : s.rules)
        result += r.second->to_string();
    result += "()\n";
    return result;
}


// perform basic checks on implementation
void test()
{
    RUN_TESTS(); // run the tests defined with DEF_TEST_FUNC


    // script_text is logically identical to the script in the CACM article
    // appendix, but the ordering and whitespace is different so that it can
    // be checked against the output of elizatest::to_string()
    const char * script_text =
        "(HOW DO YOU DO. PLEASE TELL ME YOUR PROBLEM)\n"
        "START\n"
        "(ALIKE 10 (= DIT))\n"
        "(ALWAYS 1\n"
        "    ((0)\n"
        "        (CAN YOU THINK OF A SPECIFIC EXAMPLE)\n"
        "        (WHEN)\n"
        "        (WHAT INCIDENT ARE YOU THINKING OF)\n"
        "        (REALLY, ALWAYS)))\n"
        "(AM = ARE\n"
        "    ((0 ARE YOU 0)\n"
        "        (DO YOU BELIEVE YOU ARE 4)\n"
        "        (WOULD YOU WANT TO BE 4)\n"
        "        (YOU WISH I WOULD TELL YOU YOU ARE 4)\n"
        "        (WHAT WOULD IT MEAN IF YOU WERE 4)\n"
        "        (=WHAT))\n"
        "    ((0)\n"
        "        (WHY DO YOU SAY 'AM')\n"
        "        (I DON'T UNDERSTAND THAT)))\n"
        "(ARE\n"
        "    ((0 ARE I 0)\n"
        "        (WHY ARE YOU INTERESTED IN WHETHER I AM 4 OR NOT)\n"
        "        (WOULD YOU PREFER IF I WEREN'T 4)\n"
        "        (PERHAPS I AM 4 IN YOUR FANTASIES)\n"
        "        (DO YOU SOMETIMES THINK I AM 4)\n"
        "        (=WHAT))\n"
        "    ((0 ARE 0)\n"
        "        (DID YOU THINK THEY MIGHT NOT BE 3)\n"
        "        (WOULD YOU LIKE IT IF THEY WERE NOT 3)\n"
        "        (WHAT IF THEY WERE NOT 3)\n"
        "        (POSSIBLY THEY ARE 3)))\n"
        "(BECAUSE\n"
        "    ((0)\n"
        "        (IS THAT THE REAL REASON)\n"
        "        (DON'T ANY OTHER REASONS COME TO MIND)\n"
        "        (DOES THAT REASON SEEM TO EXPLAIN ANYTHING ELSE)\n"
        "        (WHAT OTHER REASONS MIGHT THERE BE)))\n"
        "(BELIEVE DLIST(/BELIEF))\n"
        "(BROTHER DLIST(/FAMILY))\n"
        "(CAN\n"
        "    ((0 CAN I 0)\n"
        "        (YOU BELIEVE I CAN 4 DON'T YOU)\n"
        "        (=WHAT)\n"
        "        (YOU WANT ME TO BE ABLE TO 4)\n"
        "        (PERHAPS YOU WOULD LIKE TO BE ABLE TO 4 YOURSELF))\n"
        "    ((0 CAN YOU 0)\n"
        "        (WHETHER OR NOT YOU CAN 4 DEPENDS ON YOU MORE THAN ON ME)\n"
        "        (DO YOU WANT TO BE ABLE TO 4)\n"
        "        (PERHAPS YOU DON'T WANT TO 4)\n"
        "        (=WHAT)))\n"
        "(CANT = CAN'T)\n"
        "(CERTAINLY (= YES))\n"
        "(CHILDREN DLIST(/FAMILY))\n"
        "(COMPUTER 50\n"
        "    ((0)\n"
        "        (DO COMPUTERS WORRY YOU)\n"
        "        (WHY DO YOU MENTION COMPUTERS)\n"
        "        (WHAT DO YOU THINK MACHINES HAVE TO DO WITH YOUR PROBLEM)\n"
        "        (DON'T YOU THINK COMPUTERS CAN HELP PEOPLE)\n"
        "        (WHAT ABOUT MACHINES WORRIES YOU)\n"
        "        (WHAT DO YOU THINK ABOUT MACHINES)))\n"
        "(COMPUTERS 50 (= COMPUTER))\n"
        "(DAD = FATHER DLIST(/ FAMILY))\n"
        "(DEUTSCH (= XFREMD))\n"
        "(DIT\n"
        "    ((0)\n"
        "        (IN WHAT WAY)\n"
        "        (WHAT RESEMBLANCE DO YOU SEE)\n"
        "        (WHAT DOES THAT SIMILARITY SUGGEST TO YOU)\n"
        "        (WHAT OTHER CONNECTIONS DO YOU SEE)\n"
        "        (WHAT DO YOU SUPPOSE THAT RESEMBLANCE MEANS)\n"
        "        (WHAT IS THE CONNECTION, DO YOU SUPPOSE)\n"
        "        (COULD THERE REALLY BE SOME CONNECTION)\n"
        "        (HOW)))\n"
        "(DONT = DON'T)\n"
        "(DREAM 3\n"
        "    ((0)\n"
        "        (WHAT DOES THAT DREAM SUGGEST TO YOU)\n"
        "        (DO YOU DREAM OFTEN)\n"
        "        (WHAT PERSONS APPEAR IN YOUR DREAMS)\n"
        "        (DON'T YOU BELIEVE THAT DREAM HAS SOMETHING TO DO WITH YOUR PROBLEM)\n"
        "        (NEWKEY)))\n"
        "(DREAMED = DREAMT 4 (= DREAMT))\n"
        "(DREAMS = DREAM 3 (= DREAM))\n"
        "(DREAMT 4\n"
        "    ((0 YOU DREAMT 0)\n"
        "        (REALLY, 4)\n"
        "        (HAVE YOU EVER FANTASIED 4 WHILE YOU WERE AWAKE)\n"
        "        (HAVE YOU DREAMT 4 BEFORE)\n"
        "        (=DREAM)\n"
        "        (NEWKEY)))\n"
        "(ESPANOL (= XFREMD))\n"
        "(EVERYBODY 2 (= EVERYONE))\n"
        "(EVERYONE 2\n"
        "    ((0 (* EVERYONE EVERYBODY NOBODY NOONE) 0)\n"
        "        (REALLY, 2)\n"
        "        (SURELY NOT 2)\n"
        "        (CAN YOU THINK OF ANYONE IN PARTICULAR)\n"
        "        (WHO, FOR EXAMPLE)\n"
        "        (YOU ARE THINKING OF A VERY SPECIAL PERSON)\n"
        "        (WHO, MAY I ASK)\n"
        "        (SOMEONE SPECIAL PERHAPS)\n"
        "        (YOU HAVE A PARTICULAR PERSON IN MIND, DON'T YOU)\n"
        "        (WHO DO YOU THINK YOU'RE TALKING ABOUT)))\n"
        "(FATHER DLIST(/NOUN FAMILY))\n"
        "(FEEL DLIST(/BELIEF))\n"
        "(FRANCAIS (= XFREMD))\n"
        "(HELLO\n"
        "    ((0)\n"
        "        (HOW DO YOU DO. PLEASE STATE YOUR PROBLEM)))\n"
        "(HOW (= WHAT))\n"
        "(I = YOU\n"
        "    ((0 YOU (* WANT NEED) 0)\n"
        "        (WHAT WOULD IT MEAN TO YOU IF YOU GOT 4)\n"
        "        (WHY DO YOU WANT 4)\n"
        "        (SUPPOSE YOU GOT 4 SOON)\n"
        "        (WHAT IF YOU NEVER GOT 4)\n"
        "        (WHAT WOULD GETTING 4 MEAN TO YOU)\n"
        "        (WHAT DOES WANTING 4 HAVE TO DO WITH THIS DISCUSSION))\n"
        "    ((0 YOU ARE 0 (*SAD UNHAPPY DEPRESSED SICK) 0)\n"
        "        (I AM SORRY TO HEAR YOU ARE 5)\n"
        "        (DO YOU THINK COMING HERE WILL HELP YOU NOT TO BE 5)\n"
        "        (I'M SURE ITS NOT PLEASANT TO BE 5)\n"
        "        (CAN YOU EXPLAIN WHAT MADE YOU 5))\n"
        "    ((0 YOU ARE 0 (*HAPPY ELATED GLAD BETTER) 0)\n"
        "        (HOW HAVE I HELPED YOU TO BE 5)\n"
        "        (HAS YOUR TREATMENT MADE YOU 5)\n"
        "        (WHAT MAKES YOU 5 JUST NOW)\n"
        "        (CAN YOU EXPLAIN WHY YOU ARE SUDDENLY 5))\n"
        "    ((0 YOU WAS 0)\n"
        "        (=WAS))\n"
        "    ((0 YOU (/BELIEF) YOU 0)\n"
        "        (DO YOU REALLY THINK SO)\n"
        "        (BUT YOU ARE NOT SURE YOU 5)\n"
        "        (DO YOU REALLY DOUBT YOU 5))\n"
        "    ((0 YOU 0 (/BELIEF) 0 I 0)\n"
        "        (=YOU))\n"
        "    ((0 YOU ARE 0)\n"
        "        (IS IT BECAUSE YOU ARE 4 THAT YOU CAME TO ME)\n"
        "        (HOW LONG HAVE YOU BEEN 4)\n"
        "        (DO YOU BELIEVE IT NORMAL TO BE 4)\n"
        "        (DO YOU ENJOY BEING 4))\n"
        "    ((0 YOU (* CAN'T CANNOT) 0)\n"
        "        (HOW DO YOU KNOW YOU CAN'T 4)\n"
        "        (HAVE YOU TRIED)\n"
        "        (PERHAPS YOU COULD 4 NOW)\n"
        "        (DO YOU REALLY WANT TO BE ABLE TO 4))\n"
        "    ((0 YOU DON'T 0)\n"
        "        (DON'T YOU REALLY 4)\n"
        "        (WHY DON'T YOU 4)\n"
        "        (DO YOU WISH TO BE ABLE TO 4)\n"
        "        (DOES THAT TROUBLE YOU))\n"
        "    ((0 YOU FEEL 0)\n"
        "        (TELL ME MORE ABOUT SUCH FEELINGS)\n"
        "        (DO YOU OFTEN FEEL 4)\n"
        "        (DO YOU ENJOY FEELING 4)\n"
        "        (OF WHAT DOES FEELING 4 REMIND YOU))\n"
        "    ((0 YOU 0 I 0)\n"
        "        (PERHAPS IN YOUR FANTASY WE 3 EACH OTHER)\n"
        "        (DO YOU WISH TO 3 ME)\n"
        "        (YOU SEEM TO NEED TO 3 ME)\n"
        "        (DO YOU 3 ANYONE ELSE))\n"
        "    ((0)\n"
        "        (YOU SAY 1)\n"
        "        (CAN YOU ELABORATE ON THAT)\n"
        "        (DO YOU SAY 1 FOR SOME SPECIAL REASON)\n"
        "        (THAT'S QUITE INTERESTING)))\n"
        "(I'M = YOU'RE\n"
        "    ((0 YOU'RE 0)\n"
        "        (PRE (YOU ARE 3) (=I))))\n"
        "(IF 3\n"
        "    ((0 IF 0)\n"
        "        (DO YOU THINK ITS LIKELY THAT 3)\n"
        "        (DO YOU WISH THAT 3)\n"
        "        (WHAT DO YOU THINK ABOUT 3)\n"
        "        (REALLY, 2 3)))\n"
        "(ITALIANO (= XFREMD))\n"
        "(LIKE 10\n"
        "    ((0 (*AM IS ARE WAS) 0 LIKE 0)\n"
        "        (=DIT))\n"
        "    ((0)\n"
        "        (NEWKEY)))\n"
        "(MACHINE 50 (= COMPUTER))\n"
        "(MACHINES 50 (= COMPUTER))\n"
        "(MAYBE (= PERHAPS))\n"
        "(ME = YOU)\n"
        "(MOM = MOTHER DLIST(/ FAMILY))\n"
        "(MOTHER DLIST(/NOUN FAMILY))\n"
        "(MY = YOUR 2\n"
        "    ((0 YOUR 0 (/FAMILY) 0)\n"
        "        (TELL ME MORE ABOUT YOUR FAMILY)\n"
        "        (WHO ELSE IN YOUR FAMILY 5)\n"
        "        (YOUR 4)\n"
        "        (WHAT ELSE COMES TO MIND WHEN YOU THINK OF YOUR 4))\n"
        "    ((0 YOUR 0)\n"
        "        (YOUR 3)\n"
        "        (WHY DO YOU SAY YOUR 3)\n"
        "        (DOES THAT SUGGEST ANYTHING ELSE WHICH BELONGS TO YOU)\n"
        "        (IS IT IMPORTANT TO YOU THAT 2 3)))\n"
        "(MYSELF = YOURSELF)\n"
        "(NAME 15\n"
        "    ((0)\n"
        "        (I AM NOT INTERESTED IN NAMES)\n"
        "        (I'VE TOLD YOU BEFORE, I DON'T CARE ABOUT NAMES - PLEASE CONTINUE)))\n"
        "(NO\n"
        "    ((0)\n"
        "        (ARE YOU SAYING 'NO' JUST TO BE NEGATIVE)\n"
        "        (YOU ARE BEING A BIT NEGATIVE)\n"
        "        (WHY NOT)\n"
        "        (WHY 'NO')))\n"
        "(NOBODY 2 (= EVERYONE))\n"
        "(NOONE 2 (= EVERYONE))\n"
        "(PERHAPS\n"
        "    ((0)\n"
        "        (YOU DON'T SEEM QUITE CERTAIN)\n"
        "        (WHY THE UNCERTAIN TONE)\n"
        "        (CAN'T YOU BE MORE POSITIVE)\n"
        "        (YOU AREN'T SURE)\n"
        "        (DON'T YOU KNOW)))\n"
        "(REMEMBER 5\n"
        "    ((0 YOU REMEMBER 0)\n"
        "        (DO YOU OFTEN THINK OF 4)\n"
        "        (DOES THINKING OF 4 BRING ANYTHING ELSE TO MIND)\n"
        "        (WHAT ELSE DO YOU REMEMBER)\n"
        "        (WHY DO YOU REMEMBER 4 JUST NOW)\n"
        "        (WHAT IN THE PRESENT SITUATION REMINDS YOU OF 4)\n"
        "        (WHAT IS THE CONNECTION BETWEEN ME AND 4))\n"
        "    ((0 DO I REMEMBER 0)\n"
        "        (DID YOU THINK I WOULD FORGET 5)\n"
        "        (WHY DO YOU THINK I SHOULD RECALL 5 NOW)\n"
        "        (WHAT ABOUT 5)\n"
        "        (=WHAT)\n"
        "        (YOU MENTIONED 5))\n"
        "    ((0)\n"
        "        (NEWKEY)))\n"
        "(SAME 10 (= DIT))\n"
        "(SISTER DLIST(/FAMILY))\n"
        "(SORRY\n"
        "    ((0)\n"
        "        (PLEASE DON'T APOLIGIZE)\n"
        "        (APOLOGIES ARE NOT NECESSARY)\n"
        "        (WHAT FEELINGS DO YOU HAVE WHEN YOU APOLOGIZE)\n"
        "        (I'VE TOLD YOU THAT APOLOGIES ARE NOT REQUIRED)))\n"
        "(THINK DLIST(/BELIEF))\n"
        "(WAS 2\n"
        "    ((0 WAS YOU 0)\n"
        "        (WHAT IF YOU WERE 4)\n"
        "        (DO YOU THINK YOU WERE 4)\n"
        "        (WERE YOU 4)\n"
        "        (WHAT WOULD IT MEAN IF YOU WERE 4)\n"
        "        (WHAT DOES ' 4 ' SUGGEST TO YOU)\n"
        "        (=WHAT))\n"
        "    ((0 YOU WAS 0)\n"
        "        (WERE YOU REALLY)\n"
        "        (WHY DO YOU TELL ME YOU WERE 4 NOW)\n"
        "        (PERHAPS I ALREADY KNEW YOU WERE 4))\n"
        "    ((0 WAS I 0)\n"
        "        (WOULD YOU LIKE TO BELIEVE I WAS 4)\n"
        "        (WHAT SUGGESTS THAT I WAS 4)\n"
        "        (WHAT DO YOU THINK)\n"
        "        (PERHAPS I WAS 4)\n"
        "        (WHAT IF I HAD BEEN 4))\n"
        "    ((0)\n"
        "        (NEWKEY)))\n"
        "(WERE = WAS (= WAS))\n"
        "(WHAT\n"
        "    ((0)\n"
        "        (WHY DO YOU ASK)\n"
        "        (DOES THAT QUESTION INTEREST YOU)\n"
        "        (WHAT IS IT YOU REALLY WANT TO KNOW)\n"
        "        (ARE SUCH QUESTIONS MUCH ON YOUR MIND)\n"
        "        (WHAT ANSWER WOULD PLEASE YOU MOST)\n"
        "        (WHAT DO YOU THINK)\n"
        "        (WHAT COMES TO YOUR MIND WHEN YOU ASK THAT)\n"
        "        (HAVE YOU ASKED SUCH QUESTIONS BEFORE)\n"
        "        (HAVE YOU ASKED ANYONE ELSE)))\n"
        "(WHEN (= WHAT))\n"
        "(WHY\n"
        "    ((0 WHY DON'T I 0)\n"
        "        (DO YOU BELIEVE I DON'T 5)\n"
        "        (PERHAPS I WILL 5 IN GOOD TIME)\n"
        "        (SHOULD YOU 5 YOURSELF)\n"
        "        (YOU WANT ME TO 5)\n"
        "        (=WHAT))\n"
        "    ((0 WHY CAN'T YOU 0)\n"
        "        (DO YOU THINK YOU SHOULD BE ABLE TO 5)\n"
        "        (DO YOU WANT TO BE ABLE TO 5)\n"
        "        (DO YOU BELIEVE THIS WILL HELP YOU TO 5)\n"
        "        (HAVE YOU ANY IDEA WHY YOU CAN'T 5)\n"
        "        (=WHAT)))\n"
        "(WIFE DLIST(/FAMILY))\n"
        "(WISH DLIST(/BELIEF))\n"
        "(WONT = WON'T)\n"
        "(XFREMD\n"
        "    ((0)\n"
        "        (I AM SORRY, I SPEAK ONLY ENGLISH)))\n"
        "(YES\n"
        "    ((0)\n"
        "        (YOU SEEM QUITE POSITIVE)\n"
        "        (YOU ARE SURE)\n"
        "        (I SEE)\n"
        "        (I UNDERSTAND)))\n"
        "(YOU = I\n"
        "    ((0 I REMIND YOU OF 0)\n"
        "        (=DIT))\n"
        "    ((0 I ARE 0)\n"
        "        (WHAT MAKES YOU THINK I AM 4)\n"
        "        (DOES IT PLEASE YOU TO BELIEVE I AM 4)\n"
        "        (DO YOU SOMETIMES WISH YOU WERE 4)\n"
        "        (PERHAPS YOU WOULD LIKE TO BE 4))\n"
        "    ((0 I 0 YOU)\n"
        "        (WHY DO YOU THINK I 3 YOU)\n"
        "        (YOU LIKE TO THINK I 3 YOU - DON'T YOU)\n"
        "        (WHAT MAKES YOU THINK I 3 YOU)\n"
        "        (REALLY, I 3 YOU)\n"
        "        (DO YOU WISH TO BELIEVE I 3 YOU)\n"
        "        (SUPPOSE I DID 3 YOU - WHAT WOULD THAT MEAN)\n"
        "        (DOES SOMEONE ELSE BELIEVE I 3 YOU))\n"
        "    ((0 I 0)\n"
        "        (WE WERE DISCUSSING YOU - NOT ME)\n"
        "        (OH, I 3)\n"
        "        (YOU'RE NOT REALLY TALKING ABOUT ME - ARE YOU)\n"
        "        (WHAT ARE YOUR FEELINGS NOW)))\n"
        "(YOU'RE = I'M\n"
        "    ((0 I'M 0)\n"
        "        (PRE (I ARE 3) (=YOU))))\n"
        "(YOUR = MY\n"
        "    ((0 MY 0)\n"
        "        (WHY ARE YOU CONCERNED OVER MY 3)\n"
        "        (WHAT ABOUT YOUR OWN 3)\n"
        "        (ARE YOU WORRIED ABOUT SOMEONE ELSES 3)\n"
        "        (REALLY, MY 3)))\n"
        "(YOURSELF = MYSELF)\n"
        "(MEMORY MY\n"
        "    (0 YOUR 0 = LETS DISCUSS FURTHER WHY YOUR 3)\n"
        "    (0 YOUR 0 = EARLIER YOU SAID YOUR 3)\n"
        "    (0 YOUR 0 = BUT YOUR 3)\n"
        "    (0 YOUR 0 = DOES THAT HAVE ANYTHING TO DO WITH THE FACT THAT YOUR 3))\n"
        "(NONE\n"
        "    ((0)\n"
        "        (I AM NOT SURE I UNDERSTAND YOU FULLY)\n"
        "        (PLEASE GO ON)\n"
        "        (WHAT DOES THAT SUGGEST TO YOU)\n"
        "        (DO YOU FEEL STRONGLY ABOUT DISCUSSING SUCH THINGS)))\n"
        "()\n";


    std::stringstream ss(elizascript::CACM_1966_01_DOCTOR_script);
    elizascript::script s;
    elizascript::read<std::stringstream>(ss, s);

    TEST_EQUAL(s.rules.size(), (size_t)68);
    TEST_EQUAL(to_string(s), script_text);

    elizalogic::tagmap tags(elizalogic::collect_tags(s.rules));
    TEST_EQUAL(tags.size(), (size_t)3);
    TEST_EQUAL(join(tags["BELIEF"]), "BELIEVE FEEL THINK WISH");
    TEST_EQUAL(join(tags["FAMILY"]), "BROTHER CHILDREN DAD FATHER MOM MOTHER SISTER WIFE");
    TEST_EQUAL(join(tags["NOUN"]), "FATHER MOTHER");


    struct exchange {
        const char * prompt;    // input to ELIZA
        const char * response;  // output expected from ELIZA
    };
    const exchange conversation[] = {
        // --- exact conversation quoted in 1966 CACM article ---
        { "Men are all alike.",
          "IN WHAT WAY" },

        { "They're always bugging us about something or other.",
          "CAN YOU THINK OF A SPECIFIC EXAMPLE" },

        { "Well, my boyfriend made me come here.",
          "YOUR BOYFRIEND MADE YOU COME HERE" },

        { "He says I'm depressed much of the time.",
          "I AM SORRY TO HEAR YOU ARE DEPRESSED" },

        { "It's true. I am unhappy.",
          "DO YOU THINK COMING HERE WILL HELP YOU NOT TO BE UNHAPPY" },

        { "I need some help, that much seems certain.",
          "WHAT WOULD IT MEAN TO YOU IF YOU GOT SOME HELP" },

        { "Perhaps I could learn to get along with my mother.",
          "TELL ME MORE ABOUT YOUR FAMILY" },

        { "My mother takes care of me.",
          "WHO ELSE IN YOUR FAMILY TAKES CARE OF YOU" },

        { "My father.",
          "YOUR FATHER" },

        { "You are like my father in some ways.",
          "WHAT RESEMBLANCE DO YOU SEE" },

        // In the CACM article, the following prompt contained no comma.
        // Without the comma, this simulation responds "WHAT MAKES YOU
        // THINK I AM NOT VERY AGGRESSIVE BUT YOU THINK I DON'T WANT YOU TO
        // NOTICE THAT". I assume the comma got lost from the CACM article.
        //{ "You are not very aggressive, but I think you don't want me to notice that.",
        //
        // UPDATE: We now have a version of Weizenbaum's original MAD-Slip
        // source code where we see that the word "BUT" is also considered
        // to be a delimeter. So I was wrong to assume a missing comma.
        { "You are not very aggressive but I think you don't want me to notice that.",
          "WHAT MAKES YOU THINK I AM NOT VERY AGGRESSIVE" },

        { "You don't argue with me.",
          "WHY DO YOU THINK I DON'T ARGUE WITH YOU" },

        { "You are afraid of me.",
          "DOES IT PLEASE YOU TO BELIEVE I AM AFRAID OF YOU" },

        { "My father is afraid of everybody.",
          "WHAT ELSE COMES TO MIND WHEN YOU THINK OF YOUR FATHER" },

        { "Bullies.",
          "DOES THAT HAVE ANYTHING TO DO WITH THE FACT THAT YOUR BOYFRIEND MADE YOU COME HERE" },
        // --- end of 1966 CACM article conversation ---
    };

    /*  I believe that if this code reproduces the exact conversation
        published in the CACM article, using the mechanisms described in
        that article, it is not unreasonable to suppose it is a fairly
        accurate simulation of the original ELIZA.
    */
    elizalogic::eliza eliza(s.rules);
    for (const auto & exchg : conversation)
        TEST_EQUAL(eliza.response(exchg.prompt), exchg.response);
}

}//namespace elizatest



// write given 's' to std::cout, followed by newline
void writeln(const std::string & s)
{
    if (false) {
        // for fun, output 's' as if ELIZA was running on a 1966 Teletype
        auto sleep = [](long ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); };
        using std::cout; using std::flush; using std::endl;

        for (const auto c : s) {
            cout << c << flush;
            sleep(100);
        }

        cout << endl;
    }
    else {
        // spit out all of 's' in an instant
        std::cout << s << std::endl;
    }
}



#if defined(_WIN32)
const std::string option_escape("/");
#else
const std::string option_escape("--");
#endif

bool is_option(const std::string s)
{
    return s.compare(0, option_escape.size(), option_escape) == 0;
}

bool is_option(const std::string s, const std::string opt)
{
    return s.size() == option_escape.size() + opt.size()
        && s.compare(0, option_escape.size(), option_escape) == 0
        && s.compare(option_escape.size(), opt.size(), opt) == 0;
}

std::string as_option(std::string o)
{
    return option_escape + o;
}


int main(int argc, const char * argv[])
{
    try {
        std::cout <<
            "-----------------------------------------------------------------\n"
            "      ELIZA -- A Computer Program for the Study of Natural\n"
            "         Language Communication Between Man and Machine\n"
            "            DOCTOR script (c) Joseph Weizenbaum, 1966\n"
            "This implementation by Anthony Hay, 2022  (CC0 1.0) Public Domain\n"
            "-----------------------------------------------------------------\n";

        elizatest::test();

        elizascript::script s;
        if (argc == 1) {
            // use default 'internal' 1966 CACM published script
            std::cout
                << "ELIZA " << as_option("help") << " for usage.\n"
                << "Using Weizenbaum's 1966 DOCTOR script.\n"
                << "Enter a blank line to quit."
                << "\n\n\n";
            std::stringstream ss(elizascript::CACM_1966_01_DOCTOR_script);
            elizascript::read<std::stringstream>(ss, s);
        }
        else if (argc == 2 && as_option("showscript") == argv[1]) {
            // just output Weizenbaum's DOCTOR script
            std::cout << elizascript::CACM_1966_01_DOCTOR_script;
            return EXIT_SUCCESS;
        }
        else if (argc == 2 && !is_option(argv[1])) {
            // use the named script file
            std::ifstream script_file(argv[1]);
            if (!script_file.is_open()) {
                std::cerr << argv[0]
                    << ": failed to open script file '" << argv[1] << "'\n";
                return EXIT_FAILURE;
            }
            std::cout
                << "Using given script file '" << argv[1] << "'\n\n\n";
            elizascript::read<std::ifstream>(script_file, s);
        }
        else {
            std::cerr
                << "Usage: ELIZA [" << as_option("showscript") << " | <filename>]\n"
                << "  where\n"
                << "    " << as_option("showscript") << " dump Weizenbaum's 1966 DOCTOR script to stcout\n"
                << "                e.g. ELIZA " << as_option("showscript") << " > script.txt\n"
                << "    <filename>  use named script file\n"
                << "                e.g. ELIZA script.txt\n";
            return EXIT_FAILURE;
        }


        writeln(join(s.hello_message));
        for (elizalogic::eliza eliza(std::move(s.rules));;) {
            std::cout << std::endl;
            std::string userinput;
            std::getline(std::cin, userinput);
            if (userinput.empty())
                break;
            writeln(eliza.response(userinput));
        }
    }
    catch (const std::exception & e) {
        std::cerr << "exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    catch (...) {
        std::cerr << "exception" << std::endl;
        return EXIT_FAILURE;
    }
}

// (The goal was to make only a minimum viable accurate simulation
// of the original 1966 ELIZA rather than a polished product.)
