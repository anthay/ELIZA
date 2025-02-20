/*  This is a recreation of Joseph Weizenbaum's 1966 ELIZA. The code was
    written from scratch originally using only the description in the
    Weizenbaum's paper on page 36 of the January 1966 edition of
    Communications of the ACM as a guide.

    If given the same S-expression-like script from the appendix of that
    paper, and given the same prompt text, the conversation it generates
    is identical to the conversation shown in the paper.

    I made this for my amusement and hereby place it in the public domain
    or, if you prefer, I release it under either CC0 1.0 Universal or the
    MIT License.
    
    Anthony Hay, 2021, Devon, UK


    Update: In April 2021 Jeff Shrager obtained a listing of ELIZA from
    Weizenbaum's MIT archive. In May 2021 he got permission from
    Weizenbaum's estate to open source the code under CC0 1.0. Seeing
    his original code clarified some behavior that was not fully
    documented in the CACM paper. (Although the listing was not dated
    and appears to differ from the functionality described in the CACM
    paper.) I made changes to this code to reflect what I learned. These
    changes are documented in the code.
    Anthony Hay, 2022, Devon, UK

    Update: In April 2022 Jeff Shrager located the source code to SLIP,
    including the HASH function. I made hash used in this code use the
    same algorithm.

    Note: In the code below there are occasional references to
    Joseph Weizenbaum's 1966 CACM article. A reference like
    [page X (Y)] refers to paragraph Y on page X of that publication.
*/


#ifdef SUPPORT_SERIAL_IO
#include "serial_io.h"
#endif

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
#include <algorithm>
#include <deque>
#include <cctype>
#include <thread>
#include <array>
#include <cstdint>



/*  Wiezenbaum wrote ELIZA in a language called MAD-SLIP.

    He also developed SLIP ("Symmetric List Processor"),
    a library of functions used to manipulate doubly-linked
    lists of cells, where a cell may contain either a datum
    or a reference to another list.

    This code doesn't use SLIP. The type stringlist is
    used where the original ELIZA might have used a SLIP
    list. (stringlist is not equivalent to a SLIP list.) */
using stringlist = std::deque<std::string>;


// (needed for unit test purposes only)
std::ostream & operator<<(std::ostream & os, const stringlist & list)
{
    os << '(';
    for (auto s : list)
        os << '"' << s << '"' << ' ';
    os << ')';
    return os;
}


namespace micro_test_library {

    /*  Define test functions with DEF_TEST_FUNC(test_func).
        Use TEST_EQUAL(value, expected_value) to test expected outcomes.
        Execute all test functions with RUN_TESTS(). */


unsigned test_count;      // total number of tests executed
unsigned fault_count;     // total number of tests that fail
std::vector<void (*)()> test_routines; // list of all test routines


// write a message to std::cout if !(value == expected_value)
template<typename A, typename B>
void test_equal(const A & value, const B & expected_value,
    const char * filename, const size_t line_num, const char * function_name)
{
    ++test_count;
    if (!(value == expected_value)) {
        ++fault_count;
        // e.g. love.cpp(2021) : in proposal() expected 'Yes!', but got 'Hahaha'
        std::cout
            << filename << '(' << line_num
            << ") : in " << function_name
            << "() expected '" << expected_value
            << "', but got '" << value
            << "'\n";
    }
}


// register a test function; return an arbitrary value
size_t add_test(void (*f)())
{
    test_routines.push_back(f);
    return test_routines.size();
}


// run all registered tests
void run_tests()
{
    for (auto & t : test_routines)
        t();
    if (fault_count)
        std::cout << fault_count << " total failures\n";
}


// write a message to std::cout if !(value == expected_value)
#define TEST_EQUAL(value, expected_value)                   \
{                                                           \
    micro_test_library::test_equal(value, expected_value,   \
        __FILE__, __LINE__, __FUNCTION__);                  \
}


// To allow test code to be placed nearby code being tested, test functions
// may be defined with this macro. All such functions may then be called
// with one call to RUN_TESTS(). Each test function must have a unique name.
#define DEF_TEST_FUNC(test_func)                                         \
void test_func();                                                        \
size_t micro_test_##test_func = micro_test_library::add_test(test_func); \
void test_func()


// execute all the DEF_TEST_FUNC defined functions
#define RUN_TESTS() micro_test_library::run_tests()

} //namespace micro_test_library


// remove front element of given container and return it
template<typename T>
auto pop_front(T & container)
{
    auto v(container.front());
    container.pop_front();
    return v;
}


// pop_front() specialised for std::string
template<>
auto pop_front(std::string & container)
{
    auto v(container.front());
    container.erase(0, 1);
    return v;
}


// return given string s in uppercase
std::string to_upper(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); }
    );
    return s;
}


// return given string s split into a list of "words"; space delimits words
// e.g. split("one   two, three.") -> ["one", "two,", "three."]
stringlist split(const std::string & s)
{
    stringlist result;
    std::string word;
    for (auto ch : s) {
        if (ch == ' ') {
            if (!word.empty()) {
                result.push_back(word);
                word.clear();
            }
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
    const stringlist r1{ "one", "two,", "three,,", "don't." };
    TEST_EQUAL(split("one   two, three,, don't."), r1);
    TEST_EQUAL(split(" one two, three,, don't. "), r1);
}


// return given words joined into one space separated string
// e.g. join(["one", "two", "", "3"]) -> "one two 3"
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
    TEST_EQUAL(join({  }), "");
    TEST_EQUAL(join({ "ELIZA" }), "ELIZA");
    TEST_EQUAL(join({ "one", "two", "", "3" }), "one two 3");
    // There is currently no requirement that commas and periods
    // be attached to preceding words because in this code join()
    // is never called upon to do this.
    // TEST_EQUAL(join({ "one", ",", "two" }), "one, two");
}



//////// //       //// ////////    ///    //        ///////   //////   ////  //////  
//       //        //       //    // //   //       //     // //    //   //  //    // 
//       //        //      //    //   //  //       //     // //         //  //       
//////   //        //     //    //     // //       //     // //   ////  //  //       
//       //        //    //     ///////// //       //     // //    //   //  //       
//       //        //   //      //     // //       //     // //    //   //  //    // 
//////// //////// //// //////// //     // ////////  ///////   //////   ////  //////  


namespace elizalogic {

// map tag word -> associated words
// e.g. "BELIEF" -> ("BELIEVE" "FEEL" "THINK" "WISH")
using tagmap = std::map<std::string, stringlist>;


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

        The following array is derived from the above mentioned table, with
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
            (Available online from Google Books. Search for PRIME.) */
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
// ('iff' is short for 'if and only if')
bool hollerith_defined(char c)
{
    static_assert(std::numeric_limits<unsigned char>::min() == 0);
    static_assert(std::numeric_limits<unsigned char>::max() == 255);

    return hollerith_encoding[static_cast<unsigned char>(c)] != hollerith_undefined;
}


std::u32string utf8_to_utf32(const std::string & utf8_string)
{
    std::u32string s;

    const size_t len = utf8_string.size();
    for (size_t i = 0; i < len; ) {
        uint32_t c32 = 0;
        size_t trailing = 0;
        unsigned char c = utf8_string[i++];

        if ((c & 0x80) == 0x00) {
            c32 = static_cast<uint32_t>(c);
            // no trailing bytes
        }
        else if ((c & 0xE0) == 0xC0) {
            c32 = static_cast<uint32_t>(c - 0xC0);
            trailing = 1;
        }
        else if ((c & 0xF0) == 0xE0) {
            c32 = static_cast<uint32_t>(c - 0xE0);
            trailing = 2;
        }
        else if ((c & 0xF8) == 0xF0) {
            c32 = static_cast<uint32_t>(c - 0xF0);
            trailing = 3;
        }
        else if ((c & 0xFC) == 0xF8) {
            c32 = static_cast<uint32_t>(c - 0xF8);
            trailing = 4;
        }
        else if ((c & 0xFE) == 0xFC) {
            c32 = static_cast<uint32_t>(c - 0xFC);
            trailing = 5;
        }
        else
            throw std::runtime_error("utf8_to_utf32: invalid lead byte");

        if (trailing > len - i)
            throw std::runtime_error("utf8_to_utf32: missing trail byte");

        while (trailing--) {
            c = utf8_string[i++];
            if ((c & 0xC0) != 0x80)
                throw std::runtime_error("utf8_to_utf32: invalid trail byte");
            c32 <<= 6;
            c32 |= static_cast<uint32_t>(c & 0x3F);
        }

        s += c32;
    }

    return s;
}


std::string utf32_to_utf8(uint32_t c32)
{
    std::string result;
    if (c32 <= 0x7F) {
        result += static_cast<char>(c32);                           // 0xxxxxxx
    }
    else if (c32 <= 0x7FF) {
        result += static_cast<char>(0xC0 | (c32 >> 6));             // 110xxxxx
        result += static_cast<char>(0x80 | (c32 & 0x3F));           // 10xxxxxx
    }
    else if (c32 <= 0xFFFF) {
        result += static_cast<char>(0xE0 | (c32 >> 12));            // 1110xxxx
        result += static_cast<char>(0x80 | ((c32 >> 6) & 0x3F));    // 10xxxxxx
        result += static_cast<char>(0x80 | (c32 & 0x3F));           // 10xxxxxx
    }
    else if (c32 <= 0x10FFFF) {
        result += static_cast<char>(0xF0 | (c32 >> 18));            // 11110xxx
        result += static_cast<char>(0x80 | ((c32 >> 12) & 0x3F));   // 10xxxxxx
        result += static_cast<char>(0x80 | ((c32 >> 6) & 0x3F));    // 10xxxxxx
        result += static_cast<char>(0x80 | (c32 & 0x3F));           // 10xxxxxx
    }
    else
        throw std::runtime_error("utf32_to_utf8: invalid UTF-32 value");
    return result;
}


uint32_t uppercase_utf32(uint32_t c32)
{
    // Table from https://www.ibm.com/docs/vi/i/7.5?topic=tables-unicode-lowercase-uppercase-conversion-mapping-table
    // Accessed 20 February 2025.
    static const std::vector<uint16_t> lower_upper{
        0x0061, 0x0041, // LATIN SMALL LETTER A                                                     LATIN CAPITAL LETTER A
        0x0062, 0x0042, // LATIN SMALL LETTER B                                                     LATIN CAPITAL LETTER B
        0x0063, 0x0043, // LATIN SMALL LETTER C                                                     LATIN CAPITAL LETTER C
        0x0064, 0x0044, // LATIN SMALL LETTER D                                                     LATIN CAPITAL LETTER D
        0x0065, 0x0045, // LATIN SMALL LETTER E                                                     LATIN CAPITAL LETTER E
        0x0066, 0x0046, // LATIN SMALL LETTER F                                                     LATIN CAPITAL LETTER F
        0x0067, 0x0047, // LATIN SMALL LETTER G                                                     LATIN CAPITAL LETTER G
        0x0068, 0x0048, // LATIN SMALL LETTER H                                                     LATIN CAPITAL LETTER H
        0x0069, 0x0049, // LATIN SMALL LETTER I                                                     LATIN CAPITAL LETTER I
        0x006A, 0x004A, // LATIN SMALL LETTER J                                                     LATIN CAPITAL LETTER J
        0x006B, 0x004B, // LATIN SMALL LETTER K                                                     LATIN CAPITAL LETTER K
        0x006C, 0x004C, // LATIN SMALL LETTER L                                                     LATIN CAPITAL LETTER L
        0x006D, 0x004D, // LATIN SMALL LETTER M                                                     LATIN CAPITAL LETTER M
        0x006E, 0x004E, // LATIN SMALL LETTER N                                                     LATIN CAPITAL LETTER N
        0x006F, 0x004F, // LATIN SMALL LETTER O                                                     LATIN CAPITAL LETTER O
        0x0070, 0x0050, // LATIN SMALL LETTER P                                                     LATIN CAPITAL LETTER P
        0x0071, 0x0051, // LATIN SMALL LETTER Q                                                     LATIN CAPITAL LETTER Q
        0x0072, 0x0052, // LATIN SMALL LETTER R                                                     LATIN CAPITAL LETTER R
        0x0073, 0x0053, // LATIN SMALL LETTER S                                                     LATIN CAPITAL LETTER S
        0x0074, 0x0054, // LATIN SMALL LETTER T                                                     LATIN CAPITAL LETTER T
        0x0075, 0x0055, // LATIN SMALL LETTER U                                                     LATIN CAPITAL LETTER U
        0x0076, 0x0056, // LATIN SMALL LETTER V                                                     LATIN CAPITAL LETTER V
        0x0077, 0x0057, // LATIN SMALL LETTER W                                                     LATIN CAPITAL LETTER W
        0x0078, 0x0058, // LATIN SMALL LETTER X                                                     LATIN CAPITAL LETTER X
        0x0079, 0x0059, // LATIN SMALL LETTER Y                                                     LATIN CAPITAL LETTER Y
        0x007A, 0x005A, // LATIN SMALL LETTER Z                                                     LATIN CAPITAL LETTER Z
        0x00E0, 0x00C0, // LATIN SMALL LETTER A GRAVE                                               LATIN CAPITAL LETTER A GRAVE
        0x00E1, 0x00C1, // LATIN SMALL LETTER A GRAVE                                               LATIN CAPITAL LETTER A ACUTE
        0x00E2, 0x00C2, // LATIN SMALL LETTER A GRAVE                                               LATIN CAPITAL LETTER A CIRCUMFLEX
        0x00E3, 0x00C3, // LATIN SMALL LETTER A GRAVE                                               LATIN CAPITAL LETTER A TILDE
        0x00E4, 0x00C4, // LATIN SMALL LETTER A GRAVE                                               LATIN CAPITAL LETTER A DIAERESIS
        0x00E5, 0x00C5, // LATIN SMALL LETTER A GRAVE                                               LATIN CAPITAL LETTER A RING
        0x00E6, 0x00C6, // LATIN SMALL LETTER A GRAVE                                               LATIN CAPITAL LETTER A E
        0x00E7, 0x00C7, // LATIN SMALL LETTER A GRAVE                                               LATIN CAPITAL LETTER C CEDILLA
        0x00E8, 0x00C8, // LATIN SMALL LETTER A GRAVE                                               LATIN CAPITAL LETTER E GRAVE
        0x00E9, 0x00C9, // LATIN SMALL LETTER A GRAVE                                               LATIN CAPITAL LETTER E ACUTE
        0x00EA, 0x00CA, // LATIN SMALL LETTER E CIRCUMFLEX                                          LATIN CAPITAL LETTER E CIRCUMFLEX
        0x00EB, 0x00CB, // LATIN SMALL LETTER E DIAERESIS                                           LATIN CAPITAL LETTER E DIAERESIS
        0x00EC, 0x00CC, // LATIN SMALL LETTER I GRAVE                                               LATIN CAPITAL LETTER I GRAVE
        0x00ED, 0x00CD, // LATIN SMALL LETTER I ACUTE                                               LATIN CAPITAL LETTER I ACUTE
        0x00EE, 0x00CE, // LATIN SMALL LETTER I CIRCUMFLEX                                          LATIN CAPITAL LETTER I CIRCUMFLEX
        0x00EF, 0x00CF, // LATIN SMALL LETTER I DIAERESIS                                           LATIN CAPITAL LETTER I DIAERESIS
        0x00F0, 0x00D0, // LATIN SMALL LETTER ETH                                                   LATIN CAPITAL LETTER ETH
        0x00F1, 0x00D1, // LATIN SMALL LETTER N TILDE                                               LATIN CAPITAL LETTER N TILDE
        0x00F2, 0x00D2, // LATIN SMALL LETTER O GRAVE                                               LATIN CAPITAL LETTER O GRAVE
        0x00F3, 0x00D3, // LATIN SMALL LETTER O ACUTE                                               LATIN CAPITAL LETTER O ACUTE
        0x00F4, 0x00D4, // LATIN SMALL LETTER O CIRCUMFLEX                                          LATIN CAPITAL LETTER O CIRCUMFLEX
        0x00F5, 0x00D5, // LATIN SMALL LETTER O TILDE                                               LATIN CAPITAL LETTER O TILDE
        0x00F6, 0x00D6, // LATIN SMALL LETTER O DIAERESIS                                           LATIN CAPITAL LETTER O DIAERESIS
        0x00F8, 0x00D8, // LATIN SMALL LETTER O SLASH                                               LATIN CAPITAL LETTER O SLASH
        0x00F9, 0x00D9, // LATIN SMALL LETTER U GRAVE                                               LATIN CAPITAL LETTER U GRAVE
        0x00FA, 0x00DA, // LATIN SMALL LETTER U ACUTE                                               LATIN CAPITAL LETTER U ACUTE
        0x00FB, 0x00DB, // LATIN SMALL LETTER U CIRCUMFLEX                                          LATIN CAPITAL LETTER U CIRCUMFLEX
        0x00FC, 0x00DC, // LATIN SMALL LETTER U DIAERESIS                                           LATIN CAPITAL LETTER U DIAERESIS
        0x00FD, 0x00DD, // LATIN SMALL LETTER Y ACUTE                                               LATIN CAPITAL LETTER Y ACUTE
        0x00FE, 0x00DE, // LATIN SMALL LETTER THORN                                                 LATIN CAPITAL LETTER THORN
        0x00FF, 0x0178, // LATIN SMALL LETTER Y DIAERESIS                                           LATIN CAPITAL LETTER Y WITH DIAERESIS
        0x0101, 0x0100, // LATIN SMALL LETTER A WITH MACRON                                         LATIN CAPITAL LETTER A WITH MACRON
        0x0103, 0x0102, // LATIN SMALL LETTER A WITH BREVE                                          LATIN CAPITAL LETTER A WITH BREVE
        0x0105, 0x0104, // LATIN SMALL LETTER A WITH OGONEK                                         LATIN CAPITAL LETTER A WITH OGONEK
        0x0107, 0x0106, // LATIN SMALL LETTER C WITH ACUTE                                          LATIN CAPITAL LETTER C WITH ACUTE
        0x0109, 0x0108, // LATIN SMALL LETTER C WITH CIRCUMFLEX                                     LATIN CAPITAL LETTER C WITH CIRCUMFLEX
        0x010B, 0x010A, // LATIN SMALL LETTER C WITH DOT ABOVE                                      LATIN CAPITAL LETTER C WITH DOT ABOVE
        0x010D, 0x010C, // LATIN SMALL LETTER C WITH CARON                                          LATIN CAPITAL LETTER C WITH CARON
        0x010F, 0x010E, // LATIN SMALL LETTER D WITH CARON                                          LATIN CAPITAL LETTER D WITH CARON
        0x0111, 0x0110, // LATIN SMALL LETTER D WITH STROKE                                         LATIN CAPITAL LETTER D WITH STROKE
        0x0113, 0x0112, // LATIN SMALL LETTER E WITH MACRON                                         LATIN CAPITAL LETTER E WITH MACRON
        0x0115, 0x0114, // LATIN SMALL LETTER E WITH BREVE                                          LATIN CAPITAL LETTER E WITH BREVE
        0x0117, 0x0116, // LATIN SMALL LETTER E WITH DOT ABOVE                                      LATIN CAPITAL LETTER E WITH DOT ABOVE
        0x0119, 0x0118, // LATIN SMALL LETTER E WITH OGONEK                                         LATIN CAPITAL LETTER E WITH OGONEK
        0x011B, 0x011A, // LATIN SMALL LETTER E WITH CARON                                          LATIN CAPITAL LETTER E WITH CARON
        0x011D, 0x011C, // LATIN SMALL LETTER G WITH CIRCUMFLEX                                     LATIN CAPITAL LETTER G WITH CIRCUMFLEX
        0x011F, 0x011E, // LATIN SMALL LETTER G WITH BREVE                                          LATIN CAPITAL LETTER G WITH BREVE
        0x0121, 0x0120, // LATIN SMALL LETTER G WITH DOT ABOVE                                      LATIN CAPITAL LETTER G WITH DOT ABOVE
        0x0123, 0x0122, // LATIN SMALL LETTER G WITH CEDILLA                                        LATIN CAPITAL LETTER G WITH CEDILLA
        0x0125, 0x0124, // LATIN SMALL LETTER H WITH CIRCUMFLEX                                     LATIN CAPITAL LETTER H WITH CIRCUMFLEX
        0x0127, 0x0126, // LATIN SMALL LETTER H WITH STROKE                                         LATIN CAPITAL LETTER H WITH STROKE
        0x0129, 0x0128, // LATIN SMALL LETTER I WITH TILDE                                          LATIN CAPITAL LETTER I WITH TILDE
        0x012B, 0x012A, // LATIN SMALL LETTER I WITH MACRON                                         LATIN CAPITAL LETTER I WITH MACRON
        0x012D, 0x012C, // LATIN SMALL LETTER I WITH BREVE                                          LATIN CAPITAL LETTER I WITH BREVE
        0x012F, 0x012E, // LATIN SMALL LETTER I WITH OGONEK                                         LATIN CAPITAL LETTER I WITH OGONEK
        0x0131, 0x0049, // LATIN SMALL LETTER DOTLESS I                                             LATIN CAPITAL LETTER I
        0x0133, 0x0132, // LATIN SMALL LIGATURE IJ                                                  LATIN CAPITAL LIGATURE IJ
        0x0135, 0x0134, // LATIN SMALL LETTER J WITH CIRCUMFLEX                                     LATIN CAPITAL LETTER J WITH CIRCUMFLEX
        0x0137, 0x0136, // LATIN SMALL LETTER K WITH CEDILLA                                        LATIN CAPITAL LETTER K WITH CEDILLA
        0x013A, 0x0139, // LATIN SMALL LETTER L WITH ACUTE                                          LATIN CAPITAL LETTER L WITH ACUTE
        0x013C, 0x013B, // LATIN SMALL LETTER L WITH CEDILLA                                        LATIN CAPITAL LETTER L WITH CEDILLA
        0x013E, 0x013D, // LATIN SMALL LETTER L WITH CARON                                          LATIN CAPITAL LETTER L WITH CARON
        0x0140, 0x013F, // LATIN SMALL LETTER L WITH MIDDLE DOT                                     LATIN CAPITAL LETTER L WITH MIDDLE DOT
        0x0142, 0x0141, // LATIN SMALL LETTER L WITH STROKE                                         LATIN CAPITAL LETTER L WITH STROKE
        0x0144, 0x0143, // LATIN SMALL LETTER N WITH ACUTE                                          LATIN CAPITAL LETTER N WITH ACUTE
        0x0146, 0x0145, // LATIN SMALL LETTER N WITH CEDILLA                                        LATIN CAPITAL LETTER N WITH CEDILLA
        0x0148, 0x0147, // LATIN SMALL LETTER N WITH CARON                                          LATIN CAPITAL LETTER N WITH CARON
        0x014B, 0x014A, // LATIN SMALL LETTER ENG (SAMI)                                            LATIN CAPITAL LETTER ENG (SAMI)
        0x014D, 0x014C, // LATIN SMALL LETTER O WITH MACRON                                         LATIN CAPITAL LETTER O WITH MACRON
        0x014F, 0x014E, // LATIN SMALL LETTER O WITH BREVE                                          LATIN CAPITAL LETTER O WITH BREVE
        0x0151, 0x0150, // LATIN SMALL LETTER O WITH DOUBLE ACUTE                                   LATIN CAPITAL LETTER O WITH DOUBLE ACUTE
        0x0153, 0x0152, // LATIN SMALL LIGATURE OE                                                  LATIN CAPITAL LIGATURE OE
        0x0155, 0x0154, // LATIN SMALL LETTER R WITH ACUTE                                          LATIN CAPITAL LETTER R WITH ACUTE
        0x0157, 0x0156, // LATIN SMALL LETTER R WITH CEDILLA                                        LATIN CAPITAL LETTER R WITH CEDILLA
        0x0159, 0x0158, // LATIN SMALL LETTER R WITH CARON                                          LATIN CAPITAL LETTER R WITH CARON
        0x015B, 0x015A, // LATIN SMALL LETTER S WITH ACUTE                                          LATIN CAPITAL LETTER S WITH ACUTE
        0x015D, 0x015C, // LATIN SMALL LETTER S WITH CIRCUMFLEX                                     LATIN CAPITAL LETTER S WITH CIRCUMFLEX
        0x015F, 0x015E, // LATIN SMALL LETTER S WITH CEDILLA                                        LATIN CAPITAL LETTER S WITH CEDILLA
        0x0161, 0x0160, // LATIN SMALL LETTER S WITH CARON                                          LATIN CAPITAL LETTER S WITH CARON
        0x0163, 0x0162, // LATIN SMALL LETTER T WITH CEDILLA                                        LATIN CAPITAL LETTER T WITH CEDILLA
        0x0165, 0x0164, // LATIN SMALL LETTER T WITH CARON                                          LATIN CAPITAL LETTER T WITH CARON
        0x0167, 0x0166, // LATIN SMALL LETTER T WITH STROKE                                         LATIN CAPITAL LETTER T WITH STROKE
        0x0169, 0x0168, // LATIN SMALL LETTER U WITH TILDE                                          LATIN CAPITAL LETTER U WITH TILDE
        0x016B, 0x016A, // LATIN SMALL LETTER U WITH MACRON                                         LATIN CAPITAL LETTER U WITH MACRON
        0x016D, 0x016C, // LATIN SMALL LETTER U WITH BREVE                                          LATIN CAPITAL LETTER U WITH BREVE
        0x016F, 0x016E, // LATIN SMALL LETTER U WITH RING ABOVE                                     LATIN CAPITAL LETTER U WITH RING ABOVE
        0x0171, 0x0170, // LATIN SMALL LETTER U WITH DOUBLE ACUTE                                   LATIN CAPITAL LETTER U WITH DOUBLE ACUTE
        0x0173, 0x0172, // LATIN SMALL LETTER U WITH OGONEK                                         LATIN CAPITAL LETTER U WITH OGONEK
        0x0175, 0x0174, // LATIN SMALL LETTER W WITH CIRCUMFLEX                                     LATIN CAPITAL LETTER W WITH CIRCUMFLEX
        0x0177, 0x0176, // LATIN SMALL LETTER Y WITH CIRCUMFLEX                                     LATIN CAPITAL LETTER Y WITH CIRCUMFLEX
        0x017A, 0x0179, // LATIN SMALL LETTER Z WITH ACUTE                                          LATIN CAPITAL LETTER Z WITH ACUTE
        0x017C, 0x017B, // LATIN SMALL LETTER Z WITH DOT ABOVE                                      LATIN CAPITAL LETTER Z WITH DOT ABOVE
        0x017E, 0x017D, // LATIN SMALL LETTER Z WITH CARON                                          LATIN CAPITAL LETTER Z WITH CARON
        0x0183, 0x0182, // LATIN SMALL LETTER B WITH TOPBAR                                         LATIN CAPITAL LETTER B WITH TOPBAR
        0x0185, 0x0184, // LATIN SMALL LETTER TONE SIX                                              LATIN CAPITAL LETTER TONE SIX
        0x0188, 0x0187, // LATIN SMALL LETTER C WITH HOOK                                           LATIN CAPITAL LETTER C WITH HOOK
        0x018C, 0x018B, // LATIN SMALL LETTER D WITH TOPBAR                                         LATIN CAPITAL LETTER D WITH TOPBAR
        0x0192, 0x0191, // LATIN SMALL LETTER F WITH HOOK                                           LATIN CAPITAL LETTER F WITH HOOK
        0x0199, 0x0198, // LATIN SMALL LETTER K WITH HOOK                                           LATIN CAPITAL LETTER K WITH HOOK
        0x01A1, 0x01A0, // LATIN SMALL LETTER O WITH HORN                                           LATIN CAPITAL LETTER O WITH HORN
        0x01A3, 0x01A2, // LATIN SMALL LETTER OI                                                    LATIN CAPITAL LETTER OI
        0x01A5, 0x01A4, // LATIN SMALL LETTER P WITH HOOK                                           LATIN CAPITAL LETTER P WITH HOOK
        0x01A8, 0x01A7, // LATIN SMALL LETTER TONE TWO                                              LATIN CAPITAL LETTER TONE TWO
        0x01AD, 0x01AC, // LATIN SMALL LETTER T WITH HOOK                                           LATIN CAPITAL LETTER T WITH HOOK
        0x01B0, 0x01AF, // LATIN SMALL LETTER U WITH HORN                                           LATIN CAPITAL LETTER U WITH HORN
        0x01B4, 0x01B3, // LATIN SMALL LETTER Y WITH HOOK                                           LATIN CAPITAL LETTER Y WITH HOOK
        0x01B6, 0x01B5, // LATIN SMALL LETTER Z WITH STROKE                                         LATIN CAPITAL LETTER Z WITH STROKE
        0x01B9, 0x01B8, // LATIN SMALL LETTER EZH REVERSED                                          LATIN CAPITAL LETTER EZH REVERSED
        0x01BD, 0x01BC, // LATIN SMALL LETTER TONE FIVE                                             LATIN CAPITAL LETTER TONE FIVE
        0x01C6, 0x01C4, // LATIN SMALL LETTER DZ WITH CARON                                         LATIN CAPITAL LETTER DZ WITH CARON
        0x01C9, 0x01C7, // LATIN SMALL LETTER LJ                                                    LATIN CAPITAL LETTER LJ
        0x01CC, 0x01CA, // LATIN SMALL LETTER NJ                                                    LATIN CAPITAL LETTER NJ
        0x01CE, 0x01CD, // LATIN SMALL LETTER A WITH CARON                                          LATIN CAPITAL LETTER A WITH CARON
        0x01D0, 0x01CF, // LATIN SMALL LETTER I WITH CARON                                          LATIN CAPITAL LETTER I WITH CARON
        0x01D2, 0x01D1, // LATIN SMALL LETTER O WITH CARON                                          LATIN CAPITAL LETTER O WITH CARON
        0x01D4, 0x01D3, // LATIN SMALL LETTER U WITH CARON                                          LATIN CAPITAL LETTER U WITH CARON
        0x01D6, 0x01D5, // LATIN SMALL LETTER U WITH DIAERESIS AND MACRON                           LATIN CAPITAL LETTER U WITH DIAERESIS AND MACRON
        0x01D8, 0x01D7, // LATIN SMALL LETTER U WITH DIAERESIS AND ACUTE                            LATIN CAPITAL LETTER U WITH DIAERESIS AND ACUTE
        0x01DA, 0x01D9, // LATIN SMALL LETTER U WITH DIAERESIS AND CARON                            LATIN CAPITAL LETTER U WITH DIAERESIS AND CARON
        0x01DC, 0x01DB, // LATIN SMALL LETTER U WITH DIAERESIS AND GRAVE                            LATIN CAPITAL LETTER U WITH DIAERESIS AND GRAVE
        0x01DF, 0x01DE, // LATIN SMALL LETTER A WITH DIAERESIS AND MACRON                           LATIN CAPITAL LETTER A WITH DIAERESIS AND MACRON
        0x01E1, 0x01E0, // LATIN SMALL LETTER A WITH DOT ABOVE AND MACRON                           LATIN CAPITAL LETTER A WITH DOT ABOVE AND MACRON
        0x01E3, 0x01E2, // LATIN SMALL LIGATURE AE WITH MACRON                                      LATIN CAPITAL LIGATURE AE MTH MACRON
        0x01E5, 0x01E4, // LATIN SMALL LETTER G WITH STROKE                                         LATIN CAPITAL LETTER G WITH STROKE
        0x01E7, 0x01E6, // LATIN SMALL LETTER G WITH CARON                                          LATIN CAPITAL LETTER G WITH CARON
        0x01E9, 0x01E8, // LATIN SMALL LETTER K WITH CARON                                          LATIN CAPITAL LETTER K WITH CARON
        0x01EB, 0x01EA, // LATIN SMALL LETTER O WITH OGONEK                                         LATIN CAPITAL LETTER O WITH OGONEK
        0x01ED, 0x01EC, // LATIN SMALL LETTER O WITH OGONEK AND MACRON                              LATIN CAPITAL LETTER O WITH OGONEK AND MACRON
        0x01EF, 0x01EE, // LATIN SMALL LETTER EZH WITH CARON                                        LATIN CAPITAL LETTER EZH WITH CARON
        0x01F3, 0x01F1, // LATIN SMALL LETTER DZ                                                    LATIN CAPITAL LETTER DZ
        0x01F5, 0x01F4, // LATIN SMALL LETTER G WITH ACUTE                                          LATIN CAPITAL LETTER G WITH ACUTE
        0x01FB, 0x01FA, // LATIN SMALL LETTER A WITH RING ABOVE AND ACUTE                           LATIN CAPITAL LETTER A WITH RING ABOVE AND ACUTE
        0x01FD, 0x01FC, // LATIN SMALL LIGATURE AE WITH ACUTE                                       LATIN CAPITAL LIGATURE AE WITH ACUTE
        0x01FF, 0x01FE, // LATIN SMALL LETTER O WITH STROKE AND ACUTE                               LATIN CAPITAL LETTER O WITH STROKE AND ACUTE
        0x0201, 0x0200, // LATIN SMALL LETTER A WITH DOUBLE GRAVE                                   LATIN CAPITAL LETTER A WITH DOUBLE GRAVE
        0x0203, 0x0202, // LATIN SMALL LETTER A WITH INVERTED BREVE                                 LATIN CAPITAL LETTER A WITH INVERTED BREVE
        0x0205, 0x0204, // LATIN SMALL LETTER E WITH DOUBLE GRAVE                                   LATIN CAPITAL LETTER E WITH DOUBLE GRAVE
        0x0207, 0x0206, // LATIN SMALL LETTER E WITH INVERTED BREVE                                 LATIN CAPITAL LETTER E WITH INVERTED BREVE
        0x0209, 0x0208, // LATIN SMALL LETTER I WITH DOUBLE GRAVE                                   LATIN CAPITAL LETTER I WITH DOUBLE GRAVE
        0x020B, 0x020A, // LATIN SMALL LETTER I WITH INVERTED BREVE                                 LATIN CAPITAL LETTER I WITH INVERTED BREVE
        0x020D, 0x020C, // LATIN SMALL LETTER O WITH DOUBLE GRAVE                                   LATIN CAPITAL LETTER O WITH DOUBLE GRAVE
        0x020F, 0x020E, // LATIN SMALL LETTER O WITH INVERTED BREVE                                 LATIN CAPITAL LETTER O WITH INVERTED BREVE
        0x0211, 0x0210, // LATIN SMALL LETTER R WITH DOUBLE GRAVE                                   LATIN CAPITAL LETTER R WITH DOUBLE GRAVE
        0x0213, 0x0212, // LATIN SMALL LETTER R WITH INVERTED BREVE                                 LATIN CAPITAL LETTER R WITH INVERTED BREVE
        0x0215, 0x0214, // LATIN SMALL LETTER U WITH DOUBLE GRAVE                                   LATIN CAPITAL LETTER U WITH DOUBLE GRAVE
        0x0217, 0x0216, // LATIN SMALL LETTER U WITH INVERTED BREVE                                 LATIN CAPITAL LETTER U WITH INVERTED BREVE
        0x0253, 0x0181, // LATIN SMALL LETTER B WITH HOOK                                           LATIN CAPITAL LETTER B WITH HOOK
        0x0254, 0x0186, // LATIN SMALL LETTER OPEN O                                                LATIN CAPITAL LETTER OPEN O
        0x0257, 0x018A, // LATIN SMALL LETTER D WITH HOOK                                           LATIN CAPITAL LETTER D WITH HOOK
        0x0258, 0x018E, // LATIN SMALL LETTER REVERSED E                                            LATIN CAPITAL LETTER REVERSED E
        0x0259, 0x018F, // LATIN SMALL LETTER SCHWA                                                 LATIN CAPITAL LETTER SCHWA
        0x025B, 0x0190, // LATIN SMALL LETTER OPEN E                                                LATIN CAPITAL LETTER OPEN E
        0x0260, 0x0193, // LATIN SMALL LETTER G WITH HOOK                                           LATIN CAPITAL LETTER G WITH HOOK
        0x0263, 0x0194, // LATIN SMALL LETTER GAMMA                                                 LATIN CAPITAL LETTER GAMMA
        0x0268, 0x0197, // LATIN SMALL LETTER I WITH STROKE                                         LATIN CAPITAL LETTER I WITH STROKE
        0x0269, 0x0196, // LATIN SMALL LETTER IOTA                                                  LATIN CAPITAL LETTER IOTA
        0x026F, 0x019C, // LATIN SMALL LETTER TURNED M                                              LATIN CAPITAL LETTER TURNED M
        0x0272, 0x019D, // LATIN SMALL LETTER N WITH LEFT HOOK                                      LATIN CAPITAL LETTER N WITH LEFT HOOK
        0x0275, 0x019F, // LATIN SMALL LETTER BARRED O                                              LATIN CAPITAL LETTER O WITH MIDDLE TILDE
        0x0283, 0x01A9, // LATIN SMALL LETTER ESH                                                   LATIN CAPITAL LETTER ESH
        0x0288, 0x01AE, // LATIN SMALL LETTER T WITH RETROFLEX HOOK                                 LATIN CAPITAL LETTER T WITH RETROFLEX HOOK
        0x028A, 0x01B1, // LATIN SMALL LETTER UPSILON                                               LATIN CAPITAL LETTER UPSILON
        0x028B, 0x01B2, // LATIN SMALL LETTER V WITH HOOK                                           LATIN CAPITAL LETTER V WITH HOOK
        0x0292, 0x01B7, // LATIN SMALL LETTER EZH                                                   LATIN CAPITAL LETTER EZH
        0x03AC, 0x0386, // GREEK SMALL LETTER ALPHA WITH TONOS                                      GREEK CAPITAL LETTER ALPHA WITH TONOS
        0x03AD, 0x0388, // GREEK SMALL LETTER EPSILON WITH TONOS                                    GREEK CAPITAL LETTER EPSILON WITH TONOS
        0x03AE, 0x0389, // GREEK SMALL LETTER ETA WITH TONOS                                        GREEK CAPITAL LETTER ETA WITH TONOS
        0x03AF, 0x038A, // GREEK SMALL LETTER IOTA WITH TONOS                                       GREEK CAPITAL LETTER IOTA WITH TONOS
        0x03B1, 0x0391, // GREEK SMALL LETTER ALPHA                                                 GREEK CAPITAL LETTER ALPHA
        0x03B2, 0x0392, // GREEK SMALL LETTER BETA                                                  GREEK CAPITAL LETTER BETA
        0x03B3, 0x0393, // GREEK SMALL LETTER GAMMA                                                 GREEK CAPITAL LETTER GAMMA
        0x03B4, 0x0394, // GREEK SMALL LETTER DELTA                                                 GREEK CAPITAL LETTER DELTA
        0x03B5, 0x0395, // GREEK SMALL LETTER EPSILON                                               GREEK CAPITAL LETTER EPSILON
        0x03B6, 0x0396, // GREEK SMALL LETTER ZETA                                                  GREEK CAPITAL LETTER ZETA
        0x03B7, 0x0397, // GREEK SMALL LETTER ETA                                                   GREEK CAPITAL LETTER ETA
        0x03B8, 0x0398, // GREEK SMALL LETTER THETA                                                 GREEK CAPITAL LETTER THETA
        0x03B9, 0x0399, // GREEK SMALL LETTER IOTA                                                  GREEK CAPITAL LETTER IOTA
        0x03BA, 0x039A, // GREEK SMALL LETTER KAPPA                                                 GREEK CAPITAL LETTER KAPPA
        0x03BB, 0x039B, // GREEK SMALL LETTER LAMDA                                                 GREEK CAPITAL LETTER LAMDA
        0x03BC, 0x039C, // GREEK SMALL LETTER MU                                                    GREEK CAPITAL LETTER MU
        0x03BD, 0x039D, // GREEK SMALL LETTER NU                                                    GREEK CAPITAL LETTER NU
        0x03BE, 0x039E, // GREEK SMALL LETTER XI                                                    GREEK CAPITAL LETTER XI
        0x03BF, 0x039F, // GREEK SMALL LETTER OMICRON                                               GREEK CAPITAL LETTER OMICRON
        0x03C0, 0x03A0, // GREEK SMALL LETTER PI                                                    GREEK CAPITAL LETTER PI
        0x03C1, 0x03A1, // GREEK SMALL LETTER RHO                                                   GREEK CAPITAL LETTER RHO
        0x03C3, 0x03A3, // GREEK SMALL LETTER SIGMA                                                 GREEK CAPITAL LETTER SIGMA
        0x03C4, 0x03A4, // GREEK SMALL LETTER TAU                                                   GREEK CAPITAL LETTER TAU
        0x03C5, 0x03A5, // GREEK SMALL LETTER UPSILON                                               GREEK CAPITAL LETTER UPSILON
        0x03C6, 0x03A6, // GREEK SMALL LETTER PHI                                                   GREEK CAPITAL LETTER PHI
        0x03C7, 0x03A7, // GREEK SMALL LETTER CHI                                                   GREEK CAPITAL LETTER CHI
        0x03C8, 0x03A8, // GREEK SMALL LETTER PSI                                                   GREEK CAPITAL LETTER PSI
        0x03C9, 0x03A9, // GREEK SMALL LETTER OMEGA                                                 GREEK CAPITAL LETTER OMEGA
        0x03CA, 0x03AA, // GREEK SMALL LETTER IOTA WITH DIALYTIKA                                   GREEK CAPITAL LETTER IOTA WITH DIALYTIKA
        0x03CB, 0x03AB, // GREEK SMALL LETTER UPSILON WITH DIALYTIKA                                GREEK CAPITAL LETTER UPSILON WITH DIALYTIKA
        0x03CC, 0x038C, // GREEK SMALL LETTER OMICRON WITH TONOS                                    GREEK CAPITAL LETTER OMICRON WITH TONOS
        0x03CD, 0x038E, // GREEK SMALL LETTER UPSILON WITH TONOS                                    GREEK CAPITAL LETTER UPSILON WITH TONOS
        0x03CE, 0x038F, // GREEK SMALL LETTER OMEGA WITH TONOS                                      GREEK CAPITAL LETTER OMEGA WITH TONOS
        0x03E3, 0x03E2, // COPTIC SMALL LETTER SHEI                                                 COPTIC CAPITAL LETTER SHEI
        0x03E5, 0x03E4, // COPTIC SMALL LETTER FEI                                                  COPTIC CAPITAL LETTER FEI
        0x03E7, 0x03E6, // COPTIC SMALL LETTER KHEI                                                 COPTIC CAPITAL LETTER KHEI
        0x03E9, 0x03E8, // COPTIC SMALL LETTER HORI                                                 COPTIC CAPITAL LETTER HORI
        0x03EB, 0x03EA, // COPTIC SMALL LETTER GANGIA                                               COPTIC CAPITAL LETTER GANGIA
        0x03ED, 0x03EC, // COPTIC SMALL LETTER SHIMA                                                COPTIC CAPITAL LETTER SHIMA
        0x03EF, 0x03EE, // COPTIC SMALL LETTER DEI                                                  COPTIC CAPITAL LETTER DEI
        0x0430, 0x0410, // CYRILLIC SMALL LETTER A                                                  CYRILLIC CAPITAL LETTER A
        0x0431, 0x0411, // CYRILLIC SMALL LETTER BE                                                 CYRILLIC CAPITAL LETTER BE
        0x0432, 0x0412, // CYRILLIC SMALL LETTER VE                                                 CYRILLIC CAPITAL LETTER VE
        0x0433, 0x0413, // CYRILLIC SMALL LETTER GHE                                                CYRILLIC CAPITAL LETTER GHE
        0x0434, 0x0414, // CYRILLIC SMALL LETTER DE                                                 CYRILLIC CAPITAL LETTER DE
        0x0435, 0x0415, // CYRILLIC SMALL LETTER IE                                                 CYRILLIC CAPITAL LETTER IE
        0x0436, 0x0416, // CYRILLIC SMALL LETTER ZHE                                                CYRILLIC CAPITAL LETTER ZHE
        0x0437, 0x0417, // CYRILLIC SMALL LETTER ZE                                                 CYRILLIC CAPITAL LETTER ZE
        0x0438, 0x0418, // CYRILLIC SMALL LETTER I                                                  CYRILLIC CAPITAL LETTER I
        0x0439, 0x0419, // CYRILLIC SMALL LETTER SHORT I                                            CYRILLIC CAPITAL LETTER SHORT I
        0x043A, 0x041A, // CYRILLIC SMALL LETTER KA                                                 CYRILLIC CAPITAL LETTER KA
        0x043B, 0x041B, // CYRILLIC SMALL LETTER EL                                                 CYRILLIC CAPITAL LETTER EL
        0x043C, 0x041C, // CYRILLIC SMALL LETTER EM                                                 CYRILLIC CAPITAL LETTER EM
        0x043D, 0x041D, // CYRILLIC SMALL LETTER EN                                                 CYRILLIC CAPITAL LETTER EN
        0x043E, 0x041E, // CYRILLIC SMALL LETTER O                                                  CYRILLIC CAPITAL LETTER O
        0x043F, 0x041F, // CYRILLIC SMALL LETTER PE                                                 CYRILLIC CAPITAL LETTER PE
        0x0440, 0x0420, // CYRILLIC SMALL LETTER ER                                                 CYRILLIC CAPITAL LETTER ER
        0x0441, 0x0421, // CYRILLIC SMALL LETTER ES                                                 CYRILLIC CAPITAL LETTER ES
        0x0442, 0x0422, // CYRILLIC SMALL LETTER TE                                                 CYRILLIC CAPITAL LETTER TE
        0x0443, 0x0423, // CYRILLIC SMALL LETTER U                                                  CYRILLIC CAPITAL LETTER U
        0x0444, 0x0424, // CYRILLIC SMALL LETTER EF                                                 CYRILLIC CAPITAL LETTER EF
        0x0445, 0x0425, // CYRILLIC SMALL LETTER HA                                                 CYRILLIC CAPITAL LETTER HA
        0x0446, 0x0426, // CYRILLIC SMALL LETTER TSE                                                CYRILLIC CAPITAL LETTER TSE
        0x0447, 0x0427, // CYRILLIC SMALL LETTER CHE                                                CYRILLIC CAPITAL LETTER CHE
        0x0448, 0x0428, // CYRILLIC SMALL LETTER SHA                                                CYRILLIC CAPITAL LETTER SHA
        0x0449, 0x0429, // CYRILLIC SMALL LETTER SHCHA                                              CYRILLIC CAPITAL LETTER SHCHA
        0x044A, 0x042A, // CYRILLIC SMALL LETTER HARD SIGN                                          CYRILLIC CAPITAL LETTER HARD SIGN
        0x044B, 0x042B, // CYRILLIC SMALL LETTER YERU                                               CYRILLIC CAPITAL LETTER YERU
        0x044C, 0x042C, // CYRILLIC SMALL LETTER SOFT SIGN                                          CYRILLIC CAPITAL LETTER SOFT SIGN
        0x044D, 0x042D, // CYRILLIC SMALL LETTER E                                                  CYRILLIC CAPITAL LETTER E
        0x044E, 0x042E, // CYRILLIC SMALL LETTER YU                                                 CYRILLIC CAPITAL LETTER YU
        0x044F, 0x042F, // CYRILLIC SMALL LETTER YA                                                 CYRILLIC CAPITAL LETTER YA
        0x0451, 0x0401, // CYRILLIC SMALL LETTER IO                                                 CYRILLIC CAPITAL LETTER IO
        0x0452, 0x0402, // CYRILLIC SMALL LETTER DJE (SERBOCROATIAN)                                CYRILLIC CAPITAL LETTER DJE (SERBOCROATIAN)
        0x0453, 0x0403, // CYRILLIC SMALL LETTER GJE                                                CYRILLIC CAPITAL LETTER GJE
        0x0454, 0x0404, // CYRILLIC SMALL LETTER UKRAINIAN IE                                       CYRILLIC CAPITAL LETTER UKRAINIAN IE
        0x0455, 0x0405, // CYRILLIC SMALL LETTER DZE                                                CYRILLIC CAPITAL LETTER DZE
        0x0456, 0x0406, // CYRILLIC SMALL LETTER BYELORUSSIAN-UKRAINIAN I                           CYRILLIC CAPITAL LETTER BYELORUSSIAN_UKRAINIAN I
        0x0457, 0x0407, // CYRILLIC SMALL LETTER YI (UKRAINIAN)                                     CYRILLIC CAPITAL LETTER YI (UKRAINIAN)
        0x0458, 0x0408, // CYRILLIC SMALL LETTER JE                                                 CYRILLIC CAPITAL LETTER JE
        0x0459, 0x0409, // CYRILLIC SMALL LETTER LJE                                                CYRILLIC CAPITAL LETTER LJE
        0x045A, 0x040A, // CYRILLIC SMALL LETTER NJE                                                CYRILLIC CAPITAL LETTER NJE
        0x045B, 0x040B, // CYRILLIC SMALL LETTER TSHE (SERBOCROATIAN)                               CYRILLIC CAPITAL LETTER TSHE (SERBOCROATIAN)
        0x045C, 0x040C, // CYRILLIC SMALL LETTER KJE                                                CYRILLIC CAPITAL LETTER KJE
        0x045E, 0x040E, // CYRILLIC SMALL LETTER SHORT U (BYELORUSSIAN)                             CYRILLIC CAPITAL LETTER SHORT U (BYELORUSSIAN)
        0x045F, 0x040F, // CYRILLIC SMALL LETTER DZHE                                               CYRILLIC CAPITAL LETTER DZHE
        0x0461, 0x0460, // CYRILLIC SMALL LETTER OMEGA                                              CYRILLIC CAPITAL LETTER OMEGA
        0x0463, 0x0462, // CYRILLIC SMALL LETTER YAT                                                CYRILLIC CAPITAL LETTER YAT
        0x0465, 0x0464, // CYRILLIC SMALL LETTER IOTIFIED E                                         CYRILLIC CAPITAL LETTER IOTIFIED E
        0x0467, 0x0466, // CYRILLIC SMALL LETTER LITTLE YUS                                         CYRILLIC CAPITAL LETTER LITTLE YUS
        0x0469, 0x0468, // CYRILLIC SMALL LETTER IOTIFIED LITTLE YUS                                CYRILLIC CAPITAL LETTER IOTIFIED LITTLE YUS
        0x046B, 0x046A, // CYRILLIC SMALL LETTER BIG YUS                                            CYRILLIC CAPITAL LETTER BIG YUS
        0x046D, 0x046C, // CYRILLIC SMALL LETTER IOTIFIED BIG YUS                                   CYRILLIC CAPITAL LETTER IOTIFIED BIG YUS
        0x046F, 0x046E, // CYRILLIC SMALL LETTER KSI                                                CYRILLIC CAPITAL LETTER KSI
        0x0471, 0x0470, // CYRILLIC SMALL LETTER PSI                                                CYRILLIC CAPITAL LETTER PSI
        0x0473, 0x0472, // CYRILLIC SMALL LETTER FITA                                               CYRILLIC CAPITAL LETTER FITA
        0x0475, 0x0474, // CYRILLIC SMALL LETTER IZHITSA                                            CYRILLIC CAPITAL LETTER IZHITSA
        0x0477, 0x0476, // CYRILLIC SMALL LETTER IZHITSA WITH DOUBLE GRAVE ACCENT                   CYRILLIC CAPITAL LETTER IZHITSA WITH DOUBLE GRAVE ACCENT
        0x0479, 0x0478, // CYRILLIC SMALL LETTER UK                                                 CYRILLIC CAPITAL LETTER UK
        0x047B, 0x047A, // CYRILLIC SMALL LETTER ROUND OMEGA                                        CYRILLIC CAPITAL LETTER ROUND OMEGA
        0x047D, 0x047C, // CYRILLIC SMALL LETTER OMEGA WITH TITLO                                   CYRILLIC CAPITAL LETTER OMEGA WITH TITLO
        0x047F, 0x047E, // CYRILLIC SMALL LETTER OT                                                 CYRILLIC CAPITAL LETTER OT
        0x0481, 0x0480, // CYRILLIC SMALL LETTER KOPPA                                              CYRILLIC CAPITAL LETTER KOPPA
        0x0491, 0x0490, // CYRILLIC SMALL LETTER GHE WITH UPTURN                                    CYRILLIC CAPITAL LETTER GHE WITH UPTURN
        0x0493, 0x0492, // CYRILLIC SMALL LETTER GHE WITH STROKE                                    CYRILLIC CAPITAL LETTER GHE WITH STROKE
        0x0495, 0x0494, // CYRILLIC SMALL LETTER GHE WITH MIDDLE HOOK                               CYRILLIC CAPITAL LETTER GHE WITH MIDDLE HOOK
        0x0497, 0x0496, // CYRILLIC SMALL LETTER ZHE WITH DESCENDER                                 CYRILLIC CAPITAL LETTER ZHE WITH DESCENDER
        0x0499, 0x0498, // CYRILLIC SMALL LETTER ZE WITH DESCENDER                                  CYRILLIC CAPITAL LETTER ZE WITH DESCENDER
        0x049B, 0x049A, // CYRILLIC SMALL LETTER KA WITH DESCENDER                                  CYRILLIC CAPITAL LETTER KA WITH DESCENDER
        0x049D, 0x049C, // CYRILLIC SMALL LETTER KA WITH VERTICAL STROKE                            CYRILLIC CAPITAL LETTER KA WITH VERTICAL STROKE
        0x049F, 0x049E, // CYRILLIC SMALL LETTER KA WITH STROKE                                     CYRILLIC CAPITAL LETTER KA WITH STROKE
        0x04A1, 0x04A0, // CYRILLIC SMALL LETTER EASHKIR KA                                         CYRILLIC CAPITAL LETTER BASHKIR KA
        0x04A3, 0x04A2, // CYRILLIC SMALL LETTER EN WITH DESCENOER                                  CYRILLIC CAPITAL LETTER EN WITH DESCENDER
        0x04A5, 0x04A4, // CYRILLIC SMALL LIGATURE EN GHE                                           CYRILLIC CAPITAL LIGATURE EN GHF
        0x04A7, 0x04A6, // CYRILLIC SMALL LETTER PE WITH MIDDLE HOOK (ABKHASIAN)                    CYRILLIC CAPITAL LETTER PE WITH MIDDLE HOOK (ABKHASIAN)
        0x04A9, 0x04A8, // CYRILLIC SMALL LETTER ABKHASIAN HA                                       CYRILLIC CAPITAL LETTER ABKHASIAN HA
        0x04AB, 0x04AA, // CYRILLIC SMALL LETTER ES WITH DESCENDER                                  CYRILLIC CAPITAL LETTER ES WITH DESCENDER
        0x04AD, 0x04AC, // CYRILLIC SMALL LETTER TE WITH DESCENDER                                  CYRILLIC CAPITAL LETTER TE WITH DESCENDER
        0x04AF, 0x04AE, // CYRILLIC SMALL LETTER STRAIGHT U                                         CYRILLIC CAPITAL LETTER STRAIGHT U
        0x04B1, 0x04B0, // CYRILLIC SMALL LETTER STRAIGHT U WITH STROKE                             CYRILLIC CAPITAL LETTER STRAIGHT U WITH STROKE
        0x04B3, 0x04B2, // CYRILLIC SMALL LETTER HA WITH DESCENDER                                  CYRILLIC CAPITAL LETTER HA WITH DESCENDER
        0x04B5, 0x04B4, // CYRILLIC SMALL LIGATURE TE TSE (ABKHASIAN)                               CYRILLIC CAPITAL LIGATURE TE TSE (ABKHASIAN)
        0x04B7, 0x04B6, // CYRILLIC SMALL LETTER CHE WITH DESCENDER                                 CYRILLIC CAPITAL LETTER CHE WITH DESCENDER
        0x04B9, 0x04B8, // CYRILLIC SMALL LETTER CHE WITH VERTICAL STROKE                           CYRILLIC CAPITAL LETTER CHE WITH VERTICAL STROKE
        0x04BB, 0x04BA, // CYRILLIC SMALL LETTER SHHA                                               CYRILLIC CAPITAL LETTER SHHA
        0x04BD, 0x04BC, // CYRILLIC SMALL LETTER ABKHASIAN CHE                                      CYRILLIC CAPITAL LETTER ABKHASIAN CHE
        0x04BF, 0x04BE, // CYRILLIC SMALL LETTER ABKHASIAN CHE WITH DESCENDER                       CYRILLIC CAPITAL LETTER ABKHASIAN CHE WITH DESCENDER
        0x04C2, 0x04C1, // CYRILLIC SMALL LETTER ZHE WITH BREVE                                     CYRILLIC CAPITAL LETTER ZHE WITH BREVE
        0x04C4, 0x04C3, // CYRILLIC SMALL LETTER KA WITH HOOK                                       CYRILLIC CAPITAL LETTER KA WITH HOOK
        0x04C8, 0x04C7, // CYRILLIC SMALL LETTER EN WITH HOOK                                       CYRILLIC CAPITAL LETTER EN WITH HOOK
        0x04CC, 0x04CB, // CYRILLIC SMALL LETTER KHAKASSIAN CHE                                     CYRILLIC CAPITAL LETTER KHAKASSIAN CHE
        0x04D1, 0x04D0, // CYRILLIC SMALL LETTER A WITH BREVE                                       CYRILLIC CAPITAL LETTER A WITH BREVE
        0x04D3, 0x04D2, // CYRILLIC SMALL LETTER A WITH DIAERESIS                                   CYRILLIC CAPITAL LETTER A WITH DIAERESIS
        0x04D5, 0x04D4, // CYRILLIC SMALL LIGATURE A IE                                             CYRILLIC CAPITAL LIGATURE A IE
        0x04D7, 0x04D6, // CYRILLIC SMALL LETTER IE WITH BREVE                                      CYRILLIC CAPITAL LETTER IE WITH BREVE
        0x04D9, 0x04D8, // CYRILLIC SMALL LETTER SCHWA                                              CYRILLIC CAPITAL LETTER SCHWA
        0x04DB, 0x04DA, // CYRILLIC SMALL LETTER SCHWA WITH DIAERESIS                               CYRILLIC CAPITAL LETTER SCHWA WITH DIAERESIS
        0x04DD, 0x04DC, // CYRILLIC SMALL LETTER ZHE WITH DIAERESIS                                 CYRILLIC CAPITAL LETTER ZHE WITH DIAERESIS
        0x04DF, 0x04DE, // CYRILLIC SMALL LETTER ZE WITH DIAERESIS                                  CYRILLIC CAPITAL LETTER ZE WITH DIAERESIS
        0x04E1, 0x04E0, // CYRILLIC SMALL LETTER ABKHASIAN DZE                                      CYRILLIC CAPITAL LETTER ABKHASIAN DZE
        0x04E3, 0x04E2, // CYRILLIC SMALL LETTER I WITH MACRON                                      CYRILLIC CAPITAL LETTER I WITH MACRON
        0x04E5, 0x04E4, // CYRILLIC SMALL LETTER I WITH DIAERESIS                                   CYRILLIC CAPITAL LETTER I WITH DIAERESIS
        0x04E7, 0x04E6, // CYRILLIC SMALL LETTER O WITH DIAERESIS                                   CYRILLIC CAPITAL LETTER O WITH DIAERESIS
        0x04E9, 0x04E8, // CYRILLIC SMALL LETTER BARRED O                                           CYRILLIC CAPITAL LETTER BARRED O
        0x04EB, 0x04EA, // CYRILLIC SMALL LETTER BARRED O WITH DIAERESIS                            CYRILLIC CAPITAL LETTER BARRED O WITH DIAERESIS
        0x04EF, 0x04EE, // CYRILLIC SMALL LETTER U WITH MACRON                                      CYRILLIC CAPITAL LETTER U WITH MACRON
        0x04F1, 0x04F0, // CYRILLIC SMALL LETTER U WITH DIAERESIS                                   CYRILLIC CAPITAL LETTER U WITH DIAERESIS
        0x04F3, 0x04F2, // CYRILLIC SMALL LETTER U WITH DOUBLE ACUTE                                CYRILLIC CAPITAL LETTER U WITH DOUBLE ACUTE
        0x04F5, 0x04F4, // CYRILLIC SMALL LETTER CHE AITH DIAERESIS                                 CYRILLIC CAPITAL LETTER CHE WITH DIAERESIS
        0x04F9, 0x04F8, // CYRILLIC SMALL LETTER YERU WITH DIAERESIS                                CYRILLIC CAPITAL LETTER YERU WITH DIAERESIS
        0x0561, 0x0531, // ARMENIAN SMALL LETTER AYB                                                ARMENIAN CAPITAL LETTER AYB
        0x0562, 0x0532, // ARMENIAN SMALL LETTER BEN                                                ARMENIAN CAPITAL LETTER BEN
        0x0563, 0x0533, // ARMENIAN SMALL LETTER GIM                                                ARMENIAN CAPITAL LETTER GIM
        0x0564, 0x0534, // ARMENIAN SMALL LETTER DA                                                 ARMENIAN CAPITAL LETTER DA
        0x0565, 0x0535, // ARMENIAN SMALL LETTER ECH                                                ARMENIAN CAPITAL LETTER ECH
        0x0566, 0x0536, // ARMENIAN SMALL LETTER ZA                                                 ARMENIAN CAPITAL LETTER ZA
        0x0567, 0x0537, // ARMENIAN SMALL LETTER EH                                                 ARMENIAN CAPITAL LETTER EH
        0x0568, 0x0538, // ARMENIAN SMALL LETTER ET                                                 ARMENIAN CAPITAL LETTER ET
        0x0569, 0x0539, // ARMENIAN SMALL LETTER TO                                                 ARMENIAN CAPITAL LETTER TO
        0x056A, 0x053A, // ARMENIAN SMALL LETTER ZHE                                                ARMENIAN CAPITAL LETTER ZHE
        0x056B, 0x053B, // ARMENIAN SMALL LETTER INI                                                ARMENIAN CAPITAL LETTER INI
        0x056C, 0x053C, // ARMENIAN SMALL LETTER LIWN                                               ARMENIAN CAPITAL LETTER LIWN
        0x056D, 0x053D, // ARMENIAN SMALL LETTER XEH                                                ARMENIAN CAPITAL LETTER XEH
        0x056E, 0x053E, // ARMENIAN SMALL LETTER CA                                                 ARMENIAN CAPITAL LETTER CA
        0x056F, 0x053F, // ARMENIAN SMALL LETTER KEN                                                ARMENIAN CAPITAL LETTER KEN
        0x0570, 0x0540, // ARMENIAN SMALL LETTER HO                                                 ARMENIAN CAPITAL LETTER HO
        0x0571, 0x0541, // ARMENIAN SMALL LETTER JA                                                 ARMENIAN CAPITAL LETTER JA
        0x0572, 0x0542, // ARMENIAN SMALL LETTER GHAD                                               ARMENIAN CAPITAL LETTER GHAD
        0x0573, 0x0543, // ARMENIAN SMALL LETTER CHEH                                               ARMENIAN CAPITAL LETTER CHEH
        0x0574, 0x0544, // ARMENIAN SMALL LETTER MEN                                                ARMENIAN CAPITAL LETTER MEN
        0x0575, 0x0545, // ARMENIAN SMALL LETTER YI                                                 ARMENIAN CAPITAL LETTER YI
        0x0576, 0x0546, // ARMENIAN SMALL LETTER NOW                                                ARMENIAN CAPITAL LETTER NOW
        0x0577, 0x0547, // ARMENIAN SMALL LETTER SNA                                                ARMENIAN CAPITAL LETTER SHA
        0x0578, 0x0548, // ARMENIAN SMALL LETTER VO                                                 ARMENIAN CAPITAL LETTER VO
        0x0579, 0x0549, // ARMENIAN SMALL LETTER CHA                                                ARMENIAN CAPITAL LETTER CHA
        0x057A, 0x054A, // ARMENIAN SMALL LETTER PEH                                                ARMENIAN CAPITAL LETTER PEH
        0x057B, 0x054B, // ARMENIAN SMALL LETTER JHEH                                               ARMENIAN CAPITAL LETTER JHEH
        0x057C, 0x054C, // ARMENIAN SMALL LETTER RA                                                 ARMENIAN CAPITAL LETTER RA
        0x057D, 0x054D, // ARMENIAN SMALL LETTER SEH                                                ARMENIAN CAPITAL LETTER SEH
        0x057E, 0x054E, // ARMENIAN SMALL LETTER VEW                                                ARMENIAN CAPITAL LETTER VEW
        0x057F, 0x054F, // ARMENIAN SMALL LETTER TIWN                                               ARMENIAN CAPITAL LETTER TIWN
        0x0580, 0x0550, // ARMENIAN SMALL LETTER REH                                                ARMENIAN CAPITAL LETTER REH
        0x0581, 0x0551, // ARMENIAN SMALL LETTER CO                                                 ARMENIAN CAPITAL LETTER CO
        0x0582, 0x0552, // ARMENIAN SMALL LETTER YIWN                                               ARMENIAN CAPITAL LETTER YIWN
        0x0583, 0x0553, // ARMENIAN SMALL LETTER PIWP                                               ARMENIAN CAPITAL LETTER PIWR
        0x0584, 0x0554, // ARMENIAN SMALL LETTER KEH                                                ARMENIAN CAPITAL LETTER KEH
        0x0585, 0x0555, // ARMENIAN SMALL LETTER OH                                                 ARMENIAN CAPITAL LETTER OH
        0x0586, 0x0556, // ARMENIAN SMALL LETTER FEH                                                ARMENIAN CAPITAL LETTER FEH
        0x10D0, 0x10A0, // GEORGIAN LETTER AN                                                       GEORGIAN CAPITAL LETTER AN (KHUTSURI)
        0x10D1, 0x10A1, // GEORGIAN LETTER BAN                                                      GEORGIAN CAPITAL LETTER BAN (KHUTSURI)
        0x10D2, 0x10A2, // GEORGIAN LETTER GAN                                                      GEORGIAN CAPITAL LETTER GAN (KHUTSURI)
        0x10D3, 0x10A3, // GEORGIAN LETTER DON                                                      GEORGIAN CAPITAL LETTER DON (KHUTSURI)
        0x10D4, 0x10A4, // GEORGIAN LETTER EN                                                       GEORGIAN CAPITAL LETTER EN (KHUTSURI)
        0x10D5, 0x10A5, // GEORGIAN LETTER VIN                                                      GEORGIAN CAPITAL LETTER VIN (KHUTSURI)
        0x10D6, 0x10A6, // GEORGIAN LETTER ZEN                                                      GEORGIAN CAPITAL LETTER ZEN (KHUTSURI)
        0x10D7, 0x10A7, // GEORGIAN LETTER TAN                                                      GEORGIAN CAPITAL LETTER TAN (KHUTSURI)
        0x10D8, 0x10A8, // GEORGIAN LETTER IN                                                       GEORGIAN CAPITAL LETTER IN (KHUTSURI)
        0x10D9, 0x10A9, // GEORGIAN LETTER KAN                                                      GEORGIAN CAPITAL LETTER KAN (KHUTSURI)
        0x10DA, 0x10AA, // GEORGIAN LETTER LAS                                                      GEORGIAN CAPITAL LETTER LAS (KHUTSURI)
        0x10DB, 0x10AB, // GEORGIAN LETTER MAN                                                      GEORGIAN CAPITAL LETTER MAN (KHUTSURI)
        0x10DC, 0x10AC, // GEORGIAN LETTER NAR                                                      GEORGIAN CAPITAL LETTER NAR (KHUTSURI)
        0x10DD, 0x10AD, // GEORGIAN LETTER ON                                                       GEORGIAN CAPITAL LETTER ON (KHUTSURI)
        0x10DE, 0x10AE, // GEORGIAN LETTER PAR                                                      GEORGIAN CAPITAL LETTER PAR (KHUTSURI)
        0x10DF, 0x10AF, // GEORGIAN LETTER ZHAR                                                     GEORGIAN CAPITAL LETTER ZHAR (KHUTSURI)
        0x10E0, 0x10B0, // GEORGIAN LETTER RAE                                                      GEORGIAN CAPITAL LETTER RAE (KHUTSURI)
        0x10E1, 0x10B1, // GEORGIAN LETTER SAN                                                      GEORGIAN CAPITAL LETTER SAN (KHUTSURI)
        0x10E2, 0x10B2, // GEORGIAN LETTER TAR                                                      GEORGIAN CAPITAL LETTER TAR (KHUTSURI)
        0x10E3, 0x10B3, // GEORGIAN LETTER UN                                                       GEORGIAN CAPITAL LETTER UN (KHUTSURI)
        0x10E4, 0x10B4, // GEORGIAN LETTER PHAR                                                     GEORGIAN CAPITAL LETTER PHAR (KHUTSURI)
        0x10E5, 0x10B5, // GEORGIAN LETTER KHAR                                                     GEORGIAN CAPITAL LETTER KHAR (KHUTSURI)
        0x10E6, 0x10B6, // GEORGIAN LETTER GHAN                                                     GEORGIAN CAPITAL LETTER GHAN (KHUTSURI)
        0x10E7, 0x10B7, // GEORGIAN LETTER QAR                                                      GEORGIAN CAPITAL LETTER QAR (KHUTSURI)
        0x10E8, 0x10B8, // GEORGIAN LETTER SHIN                                                     GEORGIAN CAPITAL LETTER SHIN (KHUTSURI)
        0x10E9, 0x10B9, // GEORGIAN LETTER CHIN                                                     GEORGIAN CAPITAL LETTER CHIN (KHUTSURI)
        0x10EA, 0x10BA, // GEORGIAN LETTER CAN                                                      GEORGIAN CAPITAL LETTER CAN (KHUTSURI)
        0x10EB, 0x10BB, // GEORGIAN LETTER JIL                                                      GEORGIAN CAPITAL LETTER JIL (KHUTSURI)
        0x10EC, 0x10BC, // GEORGIAN LETTER CIL                                                      GEORGIAN CAPITAL LETTER CIL (KHUTSURI)
        0x10ED, 0x10BD, // GEORGIAN LETTER CHAR                                                     GEORGIAN CAPITAL LETTER CHAR (KHUTSURI)
        0x10EE, 0x10BE, // GEORGIAN LETTER XAN                                                      GEORGIAN CAPITAL LETTER XAN (KHUTSURI)
        0x10EF, 0x10BF, // GEORGIAN LETTER JHAN                                                     GEORGIAN CAPITAL LETTER JHAN (KHUTSURI)
        0x10F0, 0x10C0, // GEORGIAN LETTER HAE                                                      GEORGIAN CAPITAL LETTER HAE (KHUTSURI)
        0x10F1, 0x10C1, // GEORGIAN LETTER HE                                                       GEORGIAN CAPITAL LETTER HE (KHUTSURI)
        0x10F2, 0x10C2, // GEORGIAN LETTER HIE                                                      GEORGIAN CAPITAL LETTER HIE (KHUTSURI)
        0x10F3, 0x10C3, // GEORGIAN LETTER WE                                                       GEORGIAN CAPITAL LETTER WE (KHUTSURI)
        0x10F4, 0x10C4, // GEORGIAN LETTER HAR                                                      GEORGIAN CAPITAL LETTER HAR (KHUTSURI)
        0x10F5, 0x10C5, // GEORGIAN LETTER HOE                                                      GEORGIAN CAPITAL LETTER HOE (KHUTSURI)
        0x1E01, 0x1E00, // LATIN SMALL LETTER A WITH RING BELOW                                     LATIN CAPITAL LETTER A WITH RING BELOW
        0x1E03, 0x1E02, // LATIN SMALL LETTER B WITH DOT ABOVE                                      LATIN CAPITAL LETTER B WITH DOT ABOVE
        0x1E05, 0x1E04, // LATIN SMALL LETTER B WITH DOT BELOW                                      LATIN CAPITAL LETTER B WITH DOT BELOW
        0x1E07, 0x1E06, // LATIN SMALL LETTER B WITH LINE BELOW                                     LATIN CAPITAL LETTER B WITH LINE BELOW
        0x1E09, 0x1E08, // LATIN SMALL LETTER C WITH CEDILLA AND ACUTE                              LATIN CAPITAL LETTER C WITH CEDILLA AND ACUTE
        0x1E0B, 0x1E0A, // LATIN SMALL LETTER D WITH DOT ABOVE                                      LATIN CAPITAL LETTER D WITH DOT ABOVE
        0x1E0D, 0x1E0C, // LATIN SMALL LETTER D WITH DOT BELOW                                      LATIN CAPITAL LETTER D WITH DOT BELOW
        0x1E0F, 0x1E0E, // LATIN SMALL LETTER D WITH LINE BELOW                                     LATIN CAPITAL LETTER D WITH LINE BELOW
        0x1E11, 0x1E10, // LATIN SMALL LETTER D WITH CEDILLA                                        LATIN CAPITAL LETTER D WITH CEDILLA
        0x1E13, 0x1E12, // LATIN SMALL LETTER D WITH CIRCUMFLEX BELOW                               LATIN CAPITAL LETTER D WITH CIRCUMFLEX BELOW
        0x1E15, 0x1E14, // LATIN SMALL LETTER E WITH MACRON AND GRAVE                               LATIN CAPITAL LETTER E WITH MACRON AND GRAVE
        0x1E17, 0x1E16, // LATIN SMALL LETTER E WITH MACRON AND ACUTE                               LATIN CAPITAL LETTER E WITH MACRON AND ACUTE
        0x1E19, 0x1E18, // LATIN SMALL LETTER E WITH CIRCUMFLEX BELOW                               LATIN CAPITAL LETTER E WITH CIRCUMFLEX BELOW
        0x1E1B, 0x1E1A, // LATIN SMALL LETTER E WITH TILDE BELOW                                    LATIN CAPITAL LETTER E WITH TILDE BELOW
        0x1E1D, 0x1E1C, // LATIN SMALL LETTER E WITH CEDILLA AND BREVE                              LATIN CAPITAL LETTER E WITH CEDILLA AND BREVE
        0x1E1F, 0x1E1E, // LATIN SMALL LETTER F WITH DOT ABOVE                                      LATIN CAPITAL LETTER F WITH DOT ABOVE
        0x1E21, 0x1E20, // LATIN SMALL LETTER G WITH MACRON                                         LATIN CAPITAL LETTER G WITH MACRON
        0x1E23, 0x1E22, // LATIN SMALL LETTER H WITH DOT ABOVE                                      LATIN CAPITAL LETTER H WITH DOT ABOVE
        0x1E25, 0x1E24, // LATIN SMALL LETTER H WITH DOT BELOW                                      LATIN CAPITAL LETTER H WITH DOT BELOW
        0x1E27, 0x1E26, // LATIN SMALL LETTER H WITH DIAERESIS                                      LATIN CAPITAL LETTER H WITH DIAERESIS
        0x1E29, 0x1E28, // LATIN SMALL LETTER H WITH CEDILLA                                        LATIN CAPITAL LETTER H WITH CEDILLA
        0x1E2B, 0x1E2A, // LATIN SMALL LETTER H WITH BREVE BELOW                                    LATIN CAPITAL LETTER H WITH BREVE BELOW
        0x1E2D, 0x1E2C, // LATIN SMALL LETTER I WITH TILDE BELOW                                    LATIN CAPITAL LETTER I WITH TILDE BELOW
        0x1E2F, 0x1E2E, // LATIN SMALL LETTER I WITH DIAERESIS AND ACUTE                            LATIN CAPITAL LETTER I WITH DIAERESIS AND ACUTE
        0x1E31, 0x1E30, // LATIN SMALL LETTER K WITH ACUTE                                          LATIN CAPITAL LETTER K WITH ACUTE
        0x1E33, 0x1E32, // LATIN SMALL LETTER K WITH DOT BELOW                                      LATIN CAPITAL LETTER K WITH DOT BELOW
        0x1E35, 0x1E34, // LATIN SMALL LETTER K WITH LINE BELOW                                     LATIN CAPITAL LETTER K WITH LINE BELOW
        0x1E37, 0x1E36, // LATIN SMALL LETTER L WITH DOT BELOW                                      LATIN CAPITAL LETTER L WITH DOT BELOW
        0x1E39, 0x1E38, // LATIN SMALL LETTER L WITH DOT BELOW AND MACRON                           LATIN CAPITAL LETTER L WITH DOT BELOW AND MACRON
        0x1E3B, 0x1E3A, // LATIN SMALL LETTER L WITH LINE BELOW                                     LATIN CAPITAL LETTER L WITH LINE BELOW
        0x1E3D, 0x1E3C, // LATIN SMALL LETTER L WITH CIRCUMFLEX BELOW                               LATIN CAPITAL LETTER L WITH CIRCUMFLEX BELOW
        0x1E3F, 0x1E3E, // LATIN SMALL LETTER M WITH ACUTE                                          LATIN CAPITAL LETTER M WITH ACUTE
        0x1E41, 0x1E40, // LATIN SMALL LETTER M WITH DOT ABOVE                                      LATIN CAPITAL LETTER M WITH DOT ABOVE
        0x1E43, 0x1E42, // LATIN SMALL LETTER M WITH DOT BELOW                                      LATIN CAPITAL LETTER M WITH DOT BELOW
        0x1E45, 0x1E44, // LATIN SMALL LETTER N WITH DOT ABOVE                                      LATIN CAPITAL LETTER N WITH DOT ABOVE
        0x1E47, 0x1E46, // LATIN SMALL LETTER N WITH DOT BELOW                                      LATIN CAPITAL LETTER N WITH DOT BELOW
        0x1E49, 0x1E48, // LATIN SMALL LETTER N WITH LINE BELOW                                     LATIN CAPITAL LETTER N WITH LINE BELOW
        0x1E4B, 0x1E4A, // LATIN SMALL LETTER N WITH CIRCUMFLEX BELOW                               LATIN CAPITAL LETTER N WITH CIRCUMFLEX BELOW
        0x1E4D, 0x1E4C, // LATIN SMALL LETTER O WITH TILDE AND ACUTE                                LATIN CAPITAL LETTER O WITH TILDE AND ACUTE
        0x1E4F, 0x1E4E, // LATIN SMALL LETTER O WITH TlLDE AND DIAERESIS                            LATIN CAPITAL LETTER O WITH TILDE AND DIAERESIS
        0x1E51, 0x1E50, // LATIN SMALL LETTER O WITH MACRON AND GRAVE                               LATIN CAPITAL LETTER O WITH MACRON AND GRAVE
        0x1E53, 0x1E52, // LATIN SMALL LETTER O WITH MACRON AND ACUTE                               LATIN CAPITAL LETTER O WITH MACRON AND ACUTE
        0x1E55, 0x1E54, // LATIN SMALL LETTER P WITH ACUTE                                          LATIN CAPITAL LETTER P WITH ACUTE
        0x1E57, 0x1E56, // LATIN SMALL LETTER P WITH DOT ABOVE                                      LATIN CAPITAL LETTER P WITH DOT ABOVE
        0x1E59, 0x1E58, // LATIN SMALL LETTER R WITH DOT ABOVE                                      LATIN CAPITAL LETTER R WITH DOT ABOVE
        0x1E5B, 0x1E5A, // LATIN SMALL LETTER R WITH DOT BELOW                                      LATIN CAPITAL LETTER R WITH DOT BELOW
        0x1E5D, 0x1E5C, // LATIN SMALL LETTER R WITH DOT BELOW AND MACRON                           LATIN CAPITAL LETTER R WITH DOT BELOW AND MACRON
        0x1E5F, 0x1E5E, // LATIN SMALL LETTER R WITH LINE BELOW                                     LATIN CAPITAL LETTER R WITH LINE BELOW
        0x1E61, 0x1E60, // LATIN SMALL LETTER S WITH DOT ABOVE                                      LATIN CAPITAL LETTER S WITH DOT ABOVE
        0x1E63, 0x1E62, // LATIN SMALL LETTER S WITH DOT BELOW                                      LATIN CAPITAL LETTER S WITH DOT BELOW
        0x1E65, 0x1E64, // LATIN SMALL LETTER S WITH ACUTE AND DOT ABOVE                            LATIN CAPITAL LETTER S WITH ACUTE AND DOT ABOVE
        0x1E67, 0x1E66, // LATIN SMALL LETTER S WITH CARON AND DOT ABOVE                            LATIN CAPITAL LETTER S WITH CARON AND DOT ABOVE
        0x1E69, 0x1E68, // LATIN SMALL LETTER S WITH DOT BELOW AND DOT ABOVE                        LATIN CAPITAL LETTER S WITH DOT BELOW AND DOT ABOVE
        0x1E6B, 0x1E6A, // LATIN SMALL LETTER T WITH DOT ABOVE                                      LATIN CAPITAL LETTER T WITH DOT ABOVE
        0x1E6D, 0x1E6C, // LATIN SMALL LETTER T WITH DOT BELOW                                      LATIN CAPITAL LETTER T WITH DOT BELOW
        0x1E6F, 0x1E6E, // LATIN SMALL LETTER T WITH LINE BELOW                                     LATIN CAPITAL LETTER T WITH LINE BELOW
        0x1E71, 0x1E70, // LATIN SMALL LETTER T WITH CIRCUMFLEX BELOW                               LATIN CAPITAL LETTER T WITH CIRCUMFLEX BELOW
        0x1E73, 0x1E72, // LATIN SMALL LETTER U WITH DIAERESIS BELOW                                LATIN CAPITAL LETTER U WITH DIAERESIS BELOW
        0x1E75, 0x1E74, // LATIN SMALL LETTER U WITH TILDE BELOW                                    LATIN CAPITAL LETTER U WITH TILDE BELOW
        0x1E77, 0x1E76, // LATIN SMALL LETTER U WITH CIRCUMFLEX BELOW                               LATIN CAPITAL LETTER U WITH CIRCUMFLEX BELOW
        0x1E79, 0x1E78, // LATIN SMALL LETTER U WITH TILDE AND ACUTE                                LATIN CAPITAL LETTER U WITH TILDE AND ACUTE
        0x1E7B, 0x1E7A, // LATIN SMALL LETTER U WITH MACRON AND DIAERESIS                           LATIN CAPITAL LETTER U WITH MACRON AND DIAERESIS
        0x1E7D, 0x1E7C, // LATIN SMALL LETTER V WITH TILDE                                          LATIN CAPITAL LETTER V WITH TILDE
        0x1E7F, 0x1E7E, // LATIN SMALL LETTER V WITH DOT BELOW                                      LATIN CAPITAL LETTER V WITH DOT BELOW
        0x1E81, 0x1E80, // LATIN SMALL LETTER W WITH GRAVE                                          LATIN CAPITAL LETTER W WITH GRAVE
        0x1E83, 0x1E82, // LATIN SMALL LETTER W WITH ACUTE                                          LATIN CAPITAL LETTER W WITH ACUTE
        0x1E85, 0x1E84, // LATIN SMALL LETTER W WITH DIAERESIS                                      LATIN CAPITAL LETTER W WITH DIAERESIS
        0x1E87, 0x1E86, // LATIN SMALL LETTER W WITH DOT ABOVE                                      LATIN CAPITAL LETTER W WITH DOT ABOVE
        0x1E89, 0x1E88, // LATIN SMALL LETTER W WITH DOT BELOW                                      LATIN CAPITAL LETTER W WITH DOT BELOW
        0x1E8B, 0x1E8A, // LATIN SMALL LETTER X WITH DOT ABOVE                                      LATIN CAPITAL LETTER X WITH DOT ABOVE
        0x1E8D, 0x1E8C, // LATIN SMALL LETTER X WITH DIAERESIS                                      LATIN CAPITAL LETTER X5 WITH DIAERESIS
        0x1E8F, 0x1E8E, // LATIN SMALL LETTER Y WITH DOT ABOVE                                      LATIN CAPITAL LETTER Y WITH DOT ABOVE
        0x1E91, 0x1E90, // LATIN SMALL LETTER Z WITH CIRCUMFLEX                                     LATIN CAPITAL LETTER Z WITH CIRCUMFLEX
        0x1E93, 0x1E92, // LATIN SMALL LETTER Z WITH DOT BELOW                                      LATIN CAPITAL LETTER Z WITH DOT BELOW
        0x1E95, 0x1E94, // LATIN SMALL LETTER Z WITH LINE BELOW                                     LATIN CAPITAL LETTER Z WITH LINE BELOW
        0x1EA1, 0x1EA0, // LATIN SMALL LETTER A WITH DOT BELOW                                      LATIN CAPITAL LETTER A WITH DOT BELOW
        0x1EA3, 0x1EA2, // LATIN SMALL LETTER A WITH HOOK ABOVE                                     LATIN CAPITAL LETTER A WITH HOOK ABOVE
        0x1EA5, 0x1EA4, // LATIN SMALL LETTER A WITH CIRCUMFLEX AND ACUTE                           LATIN CAPITAL LETTER A WITH CIRCUMFLEX AND ACUTE
        0x1EA7, 0x1EA6, // LATIN SMALL LETTER A WITH CIRCUMFLEX AND GRAVE                           LATIN CAPITAL LETTER A WITH CIRCUMFLEX AND GRAVE
        0x1EA9, 0x1EA8, // LATIN SMALL LETTER A WITH CIRCUMFLEX AND HOOK ABOVE                      LATIN CAPITAL LETTER A WITH CIRCUMFLEX AND HOOK ABOVE
        0x1EAB, 0x1EAA, // LATIN SMALL LETTER A WITH CIRCUMFLEX AND TILDE                           LATIN CAPITAL LETTER A WITH CIRCUMFLEX AND TILDE
        0x1EAD, 0x1EAC, // LATIN SMALL LETTER A WITH CIRCUMFLEX AND DOT BELOW                       LATIN CAPITAL LETTER A WITH CIRCUMFLEX AND DOT BELOW
        0x1EAF, 0x1EAE, // LATIN SMALL LETTER A WITH BREVE AND ACUTE                                LATIN CAPITAL LETTER A WITH BREVE AND ACUTE
        0x1EB1, 0x1EB0, // LATIN SMALL LETTER A WITH BREVE AND GRAVE                                LATIN CAPITAL LETTER A WITH BREVE AND GRAVE
        0x1EB3, 0x1EB2, // LATIN SMALL LETTER A WITH BREVE AND HOOK ABOVE                           LATIN CAPITAL LETTER A WITH BREVE AND HOOK ABOVE
        0x1EB5, 0x1EB4, // LATIN SMALL LETTER A WITH BREVE AND TILDE                                LATIN CAPITAL LETTER A WITH BREVE AND TILDE
        0x1EB7, 0x1EB6, // LATIN SMALL LETTER A WITH BREVE AND DOT BELOW                            LATIN CAPITAL LETTER A WITH BREVE AND DOT BELOW
        0x1EB9, 0x1EB8, // LATIN SMALL LETTER E WITH DOT BELOW                                      LATIN CAPITAL LETTER E WITH DOT BELOW
        0x1EBB, 0x1EBA, // LATIN SMALL LETTER E WITH HOOK ABOVE                                     LATIN CAPITAL LETTER E WITH HOOK ABOVE
        0x1EBD, 0x1EBC, // LATIN SMALL LETTER E WITH TILDE                                          LATIN CAPITAL LETTER E WITH TILDE
        0x1EBF, 0x1EBE, // LATIN SMALL LETTER E WITH CIRCUMFLEX AND ACUTE                           LATIN CAPITAL LETTER E WITH CIRCUMFLEX AND ACUTE
        0x1EC1, 0x1EC0, // LATIN SMALL LETTER E WITH CIRCUMFLEX AND GRAVE                           LATIN CAPITAL LETTER E WITH CIRCUMFLEX AND GRAVE
        0x1EC3, 0x1EC2, // LATIN SMALL LETTER E WITH CIRCUMFLEX AND HOOK ABOVE                      LATIN CAPITAL LETTER E WITH CIRCUMFLEX AND HOOK ABOVE
        0x1EC5, 0x1EC4, // LATIN SMALL LETTER E WITH CIRCUMFLEX AND TILDE                           LATIN CAPITAL LETTER E WITH CIRCUMFLEX AND TILDE
        0x1EC7, 0x1EC6, // LATIN SMALL LETTER E WITH CIRCUMFLEX AND DOT BELOW                       LATIN CAPITAL LETTER E WITH CIRCUMFLEX AND DOT BELOW
        0x1EC9, 0x1EC8, // LATIN SMALL LETTER I WITH HOOK ABOVE                                     LATIN CAPITAL LETTER I WITH HOOK ABOVE
        0x1ECB, 0x1ECA, // LATIN SMALL LETTER I WITH DOT BELOW                                      LATIN CAPITAL LETTER I WITH DOT BELOW
        0x1ECD, 0x1ECC, // LATIN SMALL LETTER O WITH DOT BELOW                                      LATIN CAPITAL LETTER O WITH DOT BELOW
        0x1ECF, 0x1ECE, // LATIN SMALL LETTER O WITH HOOK ABOVE                                     LATIN CAPITAL LETTER O WITH HOOK ABOVE
        0x1ED1, 0x1ED0, // LATIN SMALL LETTER O WITH CIRCUMFLEX AND ACUTE                           LATIN CAPITAL LETTER O WITH CIRCUMFLEX AND ACUTE
        0x1ED3, 0x1ED2, // LATIN SMALL LETTER O WITH CIRCUMFLEX AND GRAVE                           LATIN CAPITAL LETTER O WITH CIRCUMFLEX AND GRAVE
        0x1ED5, 0x1ED4, // LATIN SMALL LETTER O WITH CIRCUMFLEX AND HOOK ABOVE                      LATIN CAPITAL LETTER O WITH CIRCUMFLEX AND HOOK ABOVE
        0x1ED7, 0x1ED6, // LATIN SMALL LETTER O WITH CIRCUMFLEX AND TILDE                           LATIN CAPITAL LETTER O WITH CIRCUMFLEX AND TILDE
        0x1ED9, 0x1ED8, // LATIN SMALL LETTER O WITH CIRCUMFLEX AND DOT BELOW                       LATIN CAPITAL LETTER O WITH CIRCUMFLEX AND DOT BELOW
        0x1EDB, 0x1EDA, // LATIN SMALL LETTER O WITH HORN AND ACUTE                                 LATIN CAPITAL LETTER O WITH HORN AND ACUTE
        0x1EDD, 0x1EDC, // LATIN SMALL LETTER O WITH HORN AND GRAVE                                 LATIN CAPITAL LETTER O WITH HORN AND GRAVE
        0x1EDF, 0x1EDE, // LATIN SMALL LETTER O WITH HORN AND HOOK ABOVE                            LATIN CAPITAL LETTER O WITH HORN AND HOOK ABOVE
        0x1EE1, 0x1EE0, // LATIN SMALL LETTER O WITH HORN AND TILDE                                 LATIN CAPITAL LETTER O WITH HORN AND TILDE
        0x1EE3, 0x1EE2, // LATIN SMALL LETTER O WITH HORN AND DOT BELOW                             LATIN CAPITAL LETTER O WITH HORN AND DOT BELOW
        0x1EE5, 0x1EE4, // LATIN SMALL LETTER U WITH DOT BELOW                                      LATIN CAPITAL LETTER U WITH DOT BELOW
        0x1EE7, 0x1EE6, // LATIN SMALL LETTER U WITH HOOK ABOVE                                     LATIN CAPITAL LETTER U WITH HOOK ABOVE
        0x1EE9, 0x1EE8, // LATIN SMALL LETTER U WITH HORN AND ACUTE                                 LATIN CAPITAL LETTER U WITH HORN AND ACUTE
        0x1EEB, 0x1EEA, // LATIN SMALL LETTER U WITH HORN AND GRAVE                                 LATIN CAPITAL LETTER U WITH HORN AND GRAVE
        0x1EED, 0x1EEC, // LATIN SMALL LETTER U WITH HORN AND HOCK ABOVE                            LATIN CAPITAL LETTER U WITH HORN AND HOOK ABOVE
        0x1EEF, 0x1EEE, // LATIN SMALL LETTER U WITH HORN AND TILDE                                 LATIN CAPITAL LETTER U WITH HORN AND TILDE
        0x1EF1, 0x1EF0, // LATIN SMALL LETTER U WITH HORN AND DOT BELOW                             LATIN CAPITAL LETTER U WITH HORN AND DOT BELOW
        0x1EF3, 0x1EF2, // LATIN SMALL LETTER Y WITH GRAVE                                          LATIN CAPITAL LETTER Y WITH GRAVE
        0x1EF5, 0x1EF4, // LATIN SMALL LETTER Y WITH DOT BELOW                                      LATIN CAPITAL LETTER Y WITH DOT BELOW
        0x1EF7, 0x1EF6, // LATIN SMALL LETTER Y WITH HOOK ABOVE                                     LATIN CAPITAL LETTER Y WITH HOOK ABOVE
        0x1EF9, 0x1EF8, // LATIN SMALL LETTER Y WITH TILDE                                          LATIN CAPITAL LETTER Y WITH TILDE
        0x1F00, 0x1F08, // GREEK SMALL LETTER ALPHA WITH PSILI                                      GREEK CAPITAL LETTER ALPHA WITH PSILI
        0x1F01, 0x1F09, // GREEK SMALL LETTER ALPHA WITH DASIA                                      GREEK CAPITAL LETTER ALPHA WITH DASIA
        0x1F02, 0x1F0A, // GREEK SMALL LETTER ALPHA WITH PSILI AND VARIA                            GREEK CAPITAL LETTER ALPHA WITH PSILI AND VARIA
        0x1F03, 0x1F0B, // GREEK SMALL LETTER ALPHA WITH DASIA AND VARIA                            GREEK CAPITAL LETTER ALPHA WITH DASIA AND VARIA
        0x1F04, 0x1F0C, // GREEK SMALL LETTER ALPHA WITH PSILI AND OXIA                             GREEK CAPITAL LETTER ALPHA WITH PSILI AND OXIA
        0x1F05, 0x1F0D, // GREEK SMALL LETTER ALPHA WITH DASIA AND OXIA                             GREEK CAPITAL LETTER ALPHA WITH DASIA AND OXIA
        0x1F06, 0x1F0E, // GREEK SMALL LETTER ALPHA WITH PSILI AND PERISPOMENI                      GREEK CAPITAL LETTER ALPHA WITH PSILI AND PERISPOMENI
        0x1F07, 0x1F0F, // GREEK SMALL LETTER ALPHA WITH DASIA AND PERISPOMENI                      GREEK CAPITAL LETTER ALPHA WITH DASIA AND PERISPOMENI
        0x1F10, 0x1F18, // GREEK SMALL LETTER EPSILON WITH PSILI                                    GREEK CAPITAL LETTER EPSILON WITH PSILI
        0x1F11, 0x1F19, // GREEK SMALL LETTER EPSILON WITH DASIA                                    GREEK CAPITAL LETTER EPSILON WITH DASIA
        0x1F12, 0x1F1A, // GREEK SMALL LETTER EPSILON WITH PSILI AND VARIA                          GREEK CAPITAL LETTER EPSILON WITH PSILI AND VARIA
        0x1F13, 0x1F1B, // GREEK SMALL LETTER EPSILON WITH DASIA AND VARIA                          GREEK CAPITAL LETTER EPSILON WITH DASIA AND VARIA
        0x1F14, 0x1F1C, // GREEK SMALL LETTER EPSILON WITH PSILI AND OXIA                           GREEK CAPITAL LETTER EPSILON WITH PSILI AND OXIA
        0x1F15, 0x1F1D, // GREEK SMALL LETTER EPSILON WITH DASIA AND OXIA                           GREEK CAPITAL LETTER EPSILON WITH DASIA AND OXIA
        0x1F20, 0x1F28, // GREEK SMALL LETTER ETA WITH PSILI                                        GREEK CAPITAL LETTER ETA WITH PSILI
        0x1F21, 0x1F29, // GREEK SMALL LETTER ETA WITH DASIA                                        GREEK CAPITAL LETTER ETA WITH DASIA
        0x1F22, 0x1F2A, // GREEK SMALL LETTER ETA WITH PSILI AND VARIA                              GREEK CAPITAL LETTER ETA WITH PSILI AND VARIA
        0x1F23, 0x1F2B, // GREEK SMALL LETTER ETA WITH DASIA AND VARIA                              GREEK CAPITAL LETTER ETA WITH DASIA AND VARIA
        0x1F24, 0x1F2C, // GREEK SMALL LETTER ETA WITH PSILI AND OXIA                               GREEK CAPITAL LETTER ETA WITH PSILI AND OXIA
        0x1F25, 0x1F2D, // GREEK SMALL LETTER ETA WITH DASIA AND OXIA                               GREEK CAPITAL LETTER ETA WITH DASIA AND OXIA
        0x1F26, 0x1F2E, // GREEK SMALL LETTER ETA WITH PSILI AND PERISPOMENI                        GREEK CAPITAL LETTER ETA WITH PSILI AND PERISPOMENI
        0x1F27, 0x1F2F, // GREEK SMALL LETTER ETA WITH DASIA AND PERISPOMENI                        GREEK CAPITAL LETTER ETA WITH DASIA AND PERISPOMENI
        0x1F30, 0x1F38, // GREEK SMALL LETTER IOTA WITH PSILI                                       GREEK CAPITAL LETTER IOTA WITH PSILI
        0x1F31, 0x1F39, // GREEK SMALL LETTER IOTA WITH DASIA                                       GREEK CAPITAL LETTER IOTA WITH DASIA
        0x1F32, 0x1F3A, // GREEK SMALL LETTER IOTA WITH PSILI AND VARIA                             GREEK CAPITAL LETTER IOTA WITH PSILI AND VARIA
        0x1F33, 0x1F3B, // GREEK SMALL LETTER IOTA WITH DASIA AND VARIA                             GREEK CAPITAL LETTER IOTA WITH DASIA AND VARIA
        0x1F34, 0x1F3C, // GREEK SMALL LETTER IOTA WITH PSILI AND OXIA                              GREEK CAPITAL LETTER IOTA WITH PSILI AND OXIA
        0x1F35, 0x1F3D, // GREEK SMALL LETTER IOTA WITH DASIA AND OXIA                              GREEK CAPITAL LETTER IOTA WITH DASIA AND OXIA
        0x1F36, 0x1F3E, // GREEK SMALL LETTER IOTA WITH PSILI AND PERISPOMENI                       GREEK CAPITAL LETTER IOTA WITH PSILI AND PERISPOMENI
        0x1F37, 0x1F3F, // GREEK SMALL LETTER IOTA WITH DASIA AND PERISPOMENI                       GREEK CAPITAL LETTER IOTA WITH DASIA AND PERISPOMENI
        0x1F40, 0x1F48, // GREEK SMALL LETTER OMICRON WITH PSILI                                    GREEK CAPITAL LETTER OMICRON WITH PSILI
        0x1F41, 0x1F49, // GREEK SMALL LETTER OMICRON WITH DASIA                                    GREEK CAPITAL LETTER OMICRON WITH DASIA
        0x1F42, 0x1F4A, // GREEK SMALL LETTER OMICRON WITH PSILI AND VARIA                          GREEK CAPITAL LETTER OMICRON WITH PSILI AND VARIA
        0x1F43, 0x1F4B, // GREEK SMALL LETTER OMICRON WITH DASIA AND VARIA                          GREEK CAPITAL LETTER OMICRON WITH DASIA AND VARIA
        0x1F44, 0x1F4C, // GREEK SMALL LETTER OMICRON WITH PSILI AND OXIA                           GREEK CAPITAL LETTER OMICRON WITH PSILI AND OXIA
        0x1F45, 0x1F4D, // GREEK SMALL LETTER OMICRON WITH DASIA AND OXIA                           GREEK CAPITAL LETTER OMICRON WITH DASIA AND OXIA
        0x1F51, 0x1F59, // GREEK SMALL LETTER UPSILON WITH DASIA                                    GREEK CAPITAL LETTER UPSILON WITH OASIS
        0x1F53, 0x1F5B, // GREEK SMALL LETTER UPSILON WITH DASIA AND VARIA                          GREEK CAPITAL LETTER UPSILON WITH DASIA AND VARIA
        0x1F55, 0x1F5D, // GREEK SMALL LETTER UPSILON WITH DASIA AND OXIA                           GREEK CAPITAL LETTER UPSILON WITH DASIA AND OXIA
        0x1F57, 0x1F5F, // GREEK SMALL LETTER UPSILON WITH DASIA AND PERISPOMENI                    GREEK CAPITAL LETTER UPSILON WITH DASIA AND PERISPOMENI
        0x1F60, 0x1F68, // GREEK SMALL LETTER OMEGA WITh PSILI                                      GREEK CAPITAL LETTER OMEGA WITH PSILI
        0x1F61, 0x1F69, // GREEK SMALL LETTER OMEGA WITH DASIA                                      GREEK CAPITAL LETTER OMEGA WITH DASIA
        0x1F62, 0x1F6A, // GREEK SMALL LETTER OMEGA WITH PSILI AND VARIA                            GREEK CAPITAL LETTER OMEGA WITH PSILI AND VARIA
        0x1F63, 0x1F6B, // GREEK SMALL LETTER OMEGA WITH DASIA AND VARIA                            GREEK CAPITAL LETTER OMEGA WITH DASIA AND VARIA
        0x1F64, 0x1F6C, // GREEK SMALL LETTER OMEGA WITH PSILI AND OXIA                             GREEK CAPITAL LETTER OMEGA WITH PSILI AND OXIA
        0x1F65, 0x1F6D, // GREEK SMALL LETTER OMEGA WITH DASIA AND OXIA                             GREEK CAPITAL LETTER OMEGA WITH DASIA AND OXIA
        0x1F66, 0x1F6E, // GREEK SMALL LETTER OMEGA WITH PSILI AND PERISPOMENI                      GREEK CAPITAL LETTER OMEGA WITH PSILI AND PERISPOMENI
        0x1F67, 0x1F6F, // GREEK SMALL LETTER OMEGA WITH DASIA AND PERISPOMENI                      GREEK CAPITAL LETTER OMEGA WITH DASIA AND PERISPOMENI
        0x1F80, 0x1F88, // GREEK SMALL LETTER ALPHA WITH PSILI AND YPOGEGRAMMENI                    GREEK CAPITAL LETTER ALPHA WITh PSILI AND PROSGEGRAMMENI
        0x1F81, 0x1F89, // GREEK SMALL LETTER ALPHA WITH DASIA AND YPOGEGRAMMENI                    GREEK CAPITAL LETTER ALPHA WITH DASIA AND PROSGEGRAMMENI
        0x1F82, 0x1F8A, // GREEK SMALL LETTER ALPHA WITH PSILI AND VARIA AND YPOGEGRAMMENI          GREEK CAPITAL LETTER ALPHA WITH PSILI AND VARIA AND PROSGEGRAMMENI
        0x1F83, 0x1F8B, // GREEK SMALL LETTER ALPHA WITH DASIA AND VARIA AND YPOGEGRAMMENI          GREEK CAPITAL LETTER ALPHA WITH DASIA AND VARIA AND PROSGEGRAMMENI
        0x1F84, 0x1F8C, // GREEK SMALL LETTER ALPHA WITH PSILI AND OXIA AND YPOGEGRAMMENI           GREEK CAPITAL LETTER ALPHA WITH PSILI AND OXIA AND PROSGEGRAMMEN
        0x1F85, 0x1F8D, // GREEK SMALL LETTER ALPHA WITH DASIA AND OXIA AND YPOGEGRAMMENI           GREEK CAPITAL LETTER ALPHA WITH DASIA AND OXIA AND PROSGEGRAMMEN
        0x1F86, 0x1F8E, // GREEK SMALL LETTER ALPHA WITH PSILI AND PERISPOMENI AND YPOGEGRAMMENI    GREEK CAPITAL LETTER ALPHA WITH PSILI AND PERISPOMENI AND PROSGEGRAMMENI
        0x1F87, 0x1F8F, // GREEK SMALL LETTER ALPHA WITH DASIA AND PERISPOMENI AND YPOGEGRAMMENI    GREEK CAPITAL LETTER ALPHA WITH DASIA AND PERISPOMENI AND PROSGEGRAMMENI
        0x1F90, 0x1F98, // GREEK SMALL LETTER ETA WITH PSILI AND YPOGEGRAMMENI                      GREEK CAPITAL LETTER ETA WITH PSILI AND PROSGEGRAMMENI
        0x1F91, 0x1F99, // GREEK SMALL LETTER ETA WITH DASIA AND YPOGEGRAMMENI                      GREEK CAPITAL LETTER ETA WITH DASIA AND PROSGEGRAMMENI
        0x1F92, 0x1F9A, // GREEK SMALL LETTER ETA WITH PSILI AND VARIA AND YPOGEGRAMMENI            GREEK CAPITAL LETTER ETA WITH PSILI AND VARIA AND PROSGEGRAMMENI
        0x1F93, 0x1F9B, // GREEK SMALL LETTER ETA WITH DASIA AND VARIA AND YPOGEGRAMMENI            GREEK CAPITAL LETTER ETA WITH DASIA AND VARIA AND PROSGEGRAMMENI
        0x1F94, 0x1F9C, // GREEK SMALL LETTER ETA WITH PSILI AND OXIA AND YPOGEGRAMMENI             GREEK CAPITAL LETTER ETA WITH PSILI AND OXIA AND PROSGEGRAMMENI
        0x1F95, 0x1F9D, // GREEK SMALL LETTER ETA WITH DASIA AND OXIA AND YPOGEGRAMMENI             GREEK CAPITAL LETTER ETA WITH DASIA AND OXIA AND PROSGEGRAMMENI
        0x1F96, 0x1F9E, // GREEK SMALL LETTER ETA WITH PSILI AND PERISPOMENI AND YPOGEGRAMMENI      GREEK CAPITAL LETTER ETA WITH PSILI AND PERISPOMENI AND PROSGEGRAMMENI
        0x1F97, 0x1F9F, // GREEK SMALL LETTER ETA WITH DASIA AND PERISPOMENI AND YPOGEGRAMMENI      GREEK CAPITAL LETTER ETA WITH DASIA AND PERISPOMENI AND PROSGEGRAMMENI
        0x1FA0, 0x1FA8, // GREEK SMALL LETTER OMEGA WITH PSILI AND YPOGEGRAMMENI                    GREEK CAPITAL LETTER OMEGA WITH PSILI AND PROSGEGRAMMENI
        0x1FA1, 0x1FA9, // GREEK SMALL LETTER OMEGA WITH DASIA AND YPOGEGRAMMENI                    GREEK CAPITAL LETTER OMEGA WITH DASIA AND PROSGEGRAMMENI
        0x1FA2, 0x1FAA, // GREEK SMALL LETTER OMEGA WITH PSILI AND VARIA AND YPOGEGRAMMENI GREEK    CAPITAL LETTER OMEGA WITH PSILI AND VARIA AND PROSGEGRAMMENI
        0x1FA3, 0x1FAB, // GREEK SMALL LETTER OMEGA WITH DASIA AND VARIA AND YPOGEGRAMMENI GREEK    CAPITAL LETTER OMEGA WITH DASIA AND VARIA AND PROSGEGRAMMENI
        0x1FA4, 0x1FAC, // GREEK SMALL LETTER OMEGA WITH PSILI AND OXIA AND YPOGEGRAMMENI  GREEK    CAPITAL LETTER OMEGA WITH PSILI AND OXIA AND PROSGEGRAMMENI
        0x1FA5, 0x1FAD, // GREEK SMALL LETTER OMEGA WITH DASIA AND OXIA AND YPOGEGRAMMENI  GREEK    CAPITAL LETTER OMEGA WITH DASIA AND OXIA AND PROSGEGRAMMENI
        0x1FA6, 0x1FAE, // GREEK SMALL LETTER OMEGA WITh PSILI AND PERISPOMENI AND YPOGEGRAMMENI    GREEK CAPITAL LETTER OMEGA WITH PSILI AND PERISPOMENI AND PROSGEGRAMMENI
        0x1FA7, 0x1FAF, // GREEK SMALL LETTER OMEGA WITH DASIA AND PEPISPOMENI AND YPOGEGRAMMENI    GREEK CAPITAL LETTER OMECA WITH DASIA AND PERISPOMENI AND PROSGEGRAMMENI
        0x1FB0, 0x1FB8, // GREEK SMALL LETTER ALPHA WITH VRACHY                                     GREEK CAPITAL LETTER ALPHA WITH VRACHY
        0x1FB1, 0x1FB9, // GREEK SMALL LETTER ALPHA WITH MACRON                                     GREEK CAPITAL LETTER ALPHA WITH MACRON
        0x1FD0, 0x1FD8, // GREEK SMALL LETTER IOTA WITH VRACHY                                      GREEK CAPITAL LETTER IOTA WITH VRACHY
        0x1FD1, 0x1FD9, // GREEK SMALL LETTER IOTA WITH MACRON                                      GREEK CAPITAL LETTER IOTA WITH MACRON
        0x1FE0, 0x1FE8, // GREEK SMALL LETTER UPSILON WITH VRACHY                                   GREEK CAPITAL LETTER UPSILON WITH VRACHY
        0x1FE1, 0x1FE9, // GREEK SMALL LETTER UPSILON WITH MACRON                                   GREEK CAPITAL LETTER UPSILON WITH MACRON
        0x24D0, 0x24B6, // CIRCLED LATIN SMALL LETTER A                                             CIRCLED LATIN CAPITAL LETTER A
        0x24D1, 0x24B7, // CIRCLED LATIN SMALL LETTER B                                             CIRCLED LATIN CAPITAL LETTER B
        0x24D2, 0x24B8, // CIRCLED LATIN SMALL LETTER C                                             CIRCLED LATIN CAPITAL LETTER C
        0x24D3, 0x24B9, // CIRCLED LATIN SMALL LETTER D                                             CIRCLED LATIN CAPITAL LETTER D
        0x24D4, 0x24BA, // CIRCLED LATIN SMALL LETTER E                                             CIRCLED LATIN CAPITAL LETTER E
        0x24D5, 0x24BB, // CIRCLED LATIN SMALL LETTER F                                             CIRCLED LATIN CAPITAL LETTER F
        0x24D6, 0x24BC, // CIRCLED LATIN SMALL LETTER G                                             CIRCLED LATIN CAPITAL LETTER G
        0x24D7, 0x24BD, // CIRCLED LATIN SMALL LETTER H                                             CIRCLED LATIN CAPITAL LETTER H
        0x24D8, 0x24BE, // CIRCLED LATIN SMALL LETTER I                                             CIRCLED LATIN CAPITAL LETTER I
        0x24D9, 0x24BF, // CIRCLED LATIN SMALL LETTER J                                             CIRCLED LATIN CAPITAL LETTER J
        0x24DA, 0x24C0, // CIRCLED LATIN SMALL LETTER K                                             CIRCLED LATIN CAPITAL LETTER K
        0x24DB, 0x24C1, // CIRCLED LATIN SMALL LETTER L                                             CIRCLED LATIN CAPITAL LETTER L
        0x24DC, 0x24C2, // CIRCLED LATIN SMALL LETTER M                                             CIRCLED LATIN CAPITAL LETTER M
        0x24DD, 0x24C3, // CIRCLED LATIN SMALL LETTER N                                             CIRCLED LATIN CAPITAL LETTER N
        0x24DE, 0x24C4, // CIRCLED LATIN SMALL LETTER O                                             CIRCLED LATIN CAPITAL LETTER O
        0x24DF, 0x24C5, // CIRCLED LATIN SMALL LETTER P                                             CIRCLED LATIN CAPITAL LETTER P
        0x24E0, 0x24C6, // CIRCLED LATIN SMALL LETTER Q                                             CIRCLED LATIN CAPITAL LETTER Q
        0x24E1, 0x24C7, // CIRCLED LATIN SMALL LETTER R                                             CIRCLED LATIN CAPITAL LETTER R
        0x24E2, 0x24C8, // CIRCLED LATIN SMALL LETTER S                                             CIRCLED LATIN CAPITAL LETTER S
        0x24E3, 0x24C9, // CIRCLED LATIN SMALL LETTER T                                             CIRCLED LATIN CAPITAL LETTER T
        0x24E4, 0x24CA, // CIRCLED LATIN SMALL LETTER U                                             CIRCLED LATIN CAPITAL LETTER U
        0x24E5, 0x24CB, // CIRCLED LATIN SMALL LETTER V                                             CIRCLED LATIN CAPITAL LETTER V
        0x24E6, 0x24CC, // CIRCLED LATIN SMALL LETTER W                                             CIRCLED LATIN CAPITAL LETTER W
        0x24E7, 0x24CD, // CIRCLED LATIN SMALL LETTER X                                             CIRCLED LATIN CAPITAL LETTER X
        0x24E8, 0x24CE, // CIRCLED LATIN SMALL LETTER Y                                             CIRCLED LATIN CAPITAL LETTER Y
        0x24E9, 0x24CF, // CIRCLED LATIN SMALL LETTER Z                                             CIRCLED LATIN CAPITAL LETTER Z
        0xFF41, 0xFF21, // FULLWIDTH LATIN SMALL LETTER A                                           FULLWIDTH LATIN CAPITAL LETTER A
        0xFF42, 0xFF22, // FULLWIDTH LATIN SMALL LETTER B                                           FULLWIDTH LATIN CAPITAL LETTER B
        0xFF43, 0xFF23, // FULLWIDTH LATIN SMALL LETTER C                                           FULLWIDTH LATIN CAPITAL LETTER C
        0xFF44, 0xFF24, // FULLWIDTH LATIN SMALL LETTER D                                           FULLWIDTH LATIN CAPITAL LETTER D
        0xFF45, 0xFF25, // FULLWIDTH LATIN SMALL LETTER E                                           FULLWIDTH LATIN CAPITAL LETTER E
        0xFF46, 0xFF26, // FULLWIDTH LATIN SMALL LETTER F                                           FULLWIDTH LATIN CAPITAL LETTER F
        0xFF47, 0xFF27, // FULLWIDTH LATIN SMALL LETTER G                                           FULLWIDTH LATIN CAPITAL LETTER G
        0xFF48, 0xFF28, // FULLWIDTH LATIN SMALL LETTER H                                           FULLWIDTH LATIN CAPITAL LETTER H
        0xFF49, 0xFF29, // FULLWIDTH LATIN SMALL LETTER I                                           FULLWIDTH LATIN CAPITAL LETTER I
        0xFF4A, 0xFF2A, // FULLWIDTH LATIN SMALL LETTER J                                           FULLWIDTH LATIN CAPITAL LETTER J
        0xFF4B, 0xFF2B, // FULLWIDTH LATIN SMALL LETTER K                                           FULLWIDTH LATIN CAPITAL LETTER K
        0xFF4C, 0xFF2C, // FULLWIDTH LATIN SMALL LETTER L                                           FULLWIDTH LATIN CAPITAL LETTER L
        0xFF4D, 0xFF2D, // FULLWIDTH LATIN SMALL LETTER M                                           FULLWIDTH LATIN CAPITAL LETTER M
        0xFF4E, 0xFF2E, // FULLWIDTH LATIN SMALL LETTER N                                           FULLWIDTH LATIN CAPITAL LETTER N
        0xFF4F, 0xFF2F, // FULLWIDTH LATIN SMALL LETTER O                                           FULLWIDTH LATIN CAPITAL LETTER O
        0xFF50, 0xFF30, // FULLWIDTH LATIN SMALL LETTER P                                           FULLWIDTH LATIN CAPITAL LETTER P
        0xFF51, 0xFF31, // FULLWIDTH LATIN SMALL LETTER Q                                           FULLWIDTH LATIN CAPITAL LETTER Q
        0xFF52, 0xFF32, // FULLWIDTH LATIN SMALL LETTER R                                           FULLWIDTH LATIN CAPITAL LETTER R
        0xFF53, 0xFF33, // FULLWIDTH LATIN SMALL LETTER S                                           FULLWIDTH LATIN CAPITAL LETTER S
        0xFF54, 0xFF34, // FULLWIDTH LATIN SMALL LETTER T                                           FULLWIDTH LATIN CAPITAL LETTER T
        0xFF55, 0xFF35, // FULLWIDTH LATIN SMALL LETTER U                                           FULLWIDTH LATIN CAPITAL LETTER U
        0xFF56, 0xFF36, // FULLWIDTH LATIN SMALL LETTER V                                           FULLWIDTH LATIN CAPITAL LETTER V
        0xFF57, 0xFF37, // FULLWIDTH LATIN SMALL LETTER W                                           FULLWIDTH LATIN CAPITAL LETTER W
        0xFF58, 0xFF38, // FULLWIDTH LATIN SMALL LETTER X                                           FULLWIDTH LATIN CAPITAL LETTER X
        0xFF59, 0xFF39, // FULLWIDTH LATIN SMALL LETTER Y                                           FULLWIDTH LATIN CAPITAL LETTER Y
        0xFF5A, 0xFF3A, // FULLWIDTH LATIN SMALL LETTER Z                                           FULLWIDTH LATIN CAPITAL LETTER Z
    };

    for (size_t i = 0; i < lower_upper.size(); i += 2) {
        if (c32 < lower_upper[i])
            return c32;
        if (c32 == lower_upper[i])
            return lower_upper[i + 1];
    }
    return c32;
}


// return given string uppercased and with certain punctuation filtered
std::string eliza_uppercase(const std::string & utf8_string)
{
    /*  Make reasonable efforts to convert the user's text into BCD.
        E.g. lowercase 'a' is interpreted as 'A'. Sometimes users
        copy text from documents that contain fancy apostrophes and
        other non-BCD characters. Reinterpret or remove these. */

    std::string result;
    std::u32string utf32(utf8_to_utf32(utf8_string));
    for (auto ch : utf32) {
        const uint32_t c32 = static_cast<uint32_t>(ch);
        switch (c32) {
        case 0x2018:        // 'LEFT SINGLE QUOTATION MARK' (U+2018)
        case 0x2019:        // 'RIGHT SINGLE QUOTATION MARK' (U+2019)
        case 0x0022:        // 'QUOTATION MARK' (U+0022)
        case 0x0060:        // 'GRAVE ACCENT' (U+0060) [backtick]
        case 0x00AB:        // 'LEFT-POINTING DOUBLE ANGLE QUOTATION MARK' (U+00AB)
        case 0x00BB:        // 'RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK' (U+00BB)
        case 0x201A:        // 'SINGLE LOW-9 QUOTATION MARK' (U+201A)
        case 0x201B:        // 'SINGLE HIGH-REVERSED-9 QUOTATION MARK' (U+201B)
        case 0x201C:        // 'LEFT DOUBLE QUOTATION MARK' (U+201C)
        case 0x201D:        // 'RIGHT DOUBLE QUOTATION MARK' (U+201D)
        case 0x201E:        // 'DOUBLE LOW-9 QUOTATION MARK' (U+201E)
        case 0x201F:        // 'DOUBLE HIGH-REVERSED-9 QUOTATION MARK' (U+201F)
        case 0x2039:        // 'SINGLE LEFT-POINTING ANGLE QUOTATION MARK' (U+2039)
        case 0x203A:        // 'SINGLE RIGHT-POINTING ANGLE QUOTATION MARK' (U+203A)
            result += '\''; //   => 'APOSTROPHE' (U+0027)
            break;

        case 0x0021:        // 'EXCLAMATION MARK' (U+0021)
        case 0x003F:        // 'QUESTION MARK' (U+003F)
            result += '.';  //   => 'FULL STOP' (U+002E)
            break;

        default:
            result += utf32_to_utf8(uppercase_utf32(c32));
            break;
        }
    }
    return result;
}


DEF_TEST_FUNC(eliza_uppercase_test)
{
    TEST_EQUAL(eliza_uppercase(""), "");
    TEST_EQUAL(eliza_uppercase("HELLO"), "HELLO");
    TEST_EQUAL(eliza_uppercase("Hello! How are you?"), "HELLO. HOW ARE YOU.");
    TEST_EQUAL(eliza_uppercase("Æmilia, æsop & Phœbë"), "ÆMILIA, ÆSOP & PHŒBË");
    TEST_EQUAL(eliza_uppercase("à â ç é è ê ë î ï ô ù û ü ÿ æ œ"), "À Â Ç É È Ê Ë Î Ï Ô Ù Û Ü Ÿ Æ Œ");
    TEST_EQUAL(eliza_uppercase("Maroš Šefčovič"), "MAROŠ ŠEFČOVIČ");
    TEST_EQUAL(eliza_uppercase("Сайфи Кудаш Гилемдар Зигандарович"), "САЙФИ КУДАШ ГИЛЕМДАР ЗИГАНДАРОВИЧ");
    TEST_EQUAL(eliza_uppercase("¡pónk!"), "¡PÓNK.");

    // 'LEFT SINGLE QUOTATION MARK' (U+2018
    TEST_EQUAL(eliza_uppercase("I‘m depressed"), "I'M DEPRESSED");
    // 'RIGHT SINGLE QUOTATION MARK' (U+2019)
    TEST_EQUAL(eliza_uppercase("I’m depressed"), "I'M DEPRESSED");
    // 'QUOTATION MARK' (U+0022)
    TEST_EQUAL(eliza_uppercase("I'm \"depressed\""), "I'M 'DEPRESSED'");
    // 'GRAVE ACCENT' (U+0060) [backtick]
    TEST_EQUAL(eliza_uppercase("I'm `depressed`"), "I'M 'DEPRESSED'");
    // 'LEFT-POINTING DOUBLE ANGLE QUOTATION MARK' (U+00AB)
    // 'RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK' (U+00BB)
    TEST_EQUAL(eliza_uppercase("I'm «depressed»"), "I'M 'DEPRESSED'");
    // 'SINGLE LOW-9 QUOTATION MARK' (U+201A)
    // 'SINGLE HIGH-REVERSED-9 QUOTATION MARK' (U+201B)
    TEST_EQUAL(eliza_uppercase("I'm ‚depressed‛"), "I'M 'DEPRESSED'");
    // 'LEFT DOUBLE QUOTATION MARK' (U+201C)
    // 'RIGHT DOUBLE QUOTATION MARK' (U+201D)
    TEST_EQUAL(eliza_uppercase("I'm “depressed”"), "I'M 'DEPRESSED'");
    // 'DOUBLE LOW-9 QUOTATION MARK' (U+201E)
    // 'DOUBLE HIGH-REVERSED-9 QUOTATION MARK' (U+201F)
    TEST_EQUAL(eliza_uppercase("I'm „depressed‟"), "I'M 'DEPRESSED'");
    // 'SINGLE LEFT-POINTING ANGLE QUOTATION MARK' (U+2039)
    // 'SINGLE RIGHT-POINTING ANGLE QUOTATION MARK' (U+203A)
    TEST_EQUAL(eliza_uppercase("I'm ‹depressed›"), "I'M 'DEPRESSED'");

    const std::string all_valid_bcd{
        "0123456789=\'+ABCDEFGHI.)-JKLMNOPQR$* /STUVWXYZ,("
    };
    TEST_EQUAL(eliza_uppercase(all_valid_bcd), all_valid_bcd);

    TEST_EQUAL(eliza_uppercase(
        "a"
        "\xCA\x92"),       // 'LATIN SMALL LETTER EZH' (U+0292)
        "A"
        "\xC6\xB7");       // 'LATIN CAPITAL LETTER EZH' (U+01B7)

    TEST_EQUAL(eliza_uppercase(
        "alpha"
        "\xC9\x91"          // 'LATIN SMALL LETTER ALPHA' (U+0251)
        "\xC3\xA7"          // 'LATIN SMALL LETTER C WITH CEDILLA' (U+00E7)
        "\xEF\xBF\xBE"      // not a valid unicode character (U_FFFE)
        "\xEF\xBF\xBF"      // not a valid unicode character (U+FFFF)
        "\xF0\x90\x80\x80"  // 'LINEAR B SYLLABLE B008 A' (U+10000)
        "\xF0\x90\x80\x81"  // 'LINEAR B SYLLABLE B038 E' (U+10001)
        "\xF4\x8F\xBF\xBD"  // Unicode Character '<Plane 16 Private Use, Last>' (U+10FFFD)
        "\xEA\x9E\xB7"      // 'LATIN SMALL LETTER OMEGA' (U+A7B7)
        "omega"),
        "ALPHA"
        "\xC9\x91"          // 'LATIN SMALL LETTER ALPHA' (U+0251) [i.e. not uppercased]
        "\xC3\x87"          // 'LATIN CAPITAL LETTER C WITH CEDILLA' (U+00C7)
        "\xEF\xBF\xBE"      // not a valid unicode character
        "\xEF\xBF\xBF"      // not a valid unicode character
        "\xF0\x90\x80\x80"  // 'LINEAR B SYLLABLE B008 A' (U+10000)
        "\xF0\x90\x80\x81"  // 'LINEAR B SYLLABLE B038 E' (U+10001)
        "\xF4\x8F\xBF\xBD"  // Unicode Character '<Plane 16 Private Use, Last>' (U+10FFFD)
        "\xEA\x9E\xB7"      // 'LATIN SMALL LETTER OMEGA' (U+A7B7) [i.e. not uppercased]
        "OMEGA");

}


// return numeric value of given s or -1
// e.g. to_int("2") -> 2, to_int("two") -> -1
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


DEF_TEST_FUNC(to_int_test)
{
    TEST_EQUAL(to_int("0"), 0);
    TEST_EQUAL(to_int("1"), 1);
    TEST_EQUAL(to_int("2023"), 2023);
    TEST_EQUAL(to_int("-42"), -1);
    TEST_EQUAL(to_int("int"), -1);
}


const std::string tag_six_char_matching_behavior{"USE_SIX_CHAR_MATCHING_BEHAVIOR"};

// e.g. inlist("DEPRESSED", "(*SAD HAPPY DEPRESSED)") -> true
// e.g. inlist("FATHER", "(/FAMILY)") -> true (assuming tags("FAMILY") -> "... FATHER ...")
bool inlist(const std::string & word, std::string wordlist, const tagmap & tags)
{
    assert(!word.empty());

    auto six_char_matching_behavior = [&]() { // return true iff behavior enabled
        const auto t = tags.find(tag_six_char_matching_behavior);
        return t != tags.end() && t->second == stringlist{tag_six_char_matching_behavior};
    };

    if (wordlist.back() == ')')
        wordlist.pop_back();
    const char * cp = wordlist.data();
    if (*cp == '(')
        ++cp;
    while (*cp == ' ')
        ++cp;
    if (*cp == '*') { // (*SAD HAPPY DEPRESSED)
        ++cp;
        const stringlist s{split(std::string(cp))};
        if (six_char_matching_behavior()) { // (see test function for explanation)
            stringlist t;
            for (const auto word6 = word.substr(0, 6); const auto & w : s) {
                for (unsigned i = 0; i < w.size(); i += 6)
                    if (w.substr(i, 6) == word6)
                        return true;
            }
            return false;
        }
        else
            return std::find(s.begin(), s.end(), word) != s.end();
    }
    else if (*cp == '/') { // (/NOUN FAMILY)
        ++cp;
        const auto taglist{split(std::string(cp))};
        for (const auto & tag : taglist) {
            const auto t = tags.find(tag);
            if (t != tags.end()) {
                const stringlist & s{t->second};
                if (std::find(s.begin(), s.end(), word) != s.end())
                    return true;
            }
        }
    }
    return false;
}


DEF_TEST_FUNC(inlist_test)
{
    tagmap tags{ [] {
        tagmap tm;
        tm["FAMILY"] = {"MOTHER", "FATHER", "SISTER", "BROTHER", "WIFE", "CHILDREN"};
        tm["NOUN"] = {"MOTHER", "FATHER", "FISH", "FOUL"};
        return tm;
    }() };

    /* "A decomposition rule may contain a matching constituent of the form
        (/TAG1 TAG2 ...) which will match and isolate a word in the subject
        text having any one of the mentioned tags." [page 41] */

    TEST_EQUAL(inlist("MOTHER",     "(/FAMILY)",        tags), true);
    TEST_EQUAL(inlist("FATHER",     "(/FAMILY)",        tags), true);
    TEST_EQUAL(inlist("SISTER",     "(/FAMILY)",        tags), true);
    TEST_EQUAL(inlist("BROTHER",    "( / FAMILY )",     tags), true);
    TEST_EQUAL(inlist("WIFE",       "(/FAMILY)",        tags), true);
    TEST_EQUAL(inlist("CHILDREN",   "(/FAMILY)",        tags), true);
    TEST_EQUAL(inlist("FISH",       "(/FAMILY)",        tags), false);
    TEST_EQUAL(inlist("FOUL",       "(/FAMILY)",        tags), false);

    TEST_EQUAL(inlist("MOTHER",     "(/NOUN)",          tags), true);
    TEST_EQUAL(inlist("FATHER",     "(/NOUN)",          tags), true);
    TEST_EQUAL(inlist("SISTER",     "(/NOUN)",          tags), false);
    TEST_EQUAL(inlist("BROTHER",    "(/NOUN)",          tags), false);
    TEST_EQUAL(inlist("WIFE",       "(/NOUN)",          tags), false);
    TEST_EQUAL(inlist("CHILDREN",   "(/NOUN)",          tags), false);
    TEST_EQUAL(inlist("FISH",       "(/NOUN)",          tags), true);
    TEST_EQUAL(inlist("FOUL",       "(/NOUN)",          tags), true);

    TEST_EQUAL(inlist("MOTHER",     "(/NOUN FAMILY)",   tags), true);
    TEST_EQUAL(inlist("FATHER",     "(/NOUN FAMILY)",   tags), true);
    TEST_EQUAL(inlist("SISTER",     "(/NOUN FAMILY)",   tags), true);
    TEST_EQUAL(inlist("BROTHER",    "(/NOUN FAMILY)",   tags), true);
    TEST_EQUAL(inlist("WIFE",       "(/NOUN FAMILY)",   tags), true);
    TEST_EQUAL(inlist("CHILDREN",   "(/NOUN FAMILY)",   tags), true);
    TEST_EQUAL(inlist("FISH",       "(/NOUN FAMILY)",   tags), true);
    TEST_EQUAL(inlist("FOUL",       "(/ NOUN  FAMILY )",tags), true);

    TEST_EQUAL(inlist("MOTHER",     "(/NONEXISTANTTAG)",tags), false);
    TEST_EQUAL(inlist("MOTHER",     "(/NON FAMILY TAG)",tags), true);

    TEST_EQUAL(inlist("DEPRESSED",  "(/NOUN FAMILY)",           tags), false);
    TEST_EQUAL(inlist("SAD",        "(*SAD HAPPY DEPRESSED)",   tags), true);
    TEST_EQUAL(inlist("HAPPY",      "(*SAD HAPPY DEPRESSED)",   tags), true);
    TEST_EQUAL(inlist("DEPRESSED",  "(*SAD HAPPY DEPRESSED)",   tags), true);
    TEST_EQUAL(inlist("SAD",        "( * SAD HAPPY DEPRESSED )",tags), true);
    TEST_EQUAL(inlist("HAPPY",      "( * SAD HAPPY DEPRESSED )",tags), true);
    TEST_EQUAL(inlist("DEPRESSED",  "( * SAD HAPPY DEPRESSED )",tags), true);
    TEST_EQUAL(inlist("DRUNK",      "( * SAD HAPPY DEPRESSED )",tags), false);


    /*  In the conversations from the 1965 pilot study by Quarton, McGuire
        and Lorch the following results suggest that there is a bug in
        YMATCH such that the first six characters of the user input is
        compared with each 6-character chunk of the words in the multiple
        choice list.

      - The user input
            SOMETIMES I WONDER WHY I THINK THE WAY I DO
        produces the response
            WHEN YOU FEEL WONDER HOW ARE YOU EFFECTED
        which comes from the rule
            ((0 YOU 0(*HAPPY ELATED EXCITED GOOD WONDERFUL) 0)
                (WHEN YOU FEEL 4 HOW ARE YOU EFFECTED)
        suggesting that WONDER matched WONDER'FUL.

      - The user input
            IN THE PAST I HAVE ACTED UPON ASSUMPTIONS ABOUT OTHER PEOPLE'S FEELING S AND HAVE
                FOUND MYSELF TO BE WRONG. I TRY NOT TO MAKE THOSE HASTY ASSUMPTIONSANYMORE
        produces
            S IS A STRONGWORD , YOU MUST HAVE SOME INSIGHTS INTO WHY YOU FEEL SO STRONGLY
        which comes from
            ((0 YOU 0 (*VICIOUS HOSTILE MEAN ANGRY ENVIOUS FURIOUS BITTER) 0)
                (4 IS A STRONG WORD, YOU MUST HAVE SOME INSIGHTS INTO WHY YOU FEEL SO STRONGLY)
        suggesting that S matched VICIOU'S

      - The user input
            I SUPPOSE THE Y NEVER REALLY HATE THEIR PRAENTS, BUT I CAN' CALL A LAKE  LACK OF
                TRUST THE RELATIONSHIP",
        produces
            TELL ME MORE ABOUT YOUR FEELINGS OF BEING SAD
        which comes from
            ((0 YOU 0 (*SAD ASHAMED DEPRESSED UNHAPPY JURT INJURED SADDENED IRKED) 0)
                (TELL ME MORE ABOUT YOUR FEELINGS OF BEING SAD)
        suggesting that Y matched UNHAPP'Y

      - The user input
            BECAUSE AS MUCH AS I TELL MYSELF THIS WILL NOT HAPPEN I DO IT AGAIN IN AROUND A
                WEEK AFTER I HAVE SAID THAT I WOULDNT PLUS THERE IS MUCH TENSION E WITH SCHOOL
        produces
            E IS A STRONG WORD , YOU MUST HAVE SOME INSIGHTS INTO WHY YOU FEEL SO STRONGLY
        which comes from
            ((0 YOU 0 (*VICIOUS HOSTILE MEAN ANGRY ENVIOUS FURIOUS BITTER) 0)
                (4 IS A STRONG WORD, YOU MUST HAVE SOME INSIGHTS INTO WHY YOU FEEL SO STRONGLY)
        suggesting that E matched HOSTIL'E

      - The user input
            I COULD GO ON TALKING FOR EVER IF YOU D LET ME BUT I ENJOY AND PROGRESS IN
                THE INSIGTS THAT MAYN OTHER PEOPLE MAY HAVE. I THONK IT ADDS TO THE MEANINGULNESS
                OF THE CONVERSATION ... OF COURSE I STARTED OFF WITH THIS BASIC PREMISE THAT WE'RE
                HAVEING A CONVERSATION.... WHICH MAY BE INCORRECT.
        produces
            TELL ME MORE ABOUT YOUR FEELINGS OF BEING SAD
        which comes from
            ((0 YOU 0 (*SAD ASHAMED DEPRESSED UNHAPPY JURT INJURED SADDENED IRKED) 0)
                (TELL ME MORE ABOUT YOUR FEELINGS OF BEING SAD)
        suggesting that D matched ASHAME'D
     */
    TEST_EQUAL(inlist("WONDER",     "(*HAPPY ELATED EXCITED GOOD WONDERFUL)", tags), false);
    TEST_EQUAL(inlist("FUL",        "(*HAPPY ELATED EXCITED GOOD WONDERFUL)", tags), false);
    TEST_EQUAL(inlist("D",          "(*HAPPY ELATED EXCITED GOOD WONDERFUL)", tags), false);

    tags[tag_six_char_matching_behavior] = {tag_six_char_matching_behavior};
    TEST_EQUAL(inlist("WONDER",     "(*HAPPY ELATED EXCITED GOOD WONDERFUL)", tags), true);
    TEST_EQUAL(inlist("FUL",        "(*HAPPY ELATED EXCITED GOOD WONDERFUL)", tags), true);
    TEST_EQUAL(inlist("D",          "(*HAPPY ELATED EXCITED GOOD WONDERFUL)", tags), true);
    TEST_EQUAL(inlist("SAD",        "(*SAD HAPPY DEPRESSED)",   tags), true);
    TEST_EQUAL(inlist("HAPPY",      "(*SAD HAPPY DEPRESSED)",   tags), true);
    TEST_EQUAL(inlist("DEPRESSED",  "(*SAD HAPPY DEPRESSED)",   tags), true);
    TEST_EQUAL(inlist("SAD",        "( * SAD HAPPY DEPRESSED )",tags), true);
    TEST_EQUAL(inlist("HAPPY",      "( * SAD HAPPY DEPRESSED )",tags), true);
    TEST_EQUAL(inlist("DEPRESSED",  "( * SAD HAPPY DEPRESSED )",tags), true);
    TEST_EQUAL(inlist("DRUNK",      "( * SAD HAPPY DEPRESSED )",tags), false);
}


/*  return true iff words match pattern; if they match, matching_components
    are the actual matched words, one for each element of pattern

    e.g. match(tags, [0, YOU, (* WANT NEED), 0], [YOU, NEED, NICE, FOOD], mc) -> true
      with mc = [<empty>, YOU, NEED, NICE FOOD]

    Note that grouped words in pattern, such as (* WANT NEED), must be presented
    as a single stringlist entry. */

#ifdef RECURSIVE_MATCH
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

#elif defined(NON_RECURSIVE_MATCH) // the non-recursive version

bool match(const tagmap & tags, const stringlist & pattern,
        const stringlist & words, stringlist & matching_components)
{
    matching_components.clear();

    struct variable_length_wildcard_record {
        variable_length_wildcard_record(int pattern_index = 0, int words_index = 0, int length = 0)
            : pattern_index(pattern_index), words_index(words_index), length(length)
        {}
        int pattern_index = 0;  // index of this wildcard in pat_array
        int words_index = 0;    // word_array index assigned as the start of this wildcard
        int length = 0;         // number of word_array cells assigned to this wildcard
    };
    std::deque<variable_length_wildcard_record> wildcards;
    std::vector<std::string> pat_array{ pattern.begin(), pattern.end() };
    std::vector<std::string> word_array{ words.begin(), words.end() };

    for (int p = 0, w = 0;;) {

        if (p < pat_array.size()) {
            int n = to_int(pat_array[p]);
            if (n == 0) { // pat_array[p] is a wildcard of any length
                // (assume initially that this wildcard consumes no words)
                wildcards.emplace_back(p++, w, 0);
                continue;
            }
            if (w < word_array.size()) {
                if (n > 0) { // pat_array[p] is a wildcard of specific length
                    ++p;
                    w += n;
                    if (w <= word_array.size())
                        continue;
                }
                else { // pat_array[p] is a literal or a list
                    if (pat_array[p].front() == '(') { // it's a list e.g. "(*SAD HAPPY)"
                        if (inlist(word_array[w], pat_array[p], tags)) {
                            p++;
                            w++;
                            continue;
                        }
                    }
                    else if (pat_array[p] == word_array[w]) { // it's a literal e.g. "ARE"
                        p++;
                        w++;
                        continue;
                    }
                }
            }
        }
        else if (w == word_array.size())
            break;  // simultaneously reached the end of both pattern and words;
                    // all the pattern consumed all the words => match successful

        // backtrack, if possible, and adjust the previous variable-length wildcard length
        for (;;) {
            if (wildcards.empty())
                return false;   // there are no (or no more) wildcards to adjust -
                                // the pattern does not match
            auto & current_wildcard = wildcards.back();
            current_wildcard.length++;
            if (current_wildcard.words_index + current_wildcard.length <= word_array.size())
                break;
            wildcards.pop_back();
        }
        p = wildcards.back().pattern_index + 1;
        w = wildcards.back().words_index + wildcards.back().length;
    }

    // we now know the lengths of all the variable-length wildcards
    // copy all parts to matching_components
    for (int p = 0, w = 0; p < pat_array.size(); ++p) {
        int part_length = 1;
        int n = to_int(pat_array[p]);
        if (n == 0) {
            assert(!wildcards.empty());
            assert(wildcards.front().words_index == w);
            part_length = wildcards.front().length;
            wildcards.pop_front();
        }
        else if (n > 1)
            part_length = n;
        assert(w + part_length <= word_array.size());

        stringlist part;
        for (int i = 0; i < part_length; ++i)
            part.emplace_back(word_array[w++]);
        matching_components.emplace_back(join(part));
    }    
    assert(wildcards.empty());

    return true;
}

#else // implementation similar to the SLIP YMATCH code

using vecstr = std::vector<std::string>;

/*  Match pat_array[p_begin..p_end) to word_array[w_begin...).
    These things must be true
      - the pattern segment may contain at most one 0-wildcard
      - if the segment contains a 0-wildcard it must be the first element
      - the segment must either be the last in the whole pattern or
        be followed by another segment that begins with a 0-wildcard
*/
bool xmatch(            // return true iff words matched pattern
    const tagmap & tags,
    const vecstr pat_array,
    const vecstr word_array,
    const int p_begin,  // index into pat_array where match pattern begins
    const int p_end,    // index into pat_array just after match pattern ends
    const int w_begin,  // index into word_array where pattern must begin matching
    const int fixed_len,// total number of words required to match non-0-wildcard part
    int & w_end,        // out: index into word_array just after pattern matching ended
    vecstr & result)    // out: matches will be written to result at [p_begin..p_end)
{
    if (word_array.size() - w_begin < fixed_len)
        return false;   // there are insufficient words to match the pattern

    int wildcard_len = 0;
    int wildcard_end = 0;

    const bool has_wildcard = to_int(pat_array[p_begin]) == 0;
    if (has_wildcard) {
        if (p_end == pat_array.size()) {
            // this is the last segment of the whole pattern: it must match
            // right up to the very last word
            wildcard_len = word_array.size() - w_begin - fixed_len;
            // if it doesn't match at the end, it doesn't match at all
            wildcard_end = wildcard_len;
        }
        else {
            // this is not the last segment of the whole pattern: it must
            // consume the smallest number of words possible
            wildcard_len = 0;
            // work forwards from the minimum wildcard length to the maximum possible
            wildcard_end = word_array.size() - w_begin - fixed_len;
        }
    }

    // loop until a match is found or all possible wildcard_len values have been tried
    for (;; ++wildcard_len) {
        int p = p_begin + has_wildcard;
        int w = w_begin + wildcard_len;

        // loop to match pattern at p to words at w
        // on exit, p == p_end implies success
        for (; p < p_end; ++p) {
            const int n = to_int(pat_array[p]);
            assert(n != 0);
            if (n > 0) { // pat_array[p] is a wildcard of specific length
                assert(w + n <= word_array.size());
                stringlist part;
                for (int i = 0; i < n; ++i)
                    part.emplace_back(word_array[w++]);
                result[p] = join(part);
            }
            else { // pat_array[p] is a literal or a list
                assert(w < word_array.size());
                if (pat_array[p].front() == '(') { // it's a list e.g. "(*SAD HAPPY)"
                    if (inlist(word_array[w], pat_array[p], tags))
                        result[p] = word_array[w++];
                    else
                        break;
                }
                else if (pat_array[p] == word_array[w]) // it's a literal e.g. "ARE"
                    result[p] = word_array[w++];
                else
                    break;
            }
        }
        if (p == p_end) {
            w_end = w;
            if (has_wildcard) {
                stringlist part;
                for (int i = 0; i < wildcard_len; ++i)
                    part.emplace_back(word_array[w_begin + i]);
                result[p_begin] = join(part);
            }
            return true;
        }
        if (wildcard_len == wildcard_end)
            break;
    }

    return false;
}


bool match(const tagmap & tags, const stringlist & pattern,
        const stringlist & words, stringlist & matching_components)
{
    matching_components.clear();

    std::vector<std::string> pat_array{ pattern.begin(), pattern.end() };
    std::vector<std::string> word_array{ words.begin(), words.end() };
    std::vector<std::string> matches(pat_array.size());

    int w = 0;
    for (int p_seg_end = 0; p_seg_end < pat_array.size(); ) {
        
        /*  locate the right boundary of the next anchor segment (an anchor
            segment extends from the end of the previous segment, or from the
            first element if this is the first segment, up to the end of the
            pattern, or the next 0-wildcard, whichever comes first) */
        int fixed_len = 0;
        int p = p_seg_end;
        for (; p_seg_end < pat_array.size(); ++p_seg_end) {
            const int n =  to_int(pat_array[p_seg_end]);
            if (n == 0) {           // element is a 0-wildcard
                if (p_seg_end > p)
                    break;          // this 0-wildcard isn't the first element
            }
            else if (n > 0)
                fixed_len += n;     // element is a fixed length wildcard, e.g. 3
            else
                ++fixed_len;        // element is a literal or (*...) or (/...)
        }

        /*  The current pattern segment is the half-open interval [p, p_seg_end).
            The segment always contains fixed size elements, unless there are
            no fixed size elements before the next 0-wildcard or the end of the
            pattern. If it contains a 0-wildcard that will be the first element.
            Following the wildcard (if present) will be only fixed sized elements
            (if any). I.e. the segment will have one of these three forms
                 (0)
                 (0 1 2 LITERAL (*LITERAL LITERAL) (/TAG TAG)), for example
                 (  1 2 LITERAL (*LITERAL LITERAL) (/TAG TAG)), for example
            Following this segment will either be a 0-wildcard or the end of the
            pattern. This segment must match the words at w. If the segment begins
            with a wildcard, then
              - if what follows this segment is a 0-wildcard, this segment must
                consume the smallest number of words possible
              - if what follows this segment is the pattern end, this segment must
                consume all the remaining words
            If the segment does not begin with a 0-wildcard it must consume the
            exact number of words described by the pattern elements. */

        if (!xmatch(tags, pat_array, word_array, p, p_seg_end, w, fixed_len, w, matches))
            return false;   // this segment didn't match the words
    }
    if (w < word_array.size())
        return false;       // the pattern did not consume all words

    for (auto & m : matches)
        matching_components.emplace_back(std::move(m));

    return true;
}

#endif


DEF_TEST_FUNC(match_test)
{
    stringlist words{ "HELLO" };
    stringlist pattern{ "HELLO" };
    stringlist expected{ "HELLO" };
    stringlist matching_components;
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "HELLO", "WORLD" };
    pattern = { "HELLO", "WORLD" };
    expected = { "HELLO", "WORLD" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "HELLO", "WORLD" };
    pattern = { "HELLO", "1" };
    expected = { "HELLO", "WORLD" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "HELLO", "WORLD" };
    pattern = { "1", "WORLD" };
    expected = { "HELLO", "WORLD" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "HELLO", "WORLD" };
    pattern = { "1", "1" };
    expected = { "HELLO", "WORLD" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "HELLO", "WORLD" };
    pattern = { "1" };
    expected = { };
    TEST_EQUAL(match({}, pattern, words, matching_components), false);
    TEST_EQUAL(matching_components, expected);

    words = { "HELLO", "WORLD" };
    pattern = { "2" };
    expected = { "HELLO WORLD" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "HELLO", "WORLD" };
    pattern = { "3" };
    expected = { };
    TEST_EQUAL(match({}, pattern, words, matching_components), false);
    TEST_EQUAL(matching_components, expected);

    words = { "HELLO", "WORLD" };
    pattern = { "0" };
    expected = { "HELLO WORLD" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "HELLO", "WORLD" };
    pattern = { "0", "1" };
    expected = { "HELLO", "WORLD" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "HELLO", "WORLD" };
    pattern = { "1", "0" };
    expected = { "HELLO", "WORLD" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "HELLO", "WORLD" };
    pattern = { "0", "0" };
    expected = { "", "HELLO WORLD" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "HELLO", "WORLD" };
    pattern = { "0", "WORLD" };
    expected = { "HELLO", "WORLD" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "HELLO", "WORLD" };
    pattern = { "HELLO", "0" };
    expected = { "HELLO", "WORLD" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "HELLO", "WORLD" };
    pattern = { "0", "HELLO", "WORLD" };
    expected = { "", "HELLO", "WORLD" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "HELLO", "WORLD" };
    pattern = { "HELLO", "WORLD", "0" };
    expected = { "HELLO", "WORLD", "" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "HELLO", "WORLD" };
    pattern = { "0", "0", "1" };
    expected = { "", "HELLO", "WORLD" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "HELLO", "WORLD" };
    pattern = { "0", "0", "0" };
    expected = { "", "", "HELLO WORLD" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "HELLO", "WORLD" };
    pattern = { "HELLO", "0", "0" };
    expected = { "HELLO", "", "WORLD" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "HELLO", "WORLD" };
    pattern = { "0", "HELLO", "0" };
    expected = { "", "HELLO", "WORLD" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "HELLO", "WORLD" };
    pattern = { "0", "WORLD", "0" };
    expected = { "HELLO", "WORLD", "" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "HELLO", "WORLD" };
    pattern = { "0", "0", "WORLD" };
    expected = { "", "HELLO", "WORLD" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "HELLO", "WORLD" };
    pattern = { "0", "0", "1" };
    expected = { "", "HELLO", "WORLD" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "HELLO", "WORLD" };
    pattern = { "HELLO", "0", "0", "WORLD" };
    expected = { "HELLO", "", "", "WORLD" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "'ELLO", "'ELLO" };
    pattern = { "0", "'ELLO" };
    expected = { "'ELLO", "'ELLO" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "'ELLO", "'ELLO" };
    pattern = { "0", "'ELLO", "0" };
    expected = { "", "'ELLO", "'ELLO" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    // test [0, YOU, (*WANT NEED), 0] matches [YOU, NEED, NICE, FOOD]
    words = { "YOU", "NEED", "NICE", "FOOD" };
    pattern = { "0", "YOU", "(*WANT NEED)", "0" };
    expected = { "", "YOU", "NEED", "NICE FOOD" };
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
    expected = { };
    TEST_EQUAL(match({}, pattern, words, matching_components), false);
    TEST_EQUAL(matching_components, expected);

    // test [1, (*WANT NEED), 2] matches [YOU, WANT, NICE, FOOD]
    words = { "YOU", "WANT", "NICE", "FOOD" };
    pattern = { "1", "(*WANT NEED)", "2" };
    expected = { "YOU", "WANT", "NICE FOOD" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    // test (0 YOUR 0 (* FATHER MOTHER) 0) matches
    // (CONSIDER YOUR AGED MOTHER AND FATHER TOO)
    /* "The above input text would have been decomposed precisely as stated
        above by the decomposition rule: (0 YOUR 0 (*FATHER MOTHER) 0) which,
        by virtue of the presence of "*" in the sublist structure seen above,
        would have isolated either the word "FATHER" or "MOTHER" (in that
        order) in the input text, whichever occurred first after the first
        appearance of the word "YOUR". -- Weizenbaum 1966, page 42
       What does "in that order" mean? */
    words = { "CONSIDER", "YOUR", "AGED", "MOTHER", "AND", "FATHER", "TOO" };
    pattern = { "0", "YOUR", "0", "(* FATHER MOTHER)", "0" };
    expected = { "CONSIDER", "YOUR", "AGED", "MOTHER", "AND FATHER TOO" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "MOTHER", "AND", "FATHER", "MOTHER" };
    pattern = { "0", "(* FATHER MOTHER)", "(* FATHER MOTHER)", "0" };
    expected = { "MOTHER AND", "FATHER", "MOTHER", "" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    // patterns don't require literals
    words = { "FIRST", "AND", "LAST", "TWO", "WORDS" };
    pattern = { "2", "0", "2" };
    expected = { "FIRST AND", "LAST", "TWO WORDS" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    // pointless but not prohibited
    words = { "THE", "NAME", "IS", "BOND", "JAMES", "BOND", "OR", "007", "IF", "YOU", "PREFER"};
    pattern = { "0", "0", "7" };
    expected = { "", "THE NAME IS BOND", "JAMES BOND OR 007 IF YOU PREFER" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    // how are ambiguous matches resolved?
    words = { "ITS", "MARY", "ITS", "NOT", "MARY", "IT", "IS", "MARY", "TOO" };
    pattern = { "0", "ITS", "0", "MARY", "1" };
    expected = { "", "ITS", "MARY ITS NOT MARY IT IS", "MARY", "TOO" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    // how are ambiguous matches resolved? ("I know that you know I hate you and I like you too") 
    words = { "YOU", "KNOW", "THAT", "I", "KNOW", "YOU", "HATE", "I", "AND", "YOU", "LIKE", "I", "TOO"};
    pattern = { "0", "YOU", "0", "I", "0" }; // from the I rule in the DOCTOR script
    expected = { "", "YOU", "KNOW THAT", "I", "KNOW YOU HATE I AND YOU LIKE I TOO" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    // A test pattern from the YMATCH function description in the SLIP manual
    words = { "MARY", "HAD", "A", "LITTLE", "LAMB", "ITS", "PROBABILITY", "WAS", "ZERO" };
    pattern = { "MARY", "2", "2", "ITS", "1", "0" };
    expected = { "MARY", "HAD A", "LITTLE LAMB", "ITS", "PROBABILITY", "WAS ZERO" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    /* There is an example in the SLIP manual:
       "X:RULE.(L1, L2, L3)
        This function combines the functions YMATCH and ASMBL as follows: the list L2
        is operated upon according to the specifications of L1 and produces the resulting
        list L3.
        The list L1 consists of two segments separated by an equal sign. The first segment
        specifies a rule for segmenting L2 and is composed according to the rules for the
        first argument of YMATCH. The second segment specifies a rule for assembling these
        segments into the list L3, and is composed according to the rules for the first
        argument of ASMBL.
        Example: RULE: (RUL, INPUT, OUTPUT). If INPUT is the sentence
            (MARY HAD A LITTLE LAMB ITS PROBABILITY WAS ZERO)
        and RUL is
            (1 0 2 ITS 0 = DID 1 HAVE A 3)
        then OUTPUT is constructed as follows:
            (DID MARY HAVE A LITTLE LAMB)"
        <https://www.google.co.uk/books/edition/University_of_Michigan_Executive_System/f7oSAQAAMAAJ>
    */
    // A test pattern from the RULE function description in the SLIP manual
    words = { "MARY", "HAD", "A", "LITTLE", "LAMB", "ITS", "PROBABILITY", "WAS", "ZERO" };
    pattern = { "1", "0", "2", "ITS", "0" };
    expected = { "MARY", "HAD A", "LITTLE LAMB", "ITS", "PROBABILITY WAS ZERO" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);
    // My understanding of the SLIP code is that the "ITS" term matches the first ITS in the input, not the second:
    words = { "MARY", "HAD", "A", "LITTLE", "LAMB", "ITS", "PROBABILITY", "AND", "ITS", "LIKELYHOOD", "WERE", "ZERO" };
    pattern = { "1", "0", "2", "ITS", "0" };
    expected = { "MARY", "HAD A", "LITTLE LAMB", "ITS", "PROBABILITY AND ITS LIKELYHOOD WERE ZERO" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { };
    pattern = { };
    expected = { };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { };
    pattern = { "0" };
    expected = { "" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "MARY", "HAD", "A", "LITTLE", "LAMB" };
    pattern = { "MARY", "HAD", "A", "LITTLE", "LAMB" };
    expected = { "MARY", "HAD", "A", "LITTLE", "LAMB" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "HAD", "MARY", "A", "LITTLE", "LAMB" };
    pattern = { "MARY", "HAD", "A", "LITTLE", "LAMB" };
    expected = { };
    TEST_EQUAL(match({}, pattern, words, matching_components), false);
    TEST_EQUAL(matching_components, expected);

    words = { "MARY", "HAD", "A", "LAMB" };
    pattern = { "MARY", "HAD", "A", "LITTLE", "LAMB" };
    expected = { };
    TEST_EQUAL(match({}, pattern, words, matching_components), false);
    TEST_EQUAL(matching_components, expected);

    words = { "MARY", "HAD", "A", "LITTLE", "LAMB", "CALLED", "WOOLY" };
    pattern = { "MARY", "HAD", "A", "LITTLE", "LAMB" };
    expected = { };
    TEST_EQUAL(match({}, pattern, words, matching_components), false);
    TEST_EQUAL(matching_components, expected);

    words = { "WHEN", "WILL", "WE", "THREE", "MEET" };
    pattern = { "WHEN", "WILL", "2", "MEET" };
    expected = { "WHEN", "WILL", "WE THREE", "MEET" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "WHEN", "WILL", "WE", "THREE", "MEET" };
    pattern = { "WHEN", "1", "2", "MEET" };
    expected = { "WHEN", "WILL", "WE THREE", "MEET" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "WHEN", "WILL", "WE", "THREE", "MEET" };
    pattern = { "1", "1", "2", "1" };
    expected = { "WHEN", "WILL", "WE THREE", "MEET" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "WHEN", "WILL", "WE", "THREE", "MEET" };
    pattern = { "3", "2" };
    expected = { "WHEN WILL WE", "THREE MEET" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "WHEN", "WILL", "WE", "THREE", "MEET" };
    pattern = { "3", "0" };
    expected = { "WHEN WILL WE", "THREE MEET" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "WHEN", "WILL", "WE", "THREE", "MEET" };
    pattern = { "5" };
    expected = { "WHEN WILL WE THREE MEET" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "WHEN", "WILL", "WE", "THREE", "MEET" };
    pattern = { "0", "5" };
    expected = { "", "WHEN WILL WE THREE MEET" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "WHEN", "WILL", "WE", "THREE", "MEET" };
    pattern = { "5", "0" };
    expected = { "WHEN WILL WE THREE MEET", "" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "WHEN", "WILL", "WE", "THREE", "MEET" };
    pattern = { "1", "0" };
    expected = { "WHEN", "WILL WE THREE MEET" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "WHEN", "WILL", "WE", "THREE", "MEET" };
    pattern = { "0", "1" };
    expected = { "WHEN WILL WE THREE", "MEET" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "WHEN", "WILL", "WE", "THREE", "MEET" };
    pattern = { "1", "1", "0" };
    expected = { "WHEN", "WILL", "WE THREE MEET" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "WHEN", "WILL", "WE", "THREE", "MEET" };
    pattern = { "0", "1", "0" };
    expected = { "", "WHEN", "WILL WE THREE MEET" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "WHEN", "WILL", "WE", "THREE", "MEET" };
    pattern = { "0", "1", "0", "1" };
    expected = { "", "WHEN", "WILL WE THREE", "MEET" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "WHEN", "WILL", "WE", "THREE", "MEET" };
    pattern = { "0", "1", "0", "1", "0" };
    expected = { "", "WHEN", "", "WILL", "WE THREE MEET" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "WHEN", "WILL", "WE", "THREE", "MEET" };
    pattern = { "0", "1", "0", "1", "0", "1" };
    expected = { "", "WHEN", "", "WILL", "WE THREE", "MEET" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "WHEN", "WILL", "WE", "THREE", "MEET" };
    pattern = { "0", "1", "0", "1", "0", "1", "0" };
    expected = { "", "WHEN", "", "WILL", "", "WE", "THREE MEET" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "WHEN", "WILL", "WE", "THREE", "MEET" };
    pattern = { "0", "1", "0", "1", "0", "1", "0", "1" };
    expected = { "", "WHEN", "", "WILL", "", "WE", "THREE", "MEET" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "WHEN", "WILL", "WE", "THREE", "MEET" };
    pattern = { "0", "1", "0", "1", "0", "1", "0", "1", "0" };
    expected = { "", "WHEN", "", "WILL", "", "WE", "", "THREE", "MEET" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "WHEN", "WILL", "WE", "THREE", "MEET" };
    pattern = { "0", "1", "0", "1", "0", "1", "0", "1", "0", "1" };
    expected = { "", "WHEN", "", "WILL", "", "WE", "", "THREE", "", "MEET" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "WHEN", "WILL", "WE", "THREE", "MEET" };
    pattern = { "0", "1", "0", "1", "0", "1", "0", "1", "0", "1", "0" };
    expected = { "", "WHEN", "", "WILL", "", "WE", "", "THREE", "", "MEET", "" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "WHEN", "WILL", "WE", "THREE", "MEET" };
    pattern = { "0", "1", "0", "1", "0", "1", "0", "1", "0", "1", "0", "1" };
    expected = { };
    TEST_EQUAL(match({}, pattern, words, matching_components), false);
    TEST_EQUAL(matching_components, expected);

    words = { "WHEN", "WILL", "WE", "THREE", "MEET" };
    pattern = { "6" };
    expected = { };
    TEST_EQUAL(match({}, pattern, words, matching_components), false);
    TEST_EQUAL(matching_components, expected);

    words = { "WHEN", "WILL", "WE", "THREE", "MEET" };
    pattern = { "0", "6" };
    expected = { };
    TEST_EQUAL(match({}, pattern, words, matching_components), false);
    TEST_EQUAL(matching_components, expected);

    words = { "WHEN", "WILL", "WE", "THREE", "MEET" };
    pattern = { "6", "0" };
    expected = { };
    TEST_EQUAL(match({}, pattern, words, matching_components), false);
    TEST_EQUAL(matching_components, expected);

    words = { "WHEN", "WILL", "WE", "THREE", "MEET" };
    pattern = { "1", "WHEN", "0" };
    expected = { };
    TEST_EQUAL(match({}, pattern, words, matching_components), false);
    TEST_EQUAL(matching_components, expected);

    words =   { "IT'S", "MY", "EASTEREGG", "YUM" };
    pattern = { "1",    "1",  "1",         "0",  "1" };
    expected = { "IT'S", "MY", "EASTEREGG", "", "YUM" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words =   { "IT'S", "MY", "EASTEREGG", "YUM" };
    pattern = { "IT'S", "1",  "1",         "0",  "1" };
    expected = { "IT'S", "MY", "EASTEREGG", "", "YUM" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words =   { "IT'S", "MY", "EASTEREGG", "YUM" };
    pattern = { "1",    "MY", "1",         "0",  "1" };
    expected = { "IT'S", "MY", "EASTEREGG", "", "YUM" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words =   { "IT'S", "MY", "EASTEREGG", "YUM" };
    pattern = { "1",    "1",  "EASTEREGG", "0",  "1" };
    expected = { "IT'S", "MY", "EASTEREGG", "", "YUM" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words =   { "IT'S", "MY", "EASTEREGG", "YUM" };
    pattern = { "1",    "MY", "EASTEREGG", "0",  "1" };
    expected = { "IT'S", "MY", "EASTEREGG", "", "YUM" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words =   { "IT'S", "MY", "EASTEREGG", "YUM" };
    pattern = { "IT'S", "1",  "EASTEREGG", "0",  "1" };
    expected = { "IT'S", "MY", "EASTEREGG", "", "YUM" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words =   { "IT'S", "MY", "EASTEREGG", "YUM" };
    pattern = { "IT'S", "MY", "EASTEREGG", "0",  "1" };
    expected = { "IT'S", "MY", "EASTEREGG", "", "YUM" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words =   { "IT'S", "MY", "EASTEREGG", "YUM" };
    pattern = { "IT'S", "MY", "EASTEREGG", "YUM" };
    expected = { "IT'S", "MY", "EASTEREGG", "YUM" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words =   { "X", "X", "A", "X", "X", "A" };
    pattern = { "0", "A", "0", "A" };
    expected = { "X X", "A", "X X", "A" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words =   { "X", "X", "A", "X", "X", "A", "X", "X", "A" };
    pattern = { "0", "A", "0", "A" };
    expected = { "X X", "A", "X X A X X", "A" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words =   { "X", "X", "A", "X", "X", "A", "X", "X", "A" };
    pattern = { "0", "A", "0", "A", "0" };
    expected = { "X X", "A", "X X", "A", "X X A" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "MY", "FAIR", "LADY" };
    pattern = { "MY", "(*FAIR GOOD)", "LADY" };
    expected = { "MY", "FAIR", "LADY" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "MY", "GOOD", "LADY" };
    pattern = { "MY", "(*FAIR GOOD)", "LADY" };
    expected = { "MY", "GOOD", "LADY" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "MY", "LADY" };
    pattern = { "MY", "(*FAIR GOOD)", "LADY" };
    expected = { };
    TEST_EQUAL(match({}, pattern, words, matching_components), false);
    TEST_EQUAL(matching_components, expected);

    words = { "MY", "FAIR", "GOOD", "LADY" };
    pattern = { "MY", "(*FAIR GOOD)", "LADY" };
    expected = { };
    TEST_EQUAL(match({}, pattern, words, matching_components), false);
    TEST_EQUAL(matching_components, expected);

    const tagmap tags{ [] {
        tagmap tm;
        tm["FAMILY"] = {"MOTHER", "FATHER"};
        tm["NOUN"] = {"MOTHER", "FATHER"};
        tm["BELIEF"] = {"FEEL"};
        return tm;
    }() };

    words = { "MY", "MOTHER", "LOVES", "ME" };
    pattern = { "0", "(/FAMILY)", "0" };
    expected = { "MY", "MOTHER", "LOVES ME" };
    TEST_EQUAL(match(tags, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "MY", "MOTHER", "LOVES", "ME" };
    pattern = { "0", "(/NOUN)", "0" };
    expected = { "MY", "MOTHER", "LOVES ME" };
    TEST_EQUAL(match(tags, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "MY", "MOTHER", "LOVES", "ME" };
    pattern = { "0", "(/BELIEF FAMILY)", "0" };
    expected = { "MY", "MOTHER", "LOVES ME" };
    TEST_EQUAL(match(tags, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);

    words = { "MY", "MOTHER", "LOVES", "ME" };
    pattern = { "0", "(/BELIEF)", "0" };
    expected = { };
    TEST_EQUAL(match(tags, pattern, words, matching_components), false);
    TEST_EQUAL(matching_components, expected);
}


// return words constructed from given reassembly_rule and components
// e.g. reassemble([ARE, YOU, 1], [MAD, ABOUT YOU]) -> [ARE, YOU, MAD]
stringlist reassemble(const stringlist & reassembly_rule, const stringlist & components)
{
    stringlist result;
    for (const auto & r : reassembly_rule) {
        int n = to_int(r);
        if (n < 0)
            result.push_back(r);
        else if (n == 0 || static_cast<size_t>(n) > components.size()) {
            // index out of range should never happen because indexes
            // are checked when the script is processed
            result.emplace_back("HMMM");
        }
        else {
            const stringlist expanded{split(components[n - 1])};
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

    reassembly_rule = {"1", "1", "1"};
    expected = {"MARY", "MARY", "MARY"};
    TEST_EQUAL(reassemble(reassembly_rule, matching_components), expected);

    reassembly_rule = {"1", "-1", "1"};
    expected = {"MARY", "-1", "MARY"};
    TEST_EQUAL(reassemble(reassembly_rule, matching_components), expected);

    reassembly_rule = {"1", "0", "1"};
    expected = {"MARY", "HMMM", "MARY"};
    TEST_EQUAL(reassemble(reassembly_rule, matching_components), expected);

    reassembly_rule = {"1", "6", "1"};
    expected = {"MARY", "WAS", "ZERO", "MARY"};
    TEST_EQUAL(reassemble(reassembly_rule, matching_components), expected);

    reassembly_rule = {"1", "7", "1"};
    expected = {"MARY", "HMMM", "MARY"};
    TEST_EQUAL(reassemble(reassembly_rule, matching_components), expected);
}


bool reassembly_indexes_valid(
    const stringlist & decomposition_rule,
    const stringlist & reassembly_rule,
    std::string & index_out_of_range_msg)
{
    index_out_of_range_msg.clear();
    const size_t last_dissassembly_part_index = decomposition_rule.size();
    for (const auto & r : reassembly_rule) {
        int n = to_int(r);
        if (n < 0)
            continue; // it's not an index
        if (n == 0 || static_cast<size_t>(n) > last_dissassembly_part_index) {
            index_out_of_range_msg = std::string("reassembly index '")
                + std::to_string(n)
                + "' out of range [1.."
                + std::to_string(last_dissassembly_part_index)
                + "]";
            return false;
        }
    }
    return true;
}


/*  last_chunk_as_bcd() -- What the heck?

    Very quick overview:

    ELIZA was written in SLIP for an IBM 7094. The character encoding used
    on the 7094 is called Hollerith (or BCD - see the hollerith_encoding
    table above). The Hollerith encoding uses 6 bits per character.
    The IBM 7094 machine word size is 36-bits.

    SLIP stores strings in SLIP cells. A SLIP cell consists of two
    adjacent machine words. The first word contains some type bits
    and two addresses, one pointing to the previous SLIP cell and
    the other pointing to the next SLIP cell. (The IBM 7094 had a
    32,768 word core store, so only 15 bits are required for an
    address. So two addresses fit into one 36-bit word with 6 bits
    spare.) The second word may carry the "datum." This is where
    the characters are stored.
    
    Each SLIP cell can store up to 6 6-bit Hollerith characters.

    If a string has fewer than 6 characters, the string is stored left-
    justified and space padded to the right.

    So for example, the string "HERE" would be stored in one SLIP cell,
    which would have the octal value 30 25 51 25 60 60.

    If a string has more than 6 characters, it is stored in successive
    SLIP cells. Each cell except the last has the sign bit set in the
    first word to indicated the string is continued in the next cell.

    So the word "INVENTED" would be stored in two SLIP cells, "INVENT"
    in the first and "ED    " in the second.

    In ELIZA, the user's input text is read into a SLIP list, each word
    in the sentence is in it's own cell, unless a word needs to be
    continued in the next cell because it's more than 6 characters long.

    When ELIZA chooses a MEMORY rule it hashes the last cell in the
    input sentence. That will be the last word in the sentence, or
    the last chunk of the last word, if the last word is more than
    6 characters long.

    This code doesn't use SLIP cells. A std::deque of std::string
    provided enough functionality to manage without SLIP. In this
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
        if (hollerith_defined(c)) {
            /*  As spelled out above, to accurately reproduce the original
                ELIZA conversation we need to use the Hollerith encoding
                for the character. We can only do that if the character
                exists in the Hollerith character set. In this case it does. */
            result |= hollerith_encoding[static_cast<unsigned char>(c)];
        }
        else {
            /*  Since this character is not in the Hollerith character set
                we are obviously not trying to reproduce an original ELIZA
                conversation. So the encoding we use doesn't matter. Just
                use the least-significant six bits of the character. */
            result |= static_cast<unsigned char>(c & 0x3F);
        }
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
                                                //  X _ _ _ _ _
    TEST_EQUAL(last_chunk_as_bcd("X"),            0676060606060ull);
                                                //  H E R E _ _
    TEST_EQUAL(last_chunk_as_bcd("HERE"),         0302551256060ull);
                                                //  A L W A Y S
    TEST_EQUAL(last_chunk_as_bcd("ALWAYS"),       0214366217062ull);
                                                //  E D _ _ _ _
    TEST_EQUAL(last_chunk_as_bcd("INVENTED"),     0252460606060ull);
                                                //  A B C D E F
    TEST_EQUAL(last_chunk_as_bcd("123456ABCDEF"), 0212223242526ull);

    // 'LATIN CAPITAL LETTER C WITH CEDILLA' (U+00C7) as UTF-8
    TEST_EQUAL(last_chunk_as_bcd("\xC3\x87"),     0030760606060ull);
}


/*
    "N1 : HASH.(D,N2)

    This function classifies D according to a pseudo-random scheme. The number
    of classes is 2^N2 where N2 is the second argument of the function. The
    value of the function is a pseudo-random number N1 in the range of
    0 to 2^N2-1."
    -- Documentation from University of Michigan Executive System for the
       IBM 7090, SLIP section, page 30


    The FORTRAN Assembly Program implementation of SLIP HASH from JW's
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

    Bits in an IBM 7090 instruction word are labelled
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

// Recreate the SLIP HASH function: return an n-bit hash value, for n in [0..15],
// for the given 36-bit datum d. (Uses the von Neumann middle square method.)
int hash(uint_least64_t d, int n)
{
    /*  This code implements the SLIP HASH algorithm from the FAP
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
        truncated to 64-bits. (In C++, unsigned arithmetic overflow is
        not undefined behaviour, as it is for signed arithmetic.) If
        n is 15, the middle 15 bits of a 70-bit number are bits 42-28
        (bit 0 least significant), which is well within our 64-bit
        calculation. */
    assert(0 <= n && n <= 15);

    d &= 0x7FFFFFFFFULL;        // clear all but least significant 35 bits
    d *= d;                     // square it (any overflow may be safely ignored)
    d >>= 35 - n / 2;           // shift middle n bits to least significant bits
    return d & (1ULL << n) - 1; // clear all but least significant n bits
}

DEF_TEST_FUNC(hash_test)
{
    // The four real-world test cases

    /* 1. Weizenbaum stated this on page 38 of the 1966 CACM paper
    
            "As a particular key list structure is read the keyword K at its
            top is randomized (hashed) by a procedure that produces (currently)
            a 7 bit integer "i". The word "always", for example, yields the
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
        least significant N bits (7 bits)        0xE
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

        In the ELIZA code, these lines choose the MEMORY rule

            OR W'R KEYWRD .E. MEMORY                                     001220
             I=HASH.(BOT.(INPUT),2)+1                                    001230
             NEWBOT.(REGEL.(MYTRAN(I),INPUT,LIST.(MINE)),MYLIST)         001240

        That's the HASH of the BOT (last cell) of the user's INPUT sentence,
        with a 2-bit result. MYTRAN (containing the 4 MEMORY rules) is indexed
        on the value returned by HASH plus 1.

        Later, ELIZA says

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
        recalled for ELIZA's responses

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
        least significant N bits (2 bits)        0
    */
    TEST_EQUAL(hash(0423124626060ull, 2), 1ull);
    TEST_EQUAL(hash(0633144256060ull, 2), 0ull);


    // Real-world test cases from Rupert Lane's CTSS/7094 emulator running the
    // original SLIP hash function.
    {
        // These test values were obtained by running ELIZA in the emulator,
        // selecting to play .TAPE. 100, typing + to enter the CHANGE mode,
        // then typing DISPLA to dump the keywords and noting the bucket index.
        struct test_candidate {
            const char * word;
            int expected_hash_value;
        } tape_100[] = {
            { "NOONE",       0 },
            { "WIFE",        1 },
            { "I",           2 },
            { "CAN",         2 },
            { "BECAUSE",     3 },
            { "IF",          5 },
            { "CHILDREN",    5 },
            { "HOW",         6 },
            { "YES",         6 },
            { "ALWAYS",      7 },
            { "MY",          8 },
            { "YOU'RE",     11 },
            { "ARE",        12 },
            { "EVERYONE",   12 },
            { "MAYBE",      13 },
            { "YOU",        13 },
            { "AM",         16 },
            { "YOUR",       17 },
            { "PERHAPS",    18 },
            { "MYSELF",     19 },
            { "BROTHER",    21 },
            { "WHAT",       21 },
            { "MOTHER",     22 },
            { "SISTER",     22 },
            { "NO",         24 },
            { "I'M",        25 },
            { "WHY",        27 },
            { "NOBODY",     27 },
            { "FATHER",     28 },
            { "WHEN",       29 },
            { "WAS",        29 },
            { "ME",         29 },
            { "YOURSELF",   30 },
            { "WERE",       31 },
            { "EVERYBODY",  31 }
        };

        // for each test case...
        for (const auto [word, expected_hash_value] : tape_100) {
            uint_least64_t bcd_encoded_word = 0;
            const char * c = word;
            // BCD encode the first six characters of the space-padded test word...
            for (int i = 0; i < 6; ++i) {
                bcd_encoded_word <<= 6;
                if (*c) {
                    assert(hollerith_defined(*c));
                    bcd_encoded_word |= hollerith_encoding[static_cast<unsigned char>(*c++)];
                }
                else
                    bcd_encoded_word |= hollerith_encoding[static_cast<unsigned char>(' ')];
            }
            // test we get the expected 5-bit hash value
            TEST_EQUAL(hash(bcd_encoded_word, 5), expected_hash_value);
        }
    }


    // More real-world test cases from Rupert Lane's emulated ELIZA,
    // confirmed by giving the inputs "my purpose",... to ELIZA running
    // .TAPE. 100 and waiting for the memory to be produced.
    TEST_EQUAL(hash(last_chunk_as_bcd("PURPOSE"),         2), 1ull);
    TEST_EQUAL(hash(last_chunk_as_bcd("DEVONSHIRE"),      2), 0ull);
    TEST_EQUAL(hash(last_chunk_as_bcd("PREDICAMENT"),     2), 3ull);
    TEST_EQUAL(hash(last_chunk_as_bcd("EXECUTIONERS"),    2), 3ull);
    TEST_EQUAL(hash(last_chunk_as_bcd("GLOUCESTERSHIRE"), 2), 2ull);


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
    // least significant N bits (15 bits)      0x70EE
    TEST_EQUAL(hash(0214366217062ull, 15), 0x70EEull);

    // HASH("TIME  ", 15) = HASH(0633144256060, 15)
    // input                                   0xCD9915C30
    // zero bit 35                             0x4D9915C30
    // squared                                 0x178572A252EF928900
    // shift left by N / 2 bits (7 bit)        0xBC2B9512977C9448000
    // discard the least significant 35 bits   0x178572A252E
    // least significant N bits (15 bits)      0x252E
    TEST_EQUAL(hash(0633144256060ull, 15), 0x252Eull);

    TEST_EQUAL(hash(0ull, 7), 0);
}


/*  The ELIZA script contains the opening_remarks followed by rules.
    (The formal syntax is given in the elizascript namespace below.)
    There are two types of rule: keyword_rule and memory_rule. They
    are represented by the following classes. */

// interface and data shared by both rules
class rule_base {
public:
    rule_base() = default;

    rule_base(const std::string & keyword, const std::string & word_substitution, int precedence)
        : keyword_(keyword), word_substitution_(word_substitution), precedence_(precedence)
    {}

    virtual ~rule_base() = default;


    void set_keyword(const std::string & keyword) { keyword_ = keyword; }

    void add_transformation_rule(const stringlist & decomposition,
        const std::vector<stringlist> & reassembly_rules)
    {
        trans_.emplace_back(decomposition, reassembly_rules);
    }

    int precedence() const { return precedence_; }
    std::string keyword() const { return keyword_; }


    enum class action {
        inapplicable,   // no transformation could be performed
        complete,       // transformation of input is complete
        newkey,         // request caller try next keyword in keystack
        linkkey         // request caller try returned keyword
    };

    // replace 'word' with substitute specified by script rule, if any
    virtual action apply_word_substitution(std::string & word)
    {
        if (word_substitution_.empty() || word != keyword_)
            return action::inapplicable;
        word = word_substitution_;
        return action::complete;
    }

    // replace 'word' with substitute specified by script rule, if any
    virtual std::string word_substitute(const std::string & word) const
    {
        if (word_substitution_.empty() || word != keyword_)
            return word;
        return word_substitution_;
    }

    // return true iff this rule has whole-sentence transformation rules associated with it
    virtual bool has_transformation() const { return false; }

    // use this rule's decomposition/reassembly rules to transform given 'words'
    virtual action apply_transformation(stringlist & /*words*/,
        const tagmap & /*tags*/, std::string & /*link_keyword*/)
    {
        return action::inapplicable;
    }

    virtual stringlist dlist_tags() const { return stringlist(); }

    virtual std::string to_string() const = 0;

    virtual std::string trace() const { return std::string(); }

protected:
    std::string keyword_;           // the word that triggers this rule
    std::string word_substitution_; // the word that is to replace the keyword, if any
    int precedence_{ 0 };           // the highest priority rule is selected first

    struct transform {              // decomposition and associated reassembly rules
        stringlist decomposition;
        std::vector<stringlist> reassembly_rules;
        unsigned next_reassembly_rule{ 0 };
        transform() = default;
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
using rulemap = std::map<std::string, std::shared_ptr<rule_base>>;

// The NONE rule is a special-case that cannot match any user input text
const std::string special_rule_none{"zNONE"};

// to differentiate trace text from conversation text
const std::string trace_prefix{" | "};


/* e.g.
    (MEMORY MY
        (0 YOUR 0 = LETS DISCUSS FURTHER WHY YOUR 3)
        (0 YOUR 0 = EARLIER YOU SAID YOUR 3)
        (0 YOUR 0 = BUT YOUR 3)
        (0 YOUR 0 = DOES THAT HAVE ANYTHING TO DO WITH THE FACT THAT YOUR 3))
*/
class rule_memory : public rule_base {
public:
    rule_memory() = default;

    explicit rule_memory(const std::string & keyword)
        : rule_base(keyword, "", 0)
    {}

    bool empty() const { return keyword_.empty() || trans_.empty(); }

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
        if (!match(tags, transformation.decomposition, words, constituents)) {
            trace_ << trace_prefix
                 << "cannot form new memory: decomposition pattern ("
                 << join(transformation.decomposition)
                 << ") does not match user text\n";
            return;
        }

        const auto new_memory{join(reassemble(transformation.reassembly_rules[0], constituents))};
        trace_ << trace_prefix << "new memory: " << new_memory << '\n';
        memories_.push_back(new_memory);
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

    virtual void clear_trace()
    {
        trace_.str("");
    }
    virtual std::string trace() const
    {
        return trace_.str();
    }
    std::string trace_memory_stack() const
    {
        std::stringstream s;
        if (memories_.empty())
            s << trace_prefix << "memory queue: <empty>\n";
        else {
            s << trace_prefix << "memory queue:\n";
            for (auto m : memories_)
                s << trace_prefix << "  " << m << "\n";
        }
        return s.str();
    }

    // the MEMORY rule must have this number of transformations
    static constexpr int num_transformations = 4;

private:
    stringlist memories_;
    std::stringstream trace_;
};


/* e.g.
    (MY = YOUR 2
        ((0 YOUR 0 (/FAMILY) 0)
            (TELL ME MORE ABOUT YOUR FAMILY)
            (WHO ELSE IN YOUR FAMILY 5)
            (YOUR 4)
            (WHAT ELSE COMES TO MIND WHEN YOU THINK OF YOUR 4))
        ((0 YOUR 0)
            (YOUR 3)
            (WHY DO YOU SAY YOUR 3)
            (DOES THAT SUGGEST ANYTHING ELSE WHICH BELONGS TO YOU)
            (IS IT IMPORTANT TO YOU THAT 2 3)))
*/
class rule_keyword : public rule_base {
public:
    rule_keyword() = default;

    rule_keyword(
        const std::string & keyword,
        const std::string & word_substitution,
        int precedence,
        const stringlist & tags,
        const std::string & link_keyword)
        : rule_base(keyword, word_substitution, precedence),
        tags_(tags), link_keyword_(link_keyword)
    {}

    stringlist dlist_tags() const { return tags_; }

    virtual bool has_transformation() const
    {
        return !trans_.empty() || !link_keyword_.empty();
    }

    virtual action apply_transformation(
        stringlist & words, const tagmap & tags, std::string & link_keyword)
    {
        trace_begin(words);
        stringlist constituents;
        auto rule = trans_.begin();
        while (rule != trans_.end() && !match(tags, rule->decomposition, words, constituents))
            ++rule;
        if (rule == trans_.end()) {
            if (link_keyword_.empty()) {
                trace_nomatch();
                return action::inapplicable; // [page 39 (f)] should not happen?
            }
            trace_reference(link_keyword_);
            link_keyword = link_keyword_;
            return action::linkkey;
        }
        trace_decomp(rule->decomposition, constituents);

        // get the next reassembly rule to be used for this decomposition rule
        stringlist& reassembly_rule = rule->reassembly_rules[rule->next_reassembly_rule];
        trace_reassembly(reassembly_rule);

        // update the reassembly rule index so that they all get cycled through
        rule->next_reassembly_rule++;
        if (rule->next_reassembly_rule == rule->reassembly_rules.size())
            rule->next_reassembly_rule = 0;

        // is it the special-case reassembly rule (NEWKEY)?
        if (reassembly_rule.size() == 1 && reassembly_rule[0] == "NEWKEY")
            return action::newkey; // yes, try the next highest priority keyword, if any

        // is it the special-case reassembly rule (=XXXX)?
        if (reassembly_rule.size() == 2 && reassembly_rule[0].size() == 1 && reassembly_rule[0][0] == '=') {
            link_keyword = reassembly_rule[1];
            return action::linkkey; // yes, try the specified keyword
        }

        // is it the special-case reassembly rule (PRE (reassembly) (=reference))
        // (note: this is the only reassembly_rule that is still in a list)
        if (!reassembly_rule.empty() && reassembly_rule[0] == "(") {
            auto r = std::next(reassembly_rule.begin(), 3); // skip '(', 'PRE', '('
            stringlist reassembly;
            while (*r != ")")
                reassembly.push_back(*r++);
            words = reassemble(reassembly, constituents);
            r += 3; // skip ')', '(' and '='
            link_keyword = *r;
            return action::linkkey;
        }

        // use the selected reassembly rule and decomposition components
        // to construct a response sentence
        words = reassemble(reassembly_rule, constituents);
        return action::complete;
    }

    virtual std::string to_string() const
    {
        std::string sexp("(");
        sexp += (keyword_ == elizalogic::special_rule_none) ? "NONE" : keyword_;

        if (!word_substitution_.empty())
            sexp += " = " + word_substitution_;

        if (!tags_.empty())
            sexp += " DLIST(" + join(tags_) + ")";

        if (precedence_ > 0)
            sexp += " " + std::to_string(precedence_);

        for (const auto& k : trans_) {
            sexp += "\n    ((" + join(k.decomposition) + ")";
            for (auto r : k.reassembly_rules) {
                if (r.empty())
                    sexp += "\n        ()";
                else if (r[0] == "(")
                    sexp += "\n        " + join(r); // it's a PRE rule
                else if (r[0] == "=") {
                    r.pop_front();
                    sexp += "\n        (=" + join(r) + ")"; // it's a reference =XXX
                }
                else
                    sexp += "\n        (" + join(r) + ")";
            }
            sexp += ")";
        }

        if (!link_keyword_.empty())
            sexp += "\n    (=" + link_keyword_ + ")";

        sexp += ")\n";
        return sexp;
    }

    virtual std::string trace() const
    {
        return trace_.str();
    }

private:
    stringlist tags_;
    std::string link_keyword_;

    std::stringstream trace_;
    void trace_begin(const stringlist & words) {
        trace_.str("");
        trace_
            << trace_prefix << "selected keyword: " << keyword_ << '\n'
            << trace_prefix << "input: " << join(words) << '\n';
    }
    void trace_nomatch() {
        trace_ << trace_prefix << "ill-formed script? No decomposition rule matches\n";
    }
    void trace_reference(const std::string & ref) {
        trace_ << trace_prefix << "reference to equivalence class: " << ref << '\n';
    }
    void trace_decomp(const stringlist & d, const stringlist & constituents) {
        trace_ << trace_prefix << "matching decompose pattern: (" << join(d) << ")\n";
        trace_ << trace_prefix << "decomposition parts: ";
        for (int id = 1; const auto & c : constituents) {
            if (id > 1)
                trace_ << ", ";
            trace_ << id++ << ":\"" << c << '"';
        }
        trace_ << '\n';
    }
    void trace_reassembly(const stringlist & r) {
        trace_ << trace_prefix << "selected reassemble rule: (" << join(r) << ")\n";
    }
};


// collect all tags from any of the given rules that have them into a tagmap
tagmap collect_tags(const rulemap & rules)
{
    tagmap tags;
    for (const auto & tag : rules) {
        stringlist keyword_tags(tag.second->dlist_tags());
        for (auto t : keyword_tags) {
            if (t == "/")
                continue;
            if (t.size() > 1 && t.front() == '/')
                pop_front(t);
            tags[t].push_back(tag.second->keyword());
        }
    }
    return tags;
}


template<typename T>
auto get_rule(rulemap & rules, const std::string & keyword)
{
    auto rule = rules.find(keyword);
    if (rule == rules.end()) {
        std::string msg("script error: missing keyword ");
        msg += keyword;
        throw std::runtime_error(msg);
    }
    auto castrule = std::dynamic_pointer_cast<T>(rule->second);
    if (!castrule) {
        std::string msg("internal error for keyword ");
        msg += keyword;
        throw std::runtime_error(msg);
    }
    return castrule;
}


// return true iff given c is delimiter (see delimiter())
bool delimiter_character(char c)
{
    return c == ',' || c == '.';
};


// Return given string s split into a list of "words."
// A word in s is any sequence of one or more non-space characters.
// Any single character that also appears in given punctuation
// is also considered a separate word.
// e.g. split_user_input("ONE   ABUTMENT, BUT THREE.", ",.")
//          -> ["ONE", "ABUTMENT", ",", "BUT", "THREE", "."]
stringlist split_user_input(const std::string & s, const std::string & punctuation)
{
    stringlist result;
    std::string word;
    for (auto ch : s) {
        if (ch == ' ' || std::find(punctuation.begin(), punctuation.end(), ch) != punctuation.end()) {
            if (!word.empty()) {
                result.push_back(word);
                word.clear();
            }
            if (ch != ' ')
                result.emplace_back(1, ch);
        }
        else
            word.push_back(ch);
    }
    if (!word.empty())
        result.push_back(word);
    return result;
}


DEF_TEST_FUNC(split_user_input_test)
{
    TEST_EQUAL(split_user_input("ONE   TWO, THREE,, DON'T.", ",."),
                    stringlist({ "ONE", "TWO", ",", "THREE", ",", ",", "DON'T", "." }));

    TEST_EQUAL(split_user_input(" ONE TWO, THREE,, DON'T. ", ",."),
                    stringlist({ "ONE", "TWO", ",", "THREE", ",", ",", "DON'T", "." }));

    TEST_EQUAL(split_user_input(" ONE TWO, THREE,, DON'T. ", ",.'"),
                    stringlist({ "ONE", "TWO", ",", "THREE", ",", ",", "DON", "'", "T", "." }));

    TEST_EQUAL(split_user_input("ONE   ABUTMENT, BUT THREE.", ",."),
                    stringlist({"ONE", "ABUTMENT", ",", "BUT", "THREE", "."}));

    TEST_EQUAL(split_user_input("ONE   ABUTMENT, BUT THREE.", "."),
                    stringlist({"ONE", "ABUTMENT,", "BUT", "THREE", "."}));

    TEST_EQUAL(split_user_input("ONE   ABUTMENT, BUT THREE.", ""),
                    stringlist({"ONE", "ABUTMENT,", "BUT", "THREE."}));
}


class tracer {
public:
    virtual ~tracer() = 0;
    virtual void begin_response(const stringlist & /*words*/) = 0;
    virtual void limit(int /*limit*/, const std::string & /*built_in_msg*/) = 0;
    virtual void discard_subclause(const std::string & /*text*/) = 0;
    virtual void word_substitution(const std::string & /*word*/, const std::string & /*substitute*/) = 0;
    virtual void create_memory(const std::string & /*text*/) = 0;
    virtual void using_memory(const std::string & /*script*/) = 0;
    virtual void subclause_complete(
        const std::string & /*subclause*/,
        const stringlist & /*keystack*/,
        const rulemap & /*rules*/ ) = 0;
    virtual void unknown_key(const std::string & /*keyword*/, bool /*use_nomatch_msg*/) = 0;
    virtual void decomp_failed(bool /*use_nomatch_msg*/) = 0;
    virtual void newkey_failed(const std::string & /*response_source*/) = 0;
    virtual void transform(const std::string & /*text*/, const std::string & /*script*/) = 0;
    virtual void memory_stack(const std::string & /*text*/) = 0;
    virtual void pre_transform(const std::string & /*keyword*/, const stringlist & /*words*/) = 0;
    virtual void using_none(const std::string & /*script*/) = 0;
};

tracer::~tracer() = default;


class null_tracer : public tracer {
public:
    virtual ~null_tracer() = default;
    virtual void begin_response(const stringlist & /*words*/) {}
    virtual void limit(int /*limit*/, const std::string & /*built_in_msg*/) {}
    virtual void discard_subclause(const std::string & /*text*/) {}
    virtual void word_substitution(const std::string & /*word*/, const std::string & /*substitute*/) {}
    virtual void create_memory(const std::string & /*text*/) {}
    virtual void using_memory(const std::string & /*script*/) {}
    virtual void subclause_complete(
        const std::string & /*subclause*/,
        const stringlist & /*keystack*/,
        const rulemap & /*rules*/ ) {}
    virtual void unknown_key(const std::string & /*keyword*/, bool /*use_nomatch_msg*/) {}
    virtual void decomp_failed(bool /*use_nomatch_msg*/) {}
    virtual void newkey_failed(const std::string & /*response_source*/) {}
    virtual void transform(const std::string & /*text*/, const std::string & /*script*/) {}
    virtual void memory_stack(const std::string & /*text*/) {}
    virtual void pre_transform(const std::string & /*keyword*/, const stringlist & /*words*/) {}
    virtual void using_none(const std::string & /*script*/) {}
};


class pre_tracer : public null_tracer {
public:
    virtual ~pre_tracer() = default;
    virtual void pre_transform(const std::string & keyword, const stringlist & words) {
        std::cout << join(words) <<  "   :" << keyword << "\n";
    }
};


class string_tracer : public null_tracer {
    std::stringstream trace_;
    std::stringstream script_;
    std::string word_substitutions_;
public:
    virtual ~string_tracer() = default;
    virtual void begin_response(const stringlist & words)
    {
        trace_.str("");
        script_.str("");
        word_substitutions_.clear();
        trace_ << trace_prefix << "input: " << join(words) << '\n';
    }
    virtual void limit(int limit, const std::string & built_in_msg)
    {
        trace_ << trace_prefix << "LIMIT: " << limit << " (" << built_in_msg << ")\n";
    }
    virtual void discard_subclause(const std::string & s)
    {
        trace_ << trace_prefix << "word substitutions made: "
            << (word_substitutions_.empty() ? "<none>" : word_substitutions_)
            << '\n';
        trace_ << trace_prefix << "no transformation keywords found in subclause: " << s << '\n';
        word_substitutions_.clear();
    }
    virtual void word_substitution(const std::string & word, const std::string & substitute)
    {
        if (substitute != word) {
            if (!word_substitutions_.empty())
                word_substitutions_ += ", ";
            word_substitutions_ += word + '/' + substitute;
        }
    }
    virtual void create_memory(const std::string & s) { trace_ << s; }
    virtual void using_memory(const std::string & s)
    {
        trace_
            << trace_prefix << "LIMIT=4 (\"a certain counting mechanism is in a particular state\"),\n"
            << trace_prefix << "  and there are unused memories, so the response is the oldest of these\n";
        script_ << s;
    }
    virtual void subclause_complete(
        const std::string & subclause,
        const stringlist & keystack,
        const rulemap & rules)
    {
        trace_ << trace_prefix << "word substitutions made: "
            << (word_substitutions_.empty() ? "<none>" : word_substitutions_)
            << '\n';
        if (keystack.empty()) {
            if (!subclause.empty())
                trace_ << trace_prefix
                    << "no transformation keywords found in subclause: " << subclause << '\n';
        }
        else {
            trace_ << trace_prefix << "found keywords in subclause: " + subclause + '\n';
            trace_ << trace_prefix << "keyword(precedence) stack:";
            bool comma = false;
            for (auto& keyword : keystack) {
                trace_ << (comma ? ", " : " ") << keyword << "(";
                const auto r = rules.find(keyword);
                if (r != rules.end()) {
                    const auto& rule = r->second;
                    if (rule->has_transformation())
                        trace_ << rule->precedence();
                    else
                        trace_ << "<internal error: no transform associated with this keyword>";
                }
                else
                    trace_ << "<internal error: unknown keyword>";
                trace_ << ')';
                comma = true;
            }
            trace_ << '\n';
        }
    }
    virtual void unknown_key(const std::string & keyword, bool use_nomatch_msg) {
        trace_ << trace_prefix << "ill-formed script: \"" << keyword << "\" is not a keyword\n";
        if (use_nomatch_msg)
            trace_ << trace_prefix << "response is the built-in NOMACH[LIMIT] message\n";
    }
    virtual void decomp_failed(bool use_nomatch_msg) {
        trace_ << trace_prefix << "ill-formed script? No decomposition rule matched input\n";
        if (use_nomatch_msg)
            trace_ << trace_prefix << "response is the built-in NOMACH[LIMIT] message\n";
    }
    virtual void newkey_failed(const std::string & response_source) {
        trace_ << trace_prefix << "keyword stack is empty; response is a " << response_source << " message\n";
    }
    virtual void transform(const std::string & t, const std::string & s) {
        trace_ << t;
        script_ << s;
    }
    virtual void memory_stack(const std::string & t) {
        trace_ << t;
    }
    virtual void using_none(const std::string & s) {
        trace_ << trace_prefix << "response is the next remark from the NONE rule\n";
        script_ << s;
    }

    std::string text() const { return trace_.str(); }
    std::string script() const { return script_.str(); }
    void clear() { trace_.str(""); script_.str(""); }
};



                //////// //       //// ////////    ///                    
                //       //        //       //    // //                   
                //       //        //      //    //   //                  
///////////     //////   //        //     //    //     //     /////////// 
                //       //        //    //     /////////                 
                //       //        //   //      //     //                 
                //////// //////// //// //////// //     //                 

class eliza {
public:
    eliza(const rulemap & rules, std::shared_ptr<rule_memory> mem_rule)
        : rules_(rules), mem_rule_(mem_rule), tags_(collect_tags(rules_))
    {
        /*  In the 1966 CACM ELIZA paper on page 37 Weizenbaum says
            "the procedure recognizes a comma or a period as a delimiter."
            However, in the MAD-SLIP source code the relevant code is

                W'R WORD .E. $.$ .OR. WORD .E. $,$ .OR. WORD .E. $BUT$

            (W'R means WHENEVER, .E. means equals and alphabetic constants are
            bounded by $-signs.) So BUT is also a delimiter. */
        set_delimeters({ ",", ".", "BUT" });
    }

    ~eliza() = default;

    // true use built-in error msgs (default); false use NONE messages instead
    // (Weizenbaum's ELIZA used built-in error messages. The option to use
    // NONE messages instead is provided for attempts to reproduce conversations
    // with some non-Weizenbaum ELIZAs.)
    void set_use_nomatch_msgs(bool f) { use_nomatch_msgs_ = f; }

    void set_on_newkey_fail_use_none(bool f) { on_newkey_fail_use_none_ = f; }
    void set_use_limit(bool f) { limit_ = 2; }

    void set_delimeters(const stringlist & delims)
    {
        delimiters_ = delims;
        punctuation_.clear();
        const std::string bcd_punctuation{"='+.)-$*/,("}; // (excluding space)
        for (const auto & d : delims) {
            if (d.size() == 1) {
                if (std::find(bcd_punctuation.begin(), bcd_punctuation.end(), d[0]) != bcd_punctuation.end())
                    punctuation_ += d[0];
            }
        }
    }

    // provide the user with a window into ELIZA's thought processes(!)
    void set_tracer(tracer * tr) { trace_ = tr; }


    //////////////////////////////// ELIZA ////////////////////////////////
    //
    // produce a response to the given input (this is the core ELIZA algorithm)
    //
    std::string response(const std::string & input)
    {
        // for simplicity, convert the given input string to a list of uppercase words
        // e.g. "Hello, world!" -> ("HELLO" "," "WORLD" ".")
        stringlist words(split_user_input(eliza_uppercase(input), punctuation_));
        trace_->begin_response(words);

        // JW's "a certain counting mechanism" is updated for each response
        limit_ = limit_ % 4 + 1;
        trace_->limit(limit_, nomatch_msgs_[limit_ - 1]);

        // scan for keywords [page 38 (c)]; build the keystack; apply word substitutions
        stringlist keystack;
        int top_rank = 0;
        for (auto word = words.begin(); word != words.end(); ) {
            if (delimiter(*word)) {
                // keep only the first clause to contain a keyword [page 37 (c)]
                if (keystack.empty()) {
                    // discard left of and including, continue scanning what remains
                    ++word;
                    trace_->discard_subclause(join({words.begin(), word}));
                    word = words.erase(words.begin(), word);
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
                const std::string substitute(rule->word_substitute(*word)); // [page 39 (a)]
                trace_->word_substitution(*word, substitute);
                *word = substitute;
            }

            ++word;
        }
        trace_->subclause_complete(join(words), keystack, rules_);

        mem_rule_->clear_trace();
        trace_->memory_stack(mem_rule_->trace_memory_stack());
        if (keystack.empty()) {
            /*  a text without keywords; can we recall a MEMORY ? [page 41 (f)]
                JW's 1966 CACM paper refers to this decision as "a certain counting
                mechanism is in a particular state." The ELIZA code shows that the
                memory is recalled only when LIMIT has the value 4 */
            if ((!use_limit_ || limit_ == 4) && mem_rule_->memory_exists()) {
                trace_->using_memory(mem_rule_->to_string());
                return mem_rule_->recall_memory();
            }
        }

        // the keystack contains all keywords that occur in the given 'input';
        // apply transformation associated with the top keyword [page 39 (d)]
        while (!keystack.empty()) {
            const std::string top_keyword = pop_front(keystack);
            trace_->pre_transform(top_keyword, words);

            auto r = rules_.find(top_keyword);
            if (r == rules_.end()) {
                // e.g. could happen if a rule links to a non-existent keyword
                trace_->unknown_key(top_keyword, use_nomatch_msgs_);
                if (use_nomatch_msgs_)
                    return nomatch_msgs_[limit_ - 1];
                break; // (use NONE message)
            }
            auto rule = r->second;

            // try to lay down a memory for future use
            mem_rule_->create_memory(top_keyword, words, tags_);
            trace_->create_memory(mem_rule_->trace());

            // perform the transformation for this rule
            std::string link_keyword;
            auto act = rule->apply_transformation(words, tags_, link_keyword);
            trace_->transform(rule->trace(), rule->to_string());

            if (act == rule_base::action::complete)
                return join(words); // decomposition/reassembly successfully applied

            if (act == rule_base::action::inapplicable) {
                // no decomposition rule matched the input words; script error
                trace_->decomp_failed(use_nomatch_msgs_);
                if (use_nomatch_msgs_)
                    return nomatch_msgs_[limit_ - 1];
                break; // (use NONE message)
            }

            assert(act == rule_base::action::linkkey || act == rule_base::action::newkey);

            if (act == rule_base::action::linkkey)
                keystack.push_front(link_keyword); // rule links to another; loop

            else if (keystack.empty()) {
                // newkey means try next highest keyword, but keystack is empty.
                // The 1966 CACM ELIZA paper, page 41, implies in this situation
                // a NONE message is used. The conversations in the Quarton pilot
                // study suggests that a built-in message is used.
                if (!on_newkey_fail_use_none_ && use_nomatch_msgs_) {
                    trace_->newkey_failed("built-in nomatch");
                    return nomatch_msgs_[limit_ - 1];
                }
                trace_->newkey_failed("NONE");
                break; // (use NONE message)
            }
        }

        // last resort: the NONE rule never fails to produce a response [page 41 (d)]
        auto none_rule = get_rule<rule_keyword>(rules_, elizalogic::special_rule_none);
        std::string discard;
        none_rule->apply_transformation(words, tags_, discard);
        trace_->using_none(none_rule->to_string());
        return join(words);
    }
    //////////////////////////////// end ////////////////////////////////


private:
    // JW's "a certain counting mechanism," LIMIT, cycles through 1..4, then back to 1
    int limit_{ 1 };
    bool use_limit_{ true };
    stringlist delimiters_;
    std::string punctuation_;

    // return true iff given s is an ELIZA delimiter
    bool delimiter(const std::string & s) const
    {
        return std::find(delimiters_.begin(), delimiters_.end(), s) != delimiters_.end();
    }

    /*  In the 1966 CACM ELIZA paper on page 41 Weizenbaum says

        "A serious problem which remains to be discussed is the reaction of
        the system in case no keywords remain to serve as transformation
        triggers. This can arise either in case the keystack is empty when
        NEWKEY is invoked or when the input text contained no keywords
        initially.
        "The simplest mechanism supplied is in the form of the special
        reserved keyword "NONE" which must be part of any script."

        However, the McGuire, Lorch, Quarton study conversations show that
        if the keystack is empty when NEWKEY is invoked the response is a
        nomatch message, not a NONE message. */
    bool on_newkey_fail_use_none_{ true };

    // the ELIZA script in 'rulemap' form
    rulemap rules_;

    // the one MEMORY rule
    std::shared_ptr<rule_memory> mem_rule_;

    // e.g. tags[BELIEF] -> (BELIEVE FEEL THINK WISH)
    // (This is derived from rules_. It's a member so we only need derive it once.)
    const tagmap tags_;

    // script error messages hard-coded in JW's ELIZA, selected by LIMIT (our limit_)
    static const char * const nomatch_msgs_[4];
    bool use_nomatch_msgs_{ true };

    // by default ELIZA's thought processes are discarded
    null_tracer nulltr_;
    tracer * trace_{ &nulltr_ };

    // eliza isn't copyable because various members aren't copyable
    eliza(const eliza &) = delete;
    eliza & operator=(const eliza &) = delete;
};

// script error messages hard-coded in JW's ELIZA, selected by LIMIT (our limit_)
const char * const eliza::nomatch_msgs_[4] = {
    "PLEASE CONTINUE",
    "HMMM",
    "GO ON , PLEASE",
    "I SEE"
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


/*  This is my understanding of the ELIZA script file from the explanation
    in the CACM article and observation of the example script given in the
    APPENDIX to the CACM article:

    The script is a series of S-expressions. The first S-expression is a list
    of words that ELIZA will emit at startup [page 42 (a)]. This may be
    followed by the atom START. Following that are the pattern-matching and
    message assembly Rules. Finally, there may be an empty list.

    e.g.
        (HOW DO YOU DO.  PLEASE TELL ME YOUR PROBLEM)
        START
        <Rules>
        ()

    The following modified BNF is used to define the ELIZA script syntax.

        x : y ;

    Things on the left of the colon must be replaced by (or "produce") the
    things on the right, and things on the right of the colon are either
    terminals or must also appear on the left in some other rule.
    The semicolon signals the end of the rule. In addition, things on the
    right may be sequences, choices or repetitions:

        x y     means there must be an x followed by a y here
        x|y     means there must be either x or y here
        [x]     means there must be zero or one of x here
        {x}     means there must be zero of more x here; note that {x|y}
                produces, for example, x, y, xx, yy, xy, yx, xyx, xyxxy, ...
        'x'     means literally x, a terminal

    The ELIZA script syntax:

    eliza_script        : opening_remarks ['START'] rules ['(' ')']             ;
    opening_remarks     : '(' {word | punctuation} ')'                          ;
    rules               : keyword_rule {keyword_rule} memory_rule none_rule     ;

    keyword_rule        : '(' keyword rule ')'                                  ;
    keyword             : word                                                  ;
    rule                : '=' substitute_word
                        | 'DLIST' tags
                        | ['=' substitute_word]
                            ['DLIST' tags]
                            [precedence]
                            reference
                        | ['=' substitute_word]
                            ['DLIST' tags]
                            [precedence]
                            transformation {transformation}
                            [reference]                                         ;

    memory_rule         : '(' 'MEMORY' keyword
                            '(' decompose_terms '=' reassemble_terms ')'
                            '(' decompose_terms '=' reassemble_terms ')'
                            '(' decompose_terms '=' reassemble_terms ')'
                            '(' decompose_terms '=' reassemble_terms ')' ')'    ;

    none_rule           : '(' 'NONE' '(' '(' '0' ')'
                            reassemble_pattern {reassemble_pattern} ')' ')'     ;

    substitute_word     : word                                                  ;
    precedence          : integer                                               ;
    reference           : '(' '=' keyword ')'                                   ;

    transformation      : '(' decompose_pattern
                            reassemble_rule {reassemble_rule} ')'               ;
    decompose_pattern   : '(' decompose_terms ')'                               ;
    decompose_terms     : decompose_term {decompose_term}                       ;
    decompose_term      : word | match_count | tags | any_of                    ;
    match_count         : integer                                               ;
    tags                : '(' '/' word {word} ')'                               ;
    any_of              : '(' '*' word {word} ')'                               ;

    reassemble_rule     : reassemble_pattern
                        | reference
                        | newkey
                        | pre_transform_ref                                     ;

    reassemble_pattern  : '(' reassemble_terms ')'                              ;
    reassemble_terms    : reassemble_term {reassemble_term}                     ;
    reassemble_term     : word | punctuation | match_index                      ;
    match_index         : integer                                               ;
    newkey              : '(' 'NEWKEY' ')'                                      ;
    pre_transform_ref   : '(' 'PRE' reassemble_pattern reference ')'            ;

    word                : word_char {word_char}                                 ;
    word_char           : 'A'-'Z' | '-' | ''' (i.e. a single quote)             ;
    punctuation         : ',' | '.'                                             ;
    integer             : digit {digit}                                         ;
    digit               : '0'-'9'                                               ;


    Note that whitespace is ignored, except where it is necessary to separate
    symbols that would otherwise merge. For example, two adjacent words need
    at least one space between them to stop them merging into one.

    To the syntax we must add these semantic rules

    1. No two keyword_rules may have the same keyword.

    2. If there are multiple transformation rules, the decompose_pattern of
    each is tried in turn and the first that successfully matches the user’s
    input text is selected. If none of the decompose_patterns match the user’s
    input, and the transformation rules are not followed by a reference, the
    keyword rule fails to produce a response and ELIZA will emit one of four
    built-in messages.

    3. If a transformation has multiple reassemble_rules they are used in
    turn: the first time a decompose_pattern matches a user's input the first
    reassemble_rule is used to create ELIZA’s response. The next time the
    user's input matches the same decompose_pattern the second reassemble_rule
    is used, and so on. When they have all been used a subsequent match will
    begin again with the first reassemble_rule.

    4. There must be exactly one memory_rule. The keyword associated with the
    memory_rule must also be the keyword in one of the keyword_rules.

    5. There must be a keyword_rule for the special keyword NONE. This rule
    must have a decompose_pattern that matches any text, (0), and associated
    "content-free" remarks, such as "PLEASE GO ON". It must not have a
    substitute_word, precedence or any DLIST tags.

    6. Any tag word referenced in a decompose_pattern must also be associated
    with at least one keyword via the DLIST mechanism.

    7. match_count will match the specified number of words, but doesn't
    specify what the words are. So a match_count of 2 means there must be two
    words here, but they can be any two words. A match_count of 0 means there
    can be any number, including none, of any words here.

    8. match_index is a 1-based index of the parts matching the
    decompose_pattern. For example, a decompose_pattern (A 1 0) will match the
    text "A HILL OF BEANS" and when associated with the reassemble_pattern
    (FIRST 1 SECOND 2 THIRD 3) will produce the output "FIRST A SECOND HILL
    THIRD OF BEANS."

    9. There must exist a keyword_rule for any keyword referred to in a
    (=keyword) reference.

    10. An infinite loop may be created if keyword_rule A has a reference to
    itself or to another keyword_rule, which either refers directly back to A
    or refers back to A via one or more other keyword_rules. In this case
    ELIZA would generate no response message. Weizenbaum does not explicitly
    disallow this. This looping mechanism is fundamental to the possibility
    that ELIZA scripts are Turing complete; looping via the pre_transform_ref
    rule is not necessarily infinite.

    11. The script has the word START following the opening_remarks, and ends
    with an empty list. Both these elements are present in Weizenbaum's
    published DOCTOR script, but he doesn't mention them in the CACM paper.
    The purpose of the START symbol isn't clear, but the ELIZA source code
    appears to use an empty list as a sentinel signalling the end of the
    script. Both elements are included in this formal script syntax as
    optional. 
*/


struct script {
    // ELIZA's opening remarks e.g. "HOW DO YOU DO.  PLEASE TELL ME YOUR PROBLEM"
    stringlist hello_message;

    // maps keywords -> transformation rules
    elizalogic::rulemap rules;

    // the one and only special case MEMORY rule
    std::shared_ptr<elizalogic::rule_memory> mem_rule;
};


struct token {
    // types of token in an ELIZA script file
    enum class typ { eof, symbol, number, open_bracket, close_bracket };

    typ t{ typ::eof };
    std::string value;

    token() = default;
    explicit token(typ t) : t(t) {}
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
    explicit tokenizer(T & script_file) : stream_(script_file) {}

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

    size_t line() const
    {
        return line_number_;
    }

private:
    T & stream_;
    token t_;
    bool got_token_{ false };
    const static size_t buflen_ = 512;
    uint8_t buf_[buflen_];
    size_t bufdata_{ 0 };
    size_t bufptr_{ 0 };
    size_t line_number_{ 1 };

    token readtok()
    {
        uint8_t ch;
        for (;;) {
            do { // skip whitespace
                if (!nextch(ch))
                    return token(token::typ::eof);
                if (is_newline(ch))
                    consume_newline(ch);
            } while (is_whitespace(ch));
            if (ch != ';')
                break;
            do { // skip comment
                if (!nextch(ch))
                    return token(token::typ::eof);
            } while (!is_newline(ch));
            consume_newline(ch);
        }

        if (ch == '(')
            return token(token::typ::open_bracket);

        if (ch == ')')
            return token(token::typ::close_bracket);

        if (ch == '=')
            return token(token::typ::symbol, "=");

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
        while (peekch(ch) && !non_symbol(ch) && ch != '=') {
            t.value.push_back(ch);
            nextch(ch);
        }
        t.value = elizalogic::eliza_uppercase(t.value);
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
        if (bufptr_ == bufdata_) {
            refilbuf();
            if (bufptr_ == bufdata_)
                return false;
        }
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

    inline bool is_whitespace(uint8_t ch) const
    {
        return ch <= 0x20 || ch == 0x7F;
        // this must hold: is_newline(ch) implies is_whitespace(ch)
    }

    inline bool is_newline(uint8_t ch) const
    {
        return ch == '\x0A'     // LF
            || ch == '\x0B'     // VT
            || ch == '\x0C'     // FF
            || ch == '\x0D';    // CR
    }

    inline void consume_newline(uint8_t ch)
    {
        if (ch == '\x0D'        // CR
            && peekch(ch)
            && ch == '\x0A')    // LF
            nextch(ch);
        ++line_number_;
    }

    inline bool is_digit(uint8_t ch) const
    {
        return unsigned(ch) - '0' < 10;
    }

    inline bool non_symbol(uint8_t ch) const
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

        // does the script meet the minimum requirements?
        if (script_.rules.find(elizalogic::special_rule_none) == std::end(script_.rules))
            throw std::runtime_error(
                "Script error: no NONE rule specified; see Jan 1966 CACM page 41");
        if (!script_.mem_rule)
            throw std::runtime_error(
                "Script error: no MEMORY rule specified; see Jan 1966 CACM page 41");
        if (script_.rules.find(script_.mem_rule->keyword()) == std::end(script_.rules)) {
            std::string msg("Script error: MEMORY rule keyword '");
            msg += script_.mem_rule->keyword();
            msg += "' is not also a keyword in its own right; see Jan 1966 CACM page 41";
            throw std::runtime_error(msg);
        }
#if 0
        std::cout << "----------------\n";
        const int dict_size = 7;
        stringlist dict[1 << dict_size];
        int count = 0;
        for (const auto & tag : script_.rules) {
            if (tag.first == elizalogic::special_rule_none)
                continue;
            //std::cout << tag.first << " '" << tag.first.substr(0, 6) << "' " << elizalogic::hash(elizalogic::last_chunk_as_bcd(tag.first.substr(0, 6)), dict_size) << "\n";
            dict[elizalogic::hash(elizalogic::last_chunk_as_bcd(tag.first.substr(0, 6)), dict_size)].push_back(tag.first);
            ++count;
        }
        std::cout << count << " total entries\n";
        for (int i = 0; i < (1 << dict_size); ++i) {
            std::cout << i << ":";
            for (const auto k : dict[i]) {
                std::cout << " " << k;
            }
            std::cout << "\n";
        }
#endif
    }

private:
    tokenizer<T> tok_;
    script & script_;


    std::string errormsg(const std::string & msg)
    {
        return std::string("Script error on line ")
            + std::to_string(tok_.line())
            + ": "
            + msg;
    }


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
                throw std::runtime_error(errormsg("expected '('"));
            t = tok_.nexttok();
        }
        while (!t.close()) {
            if (t.symbol())
                s.emplace_back(t.value);
            else if (t.number())
                s.emplace_back(t.value);
            else if (t.open()) {
                // embed entire sublist in one sub-string
                std::string sublist;
                t = tok_.nexttok();
                while (!t.close()) {
                    if (!t.symbol())
                        throw std::runtime_error(errormsg("expected symbol"));
                    if (!sublist.empty())
                        sublist += ' ';
                    sublist += t.value;
                    t = tok_.nexttok();
                }
                s.emplace_back("(" + sublist + ")");
            }
            else
                throw std::runtime_error(errormsg("expected ')'"));
            t = tok_.nexttok();
        }

        return s;
    }


    /* e.g.
        (@MEMORY MY
            (0 YOUR 0 = LETS DISCUSS FURTHER WHY YOUR 3)
            (0 YOUR 0 = EARLIER YOU SAID YOUR 3)
            (0 YOUR 0 = BUT YOUR 3)
            (0 YOUR 0 = DOES THAT HAVE ANYTHING TO DO WITH THE FACT THAT YOUR 3))
    */
    bool read_memory_rule()
    {
#define MEMFORM "; expected form is (MEMORY keyword (decomp1=reassm1)(decomp2=reassm2)(decomp3=reassm3)(decomp4=reassm4))"

        token t = tok_.nexttok();
        assert(t.symbol("MEMORY"));
        if (script_.mem_rule)
            throw std::runtime_error(errormsg("MEMORY rule already specified"));

        t = tok_.nexttok();
        if (!t.symbol())
            throw std::runtime_error(errormsg("expected keyword to follow MEMORY" MEMFORM));
        script_.mem_rule = std::make_shared<elizalogic::rule_memory>(t.value);

        for (int i = 0; i < elizalogic::rule_memory::num_transformations; ++i) {
            stringlist decomposition;
            std::vector<stringlist> reassembly_rules;

            if (!tok_.nexttok().open())
                throw std::runtime_error(errormsg("expected '('" MEMFORM));
            for (t = tok_.nexttok(); !t.symbol("=") && !t.eof(); t = tok_.nexttok())
                decomposition.push_back(t.value);
            if (decomposition.empty())
                throw std::runtime_error(errormsg("expected 'decompose_terms = reassemble_terms'" MEMFORM));
            if (!t.symbol("="))
                throw std::runtime_error(errormsg("expected '='" MEMFORM));

            stringlist reassembly;
            for (t = tok_.nexttok(); !t.close() && !t.eof(); t = tok_.nexttok())
                reassembly.push_back(t.value);
            if (reassembly.empty())
                throw std::runtime_error(errormsg("expected 'decompose_terms = reassemble_terms'" MEMFORM));
            if (!t.close())
                throw std::runtime_error(errormsg("expected ')'" MEMFORM));
            reassembly_rules.push_back(reassembly);

            std::string index_out_of_range_msg;
            if (!elizalogic::reassembly_indexes_valid(decomposition, reassembly, index_out_of_range_msg))
                throw std::runtime_error(errormsg(index_out_of_range_msg));

            script_.mem_rule->add_transformation_rule(decomposition, reassembly_rules);
        }

        if (!tok_.nexttok().close())
            throw std::runtime_error(errormsg("expected ')'" MEMFORM));

        return true;

#undef MEMFORM
    }


    stringlist read_reassembly()
    {
        if (!tok_.nexttok().open())
            throw std::runtime_error(errormsg("expected '('"));
        if (!tok_.peektok().symbol("PRE"))
            return rdlist(false);

        // It's a PRE reassembly, e.g. (PRE (I ARE 3) (=YOU))
        tok_.nexttok(); // skip "PRE"
        stringlist pre{ "(", "PRE" };
        stringlist reconstruct = rdlist();
        stringlist reference = rdlist();
        if (reference.size() != 2 || reference[0] != "=")
            throw std::runtime_error(errormsg("expected '(=reference)' in PRE rule"));
        pre.emplace_back("(");
        pre.insert(pre.end(), reconstruct.begin(), reconstruct.end());
        pre.emplace_back(")");
        pre.emplace_back("(");
        pre.insert(pre.end(), reference.begin(), reference.end());
        pre.emplace_back(")");
        pre.emplace_back(")");
        if (!tok_.nexttok().close())
            throw std::runtime_error(errormsg("expected ')'"));
        return pre;
    }

    bool read_keyword_rule()
    {
        std::string keyword, keyword_substitution, msg_keyword;
        int precedence = 0;
        stringlist tags;
        struct transform {
            stringlist decomposition;
            std::vector<stringlist> reassembly;
        };
        std::vector<transform> transformation;
        std::string class_name;

        token t = tok_.nexttok();
        assert(t.symbol());
        keyword = msg_keyword = t.value;
        if (keyword == "NONE")
            keyword = elizalogic::special_rule_none;

        if (script_.rules.find(keyword) != script_.rules.end()) {
            std::string msg("keyword rule already specified for keyword '");
            msg += msg_keyword;
            msg += "'";
            throw std::runtime_error(errormsg(msg));
        }

        if (tok_.peektok().close()) {
            std::string msg("keyword '");
            msg += msg_keyword;
            msg += "' has no associated body";
            throw std::runtime_error(errormsg(msg));
        }

        for (t = tok_.nexttok(); !t.close(); t = tok_.nexttok()) {
            if (t.symbol("=")) {
                t = tok_.nexttok();
                if (!t.symbol())
                    throw std::runtime_error(errormsg("expected keyword"));
                keyword_substitution = t.value;
            }
            else if (t.number())
                precedence = std::stoi(t.value);
            else if (t.symbol("DLIST"))
                tags = rdlist();
            else if (t.open()) {
                // a transformation rule
                t = tok_.peektok();
                if (t.symbol("=")) {
                    // a reference
                    tok_.nexttok();
                    t = tok_.nexttok();
                    if (!t.symbol())
                        throw std::runtime_error(errormsg("expected equivalence class name"));
                    class_name = t.value;

                    if (!tok_.nexttok().close())
                        throw std::runtime_error(errormsg("expected ')'"));
                    if (!tok_.peektok().close())
                        throw std::runtime_error(errormsg("expected ')'"));
                }
                else {
                    // a decompose/reassemble transformation
                    transform trans;
                    trans.decomposition = rdlist();
                    if (trans.decomposition.empty())
                        throw std::runtime_error(errormsg("decompose pattern cannot be empty"));
                    do {
                        trans.reassembly.push_back(read_reassembly());
                        std::string index_out_of_range_msg;
                        if (!elizalogic::reassembly_indexes_valid(trans.decomposition, trans.reassembly.back(), index_out_of_range_msg))
                            throw std::runtime_error(errormsg(index_out_of_range_msg));
                    } while (tok_.peektok().open());
                    if (!tok_.nexttok().close())
                        throw std::runtime_error(errormsg("expected ')'"));
                    transformation.emplace_back(trans);
                }
            }
            else
                throw std::runtime_error(errormsg("malformed rule"));
        }

        auto r = std::make_shared<elizalogic::rule_keyword>(
            keyword, keyword_substitution, precedence, tags, class_name);
        for (auto const & tr : transformation)
            r->add_transformation_rule(tr.decomposition, tr.reassembly);
        script_.rules[keyword] = r;

        return true;
    }

    // read one rule of any type; return false => end of file reached
    bool read_rule()
    {
        token t = tok_.nexttok();
        if (t.eof())
            return false;
        if (!t.open())
            throw std::runtime_error(errormsg("expected '('"));
        t = tok_.peektok();
        if (t.close()) {
            tok_.nexttok();
            return true; // ignore empty rule list, if present
        }
        if (!t.symbol())
            throw std::runtime_error(errormsg("expected keyword|MEMORY|NONE"));
        if (t.value == "MEMORY")
            return read_memory_rule();
        return read_keyword_rule();
    }

};


template<typename T>
void read(T & script_file, script & s)
{
    eliza_script_reader<T> reader(script_file, s);
}

template<>
void read(const char * const & script_text, script & s)
{
    std::stringstream ss(script_text);
    elizascript::read<std::stringstream>(ss, s);
}

template<>
void read(const char * & script_text, script & s)
{
    std::stringstream ss(script_text);
    elizascript::read<std::stringstream>(ss, s);
}


const char * const CACM_1966_01_DOCTOR_script =
    ";\n"
    "; Joseph Weizenbaum's DOCTOR script for ELIZA\n"
    "; Copyright (c) 1966 Association for Computing Machinery, Inc."
    ";\n"
    "; This is a verbatim transcription of the script on page 44 of the\n"
    "; January 1966 edition of Communications of the ACM, with the following\n"
    "; caveats:\n"
    ";\n"
    "; a) Whitespace has been added to help reveal the structure of the script.\n"
    "; b) In the CACM paper six lines were printed twice adjacent to each\n"
    ";    other (with exactly 34 lines between each duplicate), making the\n"
    ";    structure nonsensical. These duplicates have been commented out of\n"
    ";    this transcription.\n"
    "; c) There were no comments in the script in the CACM paper.\n"
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
    "(DREAMED = DREAMT 4\n"
    "    (=DREAMT))\n"
    "\n"
    "(DREAM 3\n"
    "    ((0)\n"
    "        (WHAT DOES THAT DREAM SUGGEST TO YOU)\n"
    "        (DO YOU DREAM OFTEN)\n"
    "        (WHAT PERSONS APPEAR IN YOUR DREAMS)\n"
    "        (DON'T YOU BELIEVE THAT DREAM HAS SOMETHING TO DO WITH YOUR PROBLEM)\n"
    "        (NEWKEY)))\n"
    "\n"
    "(DREAMS = DREAM 3\n"
    "    (=DREAM))\n"
    "\n"
    "(HOW\n"
    "    (=WHAT))\n"
    "(WHEN\n"
    "    (=WHAT))\n"
    "(ALIKE 10\n"
    "    (=DIT))\n"
    "(SAME 10\n"
    "    (=DIT))\n"
    "(CERTAINLY\n"
    "    (=YES))\n"
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
    "(MAYBE\n"
    "    (=PERHAPS))\n"
    "\n"
    "(NAME 15\n"
    "    ((0)\n"
    "        (I AM NOT INTERESTED IN NAMES)\n"
    "        (I'VE TOLD YOU BEFORE, I DON'T CARE ABOUT NAMES - PLEASE CONTINUE)))\n"
    "; duplicate line removed: PLEASE CONTINUE)) )\n"
    "\n"
    "(DEUTSCH\n"
    "    (=XFREMD))\n"
    "(FRANCAIS\n"
    "    (=XFREMD))\n"
    "(ITALIANO\n"
    "    (=XFREMD))\n"
    "(ESPANOL\n"
    "    (=XFREMD))\n"
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
    "(MACHINE 50\n"
    "    (=COMPUTER))\n"
    "(MACHINES 50\n"
    "    (=COMPUTER))\n"
    "(COMPUTERS 50\n"
    "    (=COMPUTER))\n"
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
    "(WERE = WAS\n"
    "    (=WAS))\n"
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
    "        (=WHAT))\n"
    "    (=WHAT))\n"
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
    "(EVERYBODY 2\n"
    "    (= EVERYONE))\n"
    "(NOBODY 2\n"
    "    (= EVERYONE))\n"
    "(NOONE 2\n"
    "    (= EVERYONE))\n"
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
    "; --- End of ELIZA script ---\n"
    "\n"
    "\n"
    "; The ELIZA script syntax:\n"
    ";\n"
    "; eliza_script        : opening_remarks ['START'] rules ['(' ')']             ;\n"
    "; opening_remarks     : '(' {word | punctuation} ')'                          ;\n"
    "; rules               : keyword_rule {keyword_rule} memory_rule none_rule     ;\n"
    ";\n"
    "; keyword_rule        : '(' keyword rule ')'                                  ;\n"
    "; keyword             : word                                                  ;\n"
    "; rule                : '=' substitute_word\n"
    ";                     | 'DLIST' tags\n"
    ";                     | ['=' substitute_word]\n"
    ";                         ['DLIST' tags]\n"
    ";                         [precedence]\n"
    ";                         reference\n"
    ";                     | ['=' substitute_word]\n"
    ";                         ['DLIST' tags]\n"
    ";                         [precedence]\n"
    ";                         transformation {transformation}\n"
    ";                         [reference]                                         ;\n"
    ";\n"
    "; memory_rule         : '(' 'MEMORY' keyword\n"
    ";                         '(' decompose_terms '=' reassemble_terms ')'\n"
    ";                         '(' decompose_terms '=' reassemble_terms ')'\n"
    ";                         '(' decompose_terms '=' reassemble_terms ')'\n"
    ";                         '(' decompose_terms '=' reassemble_terms ')' ')'    ;\n"
    ";\n"
    "; none_rule           : '(' 'NONE' '(' '(' '0' ')'\n"
    ";                         reassemble_pattern {reassemble_pattern} ')' ')'     ;\n"
    ";\n"
    "; substitute_word     : word                                                  ;\n"
    "; precedence          : integer                                               ;\n"
    "; reference           : '(' '=' keyword ')'                                   ;\n"
    ";\n"
    "; transformation      : '(' decompose_pattern\n"
    ";                         reassemble_rule {reassemble_rule} ')'               ;\n"
    "; decompose_pattern   : '(' decompose_terms ')'                               ;\n"
    "; decompose_terms     : decompose_term {decompose_term}                       ;\n"
    "; decompose_term      : word | match_count | tags | any_of                    ;\n"
    "; match_count         : integer                                               ;\n"
    "; tags                : '(' '/' word {word} ')'                               ;\n"
    "; any_of              : '(' '*' word {word} ')'                               ;\n"
    ";\n"
    "; reassemble_rule     : reassemble_pattern\n"
    ";                     | reference\n"
    ";                     | newkey\n"
    ";                     | pre_transform_ref                                     ;\n"
    ";\n"
    "; reassemble_pattern  : '(' reassemble_terms ')'                              ;\n"
    "; reassemble_terms    : reassemble_term {reassemble_term}                     ;\n"
    "; reassemble_term     : word | punctuation | match_index                      ;\n"
    "; match_index         : integer                                               ;\n"
    "; newkey              : '(' 'NEWKEY' ')'                                      ;\n"
    "; pre_transform_ref   : '(' 'PRE' reassemble_pattern reference ')'            ;\n"
    ";\n"
    "; word                : word_char {word_char}                                 ;\n"
    "; word_char           : 'A'-'Z' | '-' | ''' (i.e. a single quote)             ;\n"
    "; punctuation         : ',' | '.'                                             ;\n"
    "; integer             : digit {digit}                                         ;\n"
    "; digit               : '0'-'9'                                               ;\n"
    "\n";


}//namespace elizascript



 //////// //       //// ////////    ///    //////// ////////  //////  //////// 
 //       //        //       //    // //      //    //       //    //    //    
 //       //        //      //    //   //     //    //       //          //    
 //////   //        //     //    //     //    //    //////    //////     //    
 //       //        //    //     /////////    //    //             //    //    
 //       //        //   //      //     //    //    //       //    //    //    
 //////// //////// //// //////// //     //    //    ////////  //////     //    

namespace elizatest { // basic test of whether this simulation is accurate



struct exchange {
    const char * prompt;    // input to ELIZA
    const char * response;  // output expected from ELIZA
};

const exchange cacm_1966_conversation[] = {

    // --- exact conversation quoted in Weizenbaum's 1966 CACM article ---

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
    // UPDATE: We now have a version of Weizenbaum's original MAD-SLIP
    // source code where we see that the word "BUT" is also considered
    // to be a delimiter. So I was wrong to assume a missing comma.
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
const int cacm_1966_conversation_size = sizeof(cacm_1966_conversation)/sizeof(cacm_1966_conversation[0]);




// return a string in ELIZA script format representing given script s
std::string to_string(const elizascript::script & s)
{
    std::string result;
    result += "(" + join(s.hello_message) + ")\n";
    for (const auto & r : s.rules)
        result += r.second->to_string();
    result += s.mem_rule->to_string();
    return result;
}


DEF_TEST_FUNC(script_test)
{
    /* Test combinations of keyword_rule

    keyword_rule : '(' keyword rule ')' ;
    keyword      : word ;
    rule         : '=' substitute_word
                 | 'DLIST' tags
                 | ['=' substitute_word]
                     ['DLIST' tags]
                     [precedence]
                     reference
                 | ['=' substitute_word]
                     ['DLIST' tags]
                     [precedence]
                     transformation {transformation}
                     [reference] ;
    */

    const char * script_text =
        "(OPENING REMARKS)\n"

        "(K00 = SUBSTITUTEWORD)\n"

        "(K01 DLIST(/TAG1 TAG2))\n"

        "(K10\n"
        "    (=REFERENCE))\n"
        "(K11 99\n"
        "    (=REFERENCE))\n"
        "(K12 DLIST(/TAG1 TAG2)\n"
        "    (=REFERENCE))\n"
        "(K13= SUBSTITUTEWORD\n"
        "    (=REFERENCE))\n"
        "(K14 DLIST(/TAG1 TAG2) 99\n"
        "    (=REFERENCE))\n"
        "(K15 =SUBSTITUTEWORD 99\n"
        "    (=REFERENCE))\n"
        "(K16=SUBSTITUTEWORD DLIST(/TAG1 TAG2)\n"
        "    (=REFERENCE))\n"
        "(K17 = SUBSTITUTEWORD DLIST(/TAG1 TAG2) 99\n"
        "    (=REFERENCE))\n"

        "(K20\n"
        "    ((DECOMPOSE (/TAG1 TAG2) PATTERN)\n"
        "        (REASSEMBLE RULE)))\n"
        "(K21 99\n"
        "    ((DECOMPOSE (*GOOD BAD UGLY) PATTERN)\n"
        "        (REASSEMBLE RULE)))\n"
        "(K22 DLIST(/TAG1 TAG2)\n"
        "    ((DECOMPOSE (*GOOD BAD) (/TAG2 TAG3) PATTERN)\n"
        "        (REASSEMBLE RULE)))\n"
        "(K23 = SUBSTITUTEWORD\n"
        "    ((DECOMPOSE PATTERN)\n"
        "        (REASSEMBLE RULE)))\n"
        "(K24 DLIST(/TAG1 TAG2) 99\n"
        "    ((DECOMPOSE PATTERN)\n"
        "        (REASSEMBLE RULE)))\n"
        "(K25 = SUBSTITUTEWORD 99\n"
        "    ((DECOMPOSE PATTERN)\n"
        "        (REASSEMBLE RULE)))\n"
        "(K26 = SUBSTITUTEWORD DLIST(/TAG1)\n"
        "    ((DECOMPOSE PATTERN)\n"
        "        (REASSEMBLE RULE)))\n"
        "(K27 = SUBSTITUTEWORD DLIST(/TAG1 TAG2) 99\n"
        "    ((DECOMPOSE PATTERN)\n"
        "        (PRE (REASSEMBLE RULE) (=K26))))\n"

        "(K30\n"
        "    ((DECOMPOSE PATTERN)\n"
        "        (REASSEMBLE RULE))\n"
        "    (= REFERENCE))\n"
        "(K31 99\n"
        "    ((DECOMPOSE PATTERN)\n"
        "        (REASSEMBLE RULE))\n"
        "    (=REFERENCE))\n"
        "(K32 DLIST(/TAG1 TAG2 TAG3)\n"
        "    ((DECOMPOSE PATTERN)\n"
        "        (REASSEMBLE RULE))\n"
        "    (=REFERENCE))\n"
        "(K33 = SUBSTITUTEWORD\n"
        "    ((DECOMPOSE PATTERN)\n"
        "        (REASSEMBLE RULE))\n"
        "    (=REFERENCE))\n"
        "(K34 DLIST(/TAG1 TAG2) 99\n"
        "    ((DECOMPOSE PATTERN)\n"
        "        (REASSEMBLE RULE))\n"
        "    (=REFERENCE))\n"
        "(K35 = SUBSTITUTEWORD 99\n"
        "    ((DECOMPOSE PATTERN)\n"
        "        (REASSEMBLE RULE))\n"
        "    (=REFERENCE))\n"
        "(K36 = SUBSTITUTEWORD DLIST(/TAG1 TAG2)\n"
        "    ((DECOMPOSE PATTERN)\n"
        "        (REASSEMBLE RULE))\n"
        "    (=REFERENCE))\n"
        "(K37 = SUBSTITUTEWORD DLIST(/TAG1 TAG2) 99\n"
        "    ((DECOMPOSE PATTERN)\n"
        "        (REASSEMBLE RULE))\n"
        "    (=REFERENCE))\n"
        "(K38 = SUBSTITUTEWORD DLIST(/TAG1 TAG2) 99\n"
        "    ((DECOMPOSE PATTERN 1)\n"
        "        (REASSEMBLE RULE A1)\n"
        "        (REASSEMBLE RULE B1)\n"
        "        (REASSEMBLE RULE C1))\n"
        "    ((DECOMPOSE PATTERN 2)\n"
        "        (REASSEMBLE RULE A2)\n"
        "        (REASSEMBLE RULE B2)\n"
        "        (REASSEMBLE RULE C2)\n"
        "        (REASSEMBLE RULE D2))\n"
        "    (=REFERENCE))\n"

        "(NONE\n"
        "    ((0)\n"
        "        (ANY NUMBER OF, BUT AT LEAST ONE, CONTEXT-FREE MESSAGES)\n"
        "        (I SEE)\n"
        "        (PLEASE GO ON)))\n"

        "(MEMORY K10\n"
        "    (0 = A)\n"
        "    (0 = B)\n"
        "    (0 = C)\n"
        "    (0 = D))\n";

    const char * recreated_script_text =
        "(OPENING REMARKS)\n"

        "(K00 = SUBSTITUTEWORD)\n"

        "(K01 DLIST(/TAG1 TAG2))\n"

        "(K10\n"
        "    (=REFERENCE))\n"
        "(K11 99\n"
        "    (=REFERENCE))\n"
        "(K12 DLIST(/TAG1 TAG2)\n"
        "    (=REFERENCE))\n"
        "(K13 = SUBSTITUTEWORD\n"
        "    (=REFERENCE))\n"
        "(K14 DLIST(/TAG1 TAG2) 99\n"
        "    (=REFERENCE))\n"
        "(K15 = SUBSTITUTEWORD 99\n"
        "    (=REFERENCE))\n"
        "(K16 = SUBSTITUTEWORD DLIST(/TAG1 TAG2)\n"
        "    (=REFERENCE))\n"
        "(K17 = SUBSTITUTEWORD DLIST(/TAG1 TAG2) 99\n"
        "    (=REFERENCE))\n"

        "(K20\n"
        "    ((DECOMPOSE (/TAG1 TAG2) PATTERN)\n"
        "        (REASSEMBLE RULE)))\n"
        "(K21 99\n"
        "    ((DECOMPOSE (*GOOD BAD UGLY) PATTERN)\n"
        "        (REASSEMBLE RULE)))\n"
        "(K22 DLIST(/TAG1 TAG2)\n"
        "    ((DECOMPOSE (*GOOD BAD) (/TAG2 TAG3) PATTERN)\n"
        "        (REASSEMBLE RULE)))\n"
        "(K23 = SUBSTITUTEWORD\n"
        "    ((DECOMPOSE PATTERN)\n"
        "        (REASSEMBLE RULE)))\n"
        "(K24 DLIST(/TAG1 TAG2) 99\n"
        "    ((DECOMPOSE PATTERN)\n"
        "        (REASSEMBLE RULE)))\n"
        "(K25 = SUBSTITUTEWORD 99\n"
        "    ((DECOMPOSE PATTERN)\n"
        "        (REASSEMBLE RULE)))\n"
        "(K26 = SUBSTITUTEWORD DLIST(/TAG1)\n"
        "    ((DECOMPOSE PATTERN)\n"
        "        (REASSEMBLE RULE)))\n"
        "(K27 = SUBSTITUTEWORD DLIST(/TAG1 TAG2) 99\n"
        "    ((DECOMPOSE PATTERN)\n"
        "        ( PRE ( REASSEMBLE RULE ) ( = K26 ) )))\n"

        "(K30\n"
        "    ((DECOMPOSE PATTERN)\n"
        "        (REASSEMBLE RULE))\n"
        "    (=REFERENCE))\n"
        "(K31 99\n"
        "    ((DECOMPOSE PATTERN)\n"
        "        (REASSEMBLE RULE))\n"
        "    (=REFERENCE))\n"
        "(K32 DLIST(/TAG1 TAG2 TAG3)\n"
        "    ((DECOMPOSE PATTERN)\n"
        "        (REASSEMBLE RULE))\n"
        "    (=REFERENCE))\n"
        "(K33 = SUBSTITUTEWORD\n"
        "    ((DECOMPOSE PATTERN)\n"
        "        (REASSEMBLE RULE))\n"
        "    (=REFERENCE))\n"
        "(K34 DLIST(/TAG1 TAG2) 99\n"
        "    ((DECOMPOSE PATTERN)\n"
        "        (REASSEMBLE RULE))\n"
        "    (=REFERENCE))\n"
        "(K35 = SUBSTITUTEWORD 99\n"
        "    ((DECOMPOSE PATTERN)\n"
        "        (REASSEMBLE RULE))\n"
        "    (=REFERENCE))\n"
        "(K36 = SUBSTITUTEWORD DLIST(/TAG1 TAG2)\n"
        "    ((DECOMPOSE PATTERN)\n"
        "        (REASSEMBLE RULE))\n"
        "    (=REFERENCE))\n"
        "(K37 = SUBSTITUTEWORD DLIST(/TAG1 TAG2) 99\n"
        "    ((DECOMPOSE PATTERN)\n"
        "        (REASSEMBLE RULE))\n"
        "    (=REFERENCE))\n"
        "(K38 = SUBSTITUTEWORD DLIST(/TAG1 TAG2) 99\n"
        "    ((DECOMPOSE PATTERN 1)\n"
        "        (REASSEMBLE RULE A1)\n"
        "        (REASSEMBLE RULE B1)\n"
        "        (REASSEMBLE RULE C1))\n"
        "    ((DECOMPOSE PATTERN 2)\n"
        "        (REASSEMBLE RULE A2)\n"
        "        (REASSEMBLE RULE B2)\n"
        "        (REASSEMBLE RULE C2)\n"
        "        (REASSEMBLE RULE D2))\n"
        "    (=REFERENCE))\n"

        "(NONE\n"
        "    ((0)\n"
        "        (ANY NUMBER OF, BUT AT LEAST ONE, CONTEXT-FREE MESSAGES)\n"
        "        (I SEE)\n"
        "        (PLEASE GO ON)))\n"

        "(MEMORY K10\n"
        "    (0 = A)\n"
        "    (0 = B)\n"
        "    (0 = C)\n"
        "    (0 = D))\n";


    elizascript::script s;
    elizascript::read(script_text, s);

    TEST_EQUAL(s.rules.size(), (size_t)28);
    TEST_EQUAL(to_string(s), recreated_script_text);
    const elizalogic::tagmap tags(elizalogic::collect_tags(s.rules));
    TEST_EQUAL(tags.size(), (size_t)3);
    TEST_EQUAL(join(tags.at("TAG1")), "K01 K12 K14 K16 K17 K22 K24 K26 K27 K32 K34 K36 K37 K38");
    TEST_EQUAL(join(tags.at("TAG2")), "K01 K12 K14 K16 K17 K22 K24 K27 K32 K34 K36 K37 K38");
    TEST_EQUAL(join(tags.at("TAG3")), "K32");


    auto read_script = [](const std::string & script_text) -> std::string {
        try {
            elizascript::script s;
            std::stringstream ss(script_text);
            elizascript::read<std::stringstream>(ss, s);
            return "success";
        }
        catch (const std::exception & e) {
            return e.what();
        }
    };

    std::string status = read_script("");
    TEST_EQUAL(status, "Script error on line 1: expected '('");
    status = read_script("(");
    TEST_EQUAL(status, "Script error on line 1: expected ')'");
    status = read_script("()");
    TEST_EQUAL(status, "Script error: no NONE rule specified; see Jan 1966 CACM page 41");
    status = read_script("()\n(");
    TEST_EQUAL(status, "Script error on line 2: expected keyword|MEMORY|NONE");
    status = read_script("()\n(NONE");
    TEST_EQUAL(status, "Script error on line 2: malformed rule");
    status = read_script("()\n(NONE\n(");
    TEST_EQUAL(status, "Script error on line 3: expected '('");
    status = read_script("()\n(NONE\n((");
    TEST_EQUAL(status, "Script error on line 3: expected ')'");
    status = read_script("()\n(NONE\n(())");
    TEST_EQUAL(status, "Script error on line 3: decompose pattern cannot be empty");
    status = read_script("()\n(NONE\n((0)()");
    TEST_EQUAL(status, "Script error on line 3: expected ')'");
    status = read_script("()\n(NONE\n((0)()");
    TEST_EQUAL(status, "Script error on line 3: expected ')'");
    status = read_script("()\n(NONE\n((0)())");
    TEST_EQUAL(status, "Script error on line 3: malformed rule");
    status = read_script("()\n(NONE\n((0)()))");
    TEST_EQUAL(status, "Script error: no MEMORY rule specified; see Jan 1966 CACM page 41");
    status = read_script("()\n(NONE\n((0)()))\r\n(MEMORY");
    TEST_EQUAL(status, "Script error on line 4: expected keyword to follow MEMORY; expected form is (MEMORY keyword (decomp1=reassm1)(decomp2=reassm2)(decomp3=reassm3)(decomp4=reassm4))");
    status = read_script("()\n(NONE\n((0)()))\r\n(MEMORY KEY");
    TEST_EQUAL(status, "Script error on line 4: expected '('; expected form is (MEMORY keyword (decomp1=reassm1)(decomp2=reassm2)(decomp3=reassm3)(decomp4=reassm4))");
    status = read_script("()\n(NONE\n((0)()))\r\n(MEMORY KEY(");
    TEST_EQUAL(status, "Script error on line 4: expected 'decompose_terms = reassemble_terms'; expected form is (MEMORY keyword (decomp1=reassm1)(decomp2=reassm2)(decomp3=reassm3)(decomp4=reassm4))");
    status = read_script("()\n(NONE\n((0)()))\r\n(MEMORY KEY(0");
    TEST_EQUAL(status, "Script error on line 4: expected '='; expected form is (MEMORY keyword (decomp1=reassm1)(decomp2=reassm2)(decomp3=reassm3)(decomp4=reassm4))");
    status = read_script("()\n(NONE\n((0)()))\r\n(MEMORY KEY(0 =");
    TEST_EQUAL(status, "Script error on line 4: expected 'decompose_terms = reassemble_terms'; expected form is (MEMORY keyword (decomp1=reassm1)(decomp2=reassm2)(decomp3=reassm3)(decomp4=reassm4))");
    status = read_script("()\n(NONE\n((0)()))\r\n(MEMORY KEY(0 = BUT YOUR 1");
    TEST_EQUAL(status, "Script error on line 4: expected ')'; expected form is (MEMORY keyword (decomp1=reassm1)(decomp2=reassm2)(decomp3=reassm3)(decomp4=reassm4))");
    status = read_script("()\n(NONE\n((0)()))\r\n(MEMORY KEY(0 = BUT YOUR 1)");
    TEST_EQUAL(status, "Script error on line 4: expected '('; expected form is (MEMORY keyword (decomp1=reassm1)(decomp2=reassm2)(decomp3=reassm3)(decomp4=reassm4))");
    status = read_script("()\n(NONE\n((0)()))\r\n(MEMORY KEY(0 = BUT YOUR 1)(0 = B)(0 = C)(0 = D)");
    TEST_EQUAL(status, "Script error on line 4: expected ')'; expected form is (MEMORY keyword (decomp1=reassm1)(decomp2=reassm2)(decomp3=reassm3)(decomp4=reassm4))");
    status = read_script("()\n(NONE\n((0)()))\r\n(MEMORY KEY(0 = BUT YOUR 1)(0 = B)(0 = C)(0 = D))");
    TEST_EQUAL(status, "Script error: MEMORY rule keyword 'KEY' is not also a keyword in its own right; see Jan 1966 CACM page 41");
    status = read_script("()\n(NONE\n((0)()))\r\n(MEMORY KEY(0 = BUT YOUR 1)(0 = B)(0 = C)(0 = D))\n(KEY((0)(TEST)))");
    TEST_EQUAL(status, "success");
    status = read_script("()\n(NONE\n((0)()))\r\n(MEMORY KEY(0 = BUT YOUR 1)(0 = B)(0 = C)(0 = D))\n(KEY((0)(TEST)))\n(K2)");
    TEST_EQUAL(status, "Script error on line 6: keyword 'K2' has no associated body");
    status = read_script("()\n(NONE\n((0)()))\r\n(memory key(0 = but your 1)(0 = b)(0 = c)(0 = d))\n(key((0)(test)))\n(k2=key)");
    TEST_EQUAL(status, "success");
    status = read_script("()\n(NONE\n((0)()))\r\n(MEMORY KEY(0 = BUT YOUR 1)(0 = B)(0 = C)(0 = D))\n(KEY((0)(TEST)))\n(K2=KEY)\n(MEMORY");
    TEST_EQUAL(status, "Script error on line 7: MEMORY rule already specified");
    status = read_script("()\n(NONE\n((0)()))\r\n(MEMORY KEY(0 = BUT YOUR 1)(0 = B)(0 = C)(0 = D))\n(KEY((0)(TEST)))\n(K2=KEY)\n(K3 ((A B C) (1 2 3)))");
    TEST_EQUAL(status, "success");
    status = read_script("()\n(NONE\n((0)()))\r\n(MEMORY KEY(0 = BUT YOUR 1)(0 = B)(0 = C)(0 = D))\n(KEY((0)(TEST)))\n(K2=KEY)\n(K3 ((A B C 0 1 2 3) (1 2 3 4 5 6 7)))");
    TEST_EQUAL(status, "success");
    status = read_script("()\n(NONE\n((0)()))\r\n(MEMORY KEY(0 = BUT YOUR 0)(0 = B)(0 = C)(0 = D))\n(KEY((0)(TEST)))\n(K2=KEY)\n(K3 ((A B C 0 1 2 3) (1 2 3 4 5 6 7)))");
    TEST_EQUAL(status, "Script error on line 4: reassembly index '0' out of range [1..1]");
    status = read_script("()\n(NONE\n((0)()))\r\n(MEMORY KEY(0 = BUT YOUR 2)(0 = B)(0 = C)(0 = D))\n(KEY((0)(TEST)))\n(K2=KEY)\n(K3 ((A B C 0 1 2 3) (1 2 3 4 5 6 7)))");
    TEST_EQUAL(status, "Script error on line 4: reassembly index '2' out of range [1..1]");
    status = read_script("()\n(NONE\n((0)()))\r\n(MEMORY KEY(0 = BUT YOUR 1)(0 X 0 = 3 2 1)(0 = C)(0 = D))\n(KEY((0)(TEST)))\n(K2=KEY)\n(K3 ((A B C 0 1 2 3) (1 2 3 4 5 6 7)))");
    TEST_EQUAL(status, "success");
    status = read_script("()\n(NONE\n((0)()))\r\n(MEMORY KEY(0 = BUT YOUR 1)(0 X 0 = 3 2 1 4)(0 = C)(0 = D))\n(KEY((0)(TEST)))\n(K2=KEY)\n(K3 ((A B C 0 1 2 3) (1 2 3 4 5 6 7)))");
    TEST_EQUAL(status, "Script error on line 4: reassembly index '4' out of range [1..3]");
    status = read_script("()\n(NONE\n((0)()))\r\n(MEMORY KEY(0 = BUT YOUR 1)(0 = B)(0 = C)(0 = D))\n(KEY((0)(TEST)))\n(K2=KEY)\n(K3 ((A B C 0 1 2 3) (0)))");
    TEST_EQUAL(status, "Script error on line 7: reassembly index '0' out of range [1..7]");
    status = read_script("()\n(NONE\n((0)()))\r\n(MEMORY KEY(0 = BUT YOUR 1)(0 = B)(0 = C)(0 = D))\n(KEY((0)(TEST)))\n(K2=KEY)\n(K3 ((A B C 0 1 2 3) (8)))");
    TEST_EQUAL(status, "Script error on line 7: reassembly index '8' out of range [1..7]");
    status = read_script("()\n(NONE\n((0)()))\r\n(MEMORY KEY(0 = BUT YOUR 1)(0 = B)(0 = C)(0 = D))\n(KEY((0)(TEST)))\n(K2=KEY)\n(K3 ((0 (/NOUN FAMILY) 0 (* CAT MAT) 0) (6)))\n(K4=K3)");
    TEST_EQUAL(status, "Script error on line 7: reassembly index '6' out of range [1..5]");
    status = read_script("()\n(NONE\n((0)()))\r\n(MEMORY KEY(0 = A)(0 = B)(0 = C)(0 = D))\n(KEY((0 KEY 0)(PRE(4)(=KEY))))\n(K4=KEY)");
    TEST_EQUAL(status, "Script error on line 5: reassembly index '4' out of range [1..3]");
}


// perform basic checks on implementation
DEF_TEST_FUNC(script_and_conversation_test)
{
    // script_text is logically identical to the script in the CACM article
    // appendix, but the ordering and whitespace is different so that it can
    // be checked against the output of elizatest::to_string()
    const char * script_text =
        "(HOW DO YOU DO. PLEASE TELL ME YOUR PROBLEM)\n"
        "(ALIKE 10\n"
        "    (=DIT))\n"
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
        "(CERTAINLY\n"
        "    (=YES))\n"
        "(CHILDREN DLIST(/FAMILY))\n"
        "(COMPUTER 50\n"
        "    ((0)\n"
        "        (DO COMPUTERS WORRY YOU)\n"
        "        (WHY DO YOU MENTION COMPUTERS)\n"
        "        (WHAT DO YOU THINK MACHINES HAVE TO DO WITH YOUR PROBLEM)\n"
        "        (DON'T YOU THINK COMPUTERS CAN HELP PEOPLE)\n"
        "        (WHAT ABOUT MACHINES WORRIES YOU)\n"
        "        (WHAT DO YOU THINK ABOUT MACHINES)))\n"
        "(COMPUTERS 50\n"
        "    (=COMPUTER))\n"
        "(DAD = FATHER DLIST(/ FAMILY))\n"
        "(DEUTSCH\n"
        "    (=XFREMD))\n"
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
        "(DREAMED = DREAMT 4\n"
        "    (=DREAMT))\n"
        "(DREAMS = DREAM 3\n"
        "    (=DREAM))\n"
        "(DREAMT 4\n"
        "    ((0 YOU DREAMT 0)\n"
        "        (REALLY, 4)\n"
        "        (HAVE YOU EVER FANTASIED 4 WHILE YOU WERE AWAKE)\n"
        "        (HAVE YOU DREAMT 4 BEFORE)\n"
        "        (=DREAM)\n"
        "        (NEWKEY)))\n"
        "(ESPANOL\n"
        "    (=XFREMD))\n"
        "(EVERYBODY 2\n"
        "    (=EVERYONE))\n"
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
        "(FRANCAIS\n"
        "    (=XFREMD))\n"
        "(HELLO\n"
        "    ((0)\n"
        "        (HOW DO YOU DO. PLEASE STATE YOUR PROBLEM)))\n"
        "(HOW\n"
        "    (=WHAT))\n"
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
        "        ( PRE ( YOU ARE 3 ) ( = I ) )))\n"
        "(IF 3\n"
        "    ((0 IF 0)\n"
        "        (DO YOU THINK ITS LIKELY THAT 3)\n"
        "        (DO YOU WISH THAT 3)\n"
        "        (WHAT DO YOU THINK ABOUT 3)\n"
        "        (REALLY, 2 3)))\n"
        "(ITALIANO\n"
        "    (=XFREMD))\n"
        "(LIKE 10\n"
        "    ((0 (*AM IS ARE WAS) 0 LIKE 0)\n"
        "        (=DIT))\n"
        "    ((0)\n"
        "        (NEWKEY)))\n"
        "(MACHINE 50\n"
        "    (=COMPUTER))\n"
        "(MACHINES 50\n"
        "    (=COMPUTER))\n"
        "(MAYBE\n"
        "    (=PERHAPS))\n"
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
        "(NOBODY 2\n"
        "    (=EVERYONE))\n"
        "(NOONE 2\n"
        "    (=EVERYONE))\n"
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
        "(SAME 10\n"
        "    (=DIT))\n"
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
        "(WERE = WAS\n"
        "    (=WAS))\n"
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
        "(WHEN\n"
        "    (=WHAT))\n"
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
        "        (=WHAT))\n"
        "    (=WHAT))\n"
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
        "        ( PRE ( I ARE 3 ) ( = YOU ) )))\n"
        "(YOUR = MY\n"
        "    ((0 MY 0)\n"
        "        (WHY ARE YOU CONCERNED OVER MY 3)\n"
        "        (WHAT ABOUT YOUR OWN 3)\n"
        "        (ARE YOU WORRIED ABOUT SOMEONE ELSES 3)\n"
        "        (REALLY, MY 3)))\n"
        "(YOURSELF = MYSELF)\n"
        "(NONE\n"
        "    ((0)\n"
        "        (I AM NOT SURE I UNDERSTAND YOU FULLY)\n"
        "        (PLEASE GO ON)\n"
        "        (WHAT DOES THAT SUGGEST TO YOU)\n"
        "        (DO YOU FEEL STRONGLY ABOUT DISCUSSING SUCH THINGS)))\n"
        "(MEMORY MY\n"
        "    (0 YOUR 0 = LETS DISCUSS FURTHER WHY YOUR 3)\n"
        "    (0 YOUR 0 = EARLIER YOU SAID YOUR 3)\n"
        "    (0 YOUR 0 = BUT YOUR 3)\n"
        "    (0 YOUR 0 = DOES THAT HAVE ANYTHING TO DO WITH THE FACT THAT YOUR 3))\n";


    elizascript::script s;
    elizascript::read(elizascript::CACM_1966_01_DOCTOR_script, s);

    TEST_EQUAL(s.rules.size(), (size_t)67);
    TEST_EQUAL(to_string(s), script_text);

    elizalogic::tagmap tags(elizalogic::collect_tags(s.rules));
    TEST_EQUAL(tags.size(), (size_t)3);
    TEST_EQUAL(join(tags["BELIEF"]), "BELIEVE FEEL THINK WISH");
    TEST_EQUAL(join(tags["FAMILY"]), "BROTHER CHILDREN DAD FATHER MOM MOTHER SISTER WIFE");
    TEST_EQUAL(join(tags["NOUN"]), "FATHER MOTHER");

    /*  I believe that if this code reproduces the exact conversation
        published in the CACM article, using the mechanisms described in
        that article, it is not unreasonable to suppose it is a fairly
        accurate simulation of the original ELIZA. */

    elizalogic::eliza eliza(s.rules, s.mem_rule);
    for (const auto & exchg : elizatest::cacm_1966_conversation)
        TEST_EQUAL(eliza.response(exchg.prompt), exchg.response);


    // how the conversation might have continued...
    // (to extend the test coverage a little)
    const exchange imagined_continuation_2023[] = {
        { "My boyfriend loves me, he's not a bully.",
          "WHY DO YOU SAY YOUR BOYFRIEND LOVES YOU" },

        { "He annoys me sometimes, but I know he cares about me.",
          "YOU SAY YOU KNOW HE CARES ABOUT YOU" },

        { "That's why he wanted me to come here, because he knows I'm unhappy.",
          "WHY DO YOU ASK" },

        { "I didn't.",
          "CAN YOU ELABORATE ON THAT" },

        { "He knows I'm not getting on with my family and thought it would help me if I talked to someone.",
          "DO YOU THINK ITS LIKELY THAT YOU TALKED TO SOMEONE" },

        { "That's why I'm talking to you.",
          "DOES THAT QUESTION INTEREST YOU" },

        { "What question?",
          "WHAT IS IT YOU REALLY WANT TO KNOW" },

        { "How to have a good relationship with my parents.",
          "DOES THAT SUGGEST ANYTHING ELSE WHICH BELONGS TO YOU" },

        { "A brighter future?",
          "I AM NOT SURE I UNDERSTAND YOU FULLY" },

        { "If I could get on better with my parents I'd be a lot happier.",
          "DO YOU WISH THAT YOU COULD GET ON BETTER WITH YOUR PARENTS I'D BE A LOT HAPPIER" },

        { "I do.",
          "DO YOU SAY YOU DO FOR SOME SPECIAL REASON" },

        { "Not really.",
          "DOES THAT HAVE ANYTHING TO DO WITH THE FACT THAT YOUR MOTHER" },

        { "I'll never be as good as my sister in my mom's eyes.",
          "TELL ME MORE ABOUT YOUR FAMILY" },

        { "My sister did better at school than me and she's always got on better with my parents.",
          "WHO ELSE IN YOUR FAMILY DID BETTER AT SCHOOL THAN YOU AND SHE'S ALWAYS GOT ON BETTER WITH YOUR PARENTS" },

        { "It's just me and my sister.",
          "YOUR SISTER" },

        { "That's right.",
          "BUT YOUR MOTHER TAKES CARE OF YOU" },

        { "She does, sure. She's not mean to me. It's just little things she does, "
            "probably without even knowing she's doing them, that make me feel stupid "
            "and not as loved as my sister.",
          "WHAT ELSE COMES TO MIND WHEN YOU THINK OF YOUR SISTER" },

        { "I don't think she likes me much either.",
          "DON'T YOU REALLY THINK SHE LIKES YOU MUCH EITHER" },

        { "She's older than me and I was always her silly little annoying sister.",
          "WERE YOU REALLY" },

        { "She thought so.",
          "BUT YOUR FATHER" },

        { "He dotes on her too.",
          "PLEASE GO ON" },
    };

    for (const auto & exchg : imagined_continuation_2023)
        TEST_EQUAL(eliza.response(exchg.prompt), exchg.response);
}


DEF_TEST_FUNC(test_alternative_men_are_all_alike_convo)
{
    const exchange alt_men_are_all_alike_convo[] = {

        /*  There is a typewritten paper in Box 8 of Weizenbaum's MIT
            archive giving the following conversation. It's similar
            to the conversation printed in the CACM ELIZA paper, but not
            identical. We don't have the ELIZA script that was used.
            Using Weizenbaum's CACM published script reproduces a similar
            but not identical conversation. The responses printed in the
            Box 8 paper that differ are commented out below.

            (The memory recalled in the last exchange suggests that the
            version of ELIZA used to create this conversation had a
            different 'certain counting mechanism', because at that
            point in the convo, LIMIT has the value 2; the ELIZA
            source we have does not print memories unless LIMIT has
            the value 4.) */

        { "Men are all alike.",
          "IN WHAT WAY" },

        { "They're always bugging us about one thing and another.",
          "CAN YOU THINK OF A SPECIFIC EXAMPLE" },

        { "Well, my boyfriend made me come here.",
          "YOUR BOYFRIEND MADE YOU COME HERE" },

        { "He noticed that I'm depressed much of the time.",
          "I AM SORRY TO HEAR YOU ARE DEPRESSED" },

        { "He's right. I am unhappy.",
          "DO YOU THINK COMING HERE WILL HELP YOU NOT TO BE UNHAPPY" },

        { "Perhaps I could learn to get along with father.",
        // WHY THE UNCERTAIN TONE
          "YOU DON'T SEEM QUITE CERTAIN" },

        { "You are like my father in some ways.",
          "WHAT RESEMBLANCE DO YOU SEE" },

        { "You are very agressive but I don't think you want me to notice that.",
          "WHAT MAKES YOU THINK I AM VERY AGRESSIVE" },

        { "These obvious attempts to steer the conversation makes me angry.",
        // DOES THAT HAVE ANYTHING TO DO WITH THE FACT THAT YOUR BOYFRIEND MADE YOU COME HERE
          "I AM NOT SURE I UNDERSTAND YOU FULLY" },
    };

    elizascript::script s;
    elizascript::read(elizascript::CACM_1966_01_DOCTOR_script, s);
    elizalogic::eliza eliza(s.rules, s.mem_rule);
    for (const auto & exchg : alt_men_are_all_alike_convo)
        TEST_EQUAL(eliza.response(exchg.prompt), exchg.response);
}


DEF_TEST_FUNC(test_busy_beaver_turing_machine)
{
    /*  4-state busy beaver

            A   B   C   D
            0   1RB 1LA 1RH 1RD
            1   1LB 0LC 1LD 0RA

        Result: 0 0 1 0 1 1 1 1 1 1 1 1 1 1 1 1 0 0 (107 steps, thirteen "1"s total)
        https://en.wikipedia.org/wiki/Busy_beaver
    */

    const char * script_text =
        "()\n"

        "(START\n"
        "    ((0)\n"
        "        (PRE (' O ') (=QA))))\n"

                                                                // state   read    write   move    state'
        "(QA\n"
        "    ((' 0) (PRE (O ' 2) (=QA)))\n"
        "    ((0 ') (PRE (1 ' O) (=QA)))\n"
        "    ((0 1 ' O ' 1 0) (PRE (1   2   I ' 6 ' 7) (=QB)))   ; QA      O       I       right   QB\n"
        "    ((0 1 ' I ' 1 0) (PRE (1 ' 2 ' I   6   7) (=QB))))  ; QA      I       I       left    QB\n"

        "(QB\n"
        "    ((' 0) (PRE (O ' 2) (=QB)))\n"
        "    ((0 ') (PRE (1 ' O) (=QB)))\n"
        "    ((0 1 ' O ' 1 0) (PRE (1 ' 2 ' I   6   7) (=QA)))   ; QB      O       I       left    QA\n"
        "    ((0 1 ' I ' 1 0) (PRE (1 ' 2 ' O   6   7) (=QC))))  ; QB      O       I       left    QC\n"

        "(QC\n"
        "    ((' 0) (PRE (O ' 2) (=QC)))\n"
        "    ((0 ') (PRE (1 ' O) (=QC)))\n"
        "    ((0 1 ' O ' 1 0) (PRE (1   2   I ' 6 ' 7) (=QHALT))); QC      O       I       right   QHALT\n"
        "    ((0 1 ' I ' 1 0) (PRE (1 ' 2 ' I   6   7) (=QD))))  ; QC      I       I       left    QD\n"

        "(QD\n"
        "    ((' 0) (PRE (O ' 2) (=QD)))\n"
        "    ((0 ') (PRE (1 ' O) (=QD)))\n"
        "    ((0 1 ' O ' 1 0) (PRE (1   2   I ' 6 ' 7) (=QD)))   ; QD      O       I       right   QD\n"
        "    ((0 1 ' I ' 1 0) (PRE (1   2   O ' 6 ' 7) (=QA))))  ; QD      O       I       right   QA\n"

        "(QHALT\n"
        "    ((0)\n"
        "        (1)))\n"

        "(TURING\n"
        "    ((0)\n"
        "        (MACHINE)))\n"

        "(MEMORY TURING\n"
        "    (0 = TURING MACHINE)\n"
        "    (0 = TURING MACHINE)\n"
        "    (0 = TURING MACHINE)\n"
        "    (0 = TURING MACHINE))\n"

        "(NONE\n"
        "    ((0)\n"
        "        (NONE)))\n";

    elizascript::script s;
    elizascript::read(script_text, s);
    elizalogic::eliza eliza(s.rules, s.mem_rule);
    TEST_EQUAL(eliza.response("START"), "O I ' O ' I I I I I I I I I I I I O");
}


DEF_TEST_FUNC(test_boston_globe_1966_convo)
{
    const exchange boston_globe_1966_conversation[] = {

        /* A conversation printed on page 15 of The Boston Globe,
           22 September 1966 by Robert L. Levey who visited MIT and talked
           to the "doctor." We don't have the ELIZA script that was used.
           Using Weizenbaum's CACM published script reproduces a similar
           but not identical conversation. The responses printed in the
           newspaper that differ are commented out below. */

        { "hello.",
          "HOW DO YOU DO. PLEASE STATE YOUR PROBLEM" },

        { "my foot hurts",
        //"TELL ME ABOUT YOUR PAINS IN GENERAL"
          "YOUR FOOT HURTS" },

        { "it aches mostly around the toes",
          "EARLIER YOU SAID YOUR FOOT HURTS" },

        { "right",
          "I AM NOT SURE I UNDERSTAND YOU FULLY" },

        { "what is your problem",
          "WHY DO YOU ASK" },

        { "because",
          "IS THAT THE REAL REASON" },

        { "no - i was simply picking on you",
        //"LET'S TRY TO GO ON"
          "WERE YOU REALLY" },

        { "what can i do about my toes",
        //"YOUR TOES"
          "WHY DO YOU SAY YOUR TOES" },

        { "they still hurt",
        //"DOES ANY PART OF YOUR BODY HURT YOU"
          "PLEASE GO ON" },

        { "some vague pains in the chest",
        //"PLEASE GO ON"
          "WHAT DOES THAT SUGGEST TO YOU" },

        { "the head bothers me too sometimes",
          "EARLIER YOU SAID YOUR TOES" },

        { "you wanted to know what else hurt me",
          "WHY DO YOU THINK I WANTED TO KNOW WHAT ELSE HURT YOU" },

        { "you are the doctor",
          "WHAT MAKES YOU THINK I AM THE DOCTOR" },

        { "the operator of this machine assured me that you were the doctor",
          "DO COMPUTERS WORRY YOU" },

        { "in general or in specific",
        //"PERHAPS YOU PREFER NOT TO DISCUSS IT"
          "DO YOU FEEL STRONGLY ABOUT DISCUSSING SUCH THINGS" },

        { "i am enjoying myself",
        //"IS IT BECAUSE YOU ARE ENJOYING YOURSELF THAT YOU CAME HERE"
          "IS IT BECAUSE YOU ARE ENJOYING YOURSELF THAT YOU CAME TO ME" },

        { "i was looking for greater enjoyment when i came to you",
        //"WERE YOU REALLY"
          "WHY DO YOU TELL ME YOU WERE LOOKING FOR GREATER ENJOYMENT WHEN YOU CAME TO I NOW" },

        { "yes - do you think i am being sarcastic",
        //"YOU SEEM QUITE SURE"
          "YOU SEEM QUITE POSITIVE" },

        { "not sure - just confident",
        //"WHAT DOES THAT SUGGEST TO YOU"
          "I AM NOT SURE I UNDERSTAND YOU FULLY" },

        { "superiority",
        //"DO YOU FEEL STRONGLY ABOUT DISCUSSING SUCH THINGS"
          "PLEASE GO ON" },

        { "more and more every moment",
        //"TELL ME MORE ABOUT SUCH FEELINGS"
          "WHAT DOES THAT SUGGEST TO YOU" },

        { "would you understand",
          "WE WERE DISCUSSING YOU - NOT ME" },

        { "i also feel you don't really want to help me",
          "YOU LIKE TO THINK I DON'T REALLY WANT TO HELP YOU - DON'T YOU" },

        { "i don't like to feel it",
          "DON'T YOU REALLY LIKE TO FEEL IT" },

        { "no",
          "ARE YOU SAYING 'NO' JUST TO BE NEGATIVE" },

        { "yes",
        //"YOU'RE PLAYING GAMES - I WON'T RESPOND UNTIL YOU QUIT"
          "YOU ARE SURE" },

        { "treat me immediately",
        //"DOES ANY PART OF YOUR BODY HURT YOU"
          "DO YOU FEEL STRONGLY ABOUT DISCUSSING SUCH THINGS" },

        { "all parts hurt and then some",
        //"WHAT DOES THAT SUGGEST TO YOU"
          "I AM NOT SURE I UNDERSTAND YOU FULLY" },

        { "maladjustment",
        //"DO YOU FEEL STRONGLY ABOUT DISCUSSING SUCH THINGS"
          "PLEASE GO ON" },

        { "no",
        //"WHY NOT"
          "YOU ARE BEING A BIT NEGATIVE" },

        { "because you are putting me on",
          "DON'T ANY OTHER REASONS COME TO MIND" },

        { "yes - you seem arrogant and silly",
        //"I UNDERSTAND"
          "I SEE" },

        { "you would",
        //"YOU'RE NOT REALLY TALKING ABOUT ME ARE YOU"
          "OH, I WOULD" },

        { "who else",
        //"I AM NOT SURE I UNDERSTAND YOU FULLY"
          "WHAT DOES THAT SUGGEST TO YOU" },

        { "i am calling you a fink",
        //"HOW LONG HAVE YOU BEEN CALLING ME A FINK"
          "HOW LONG HAVE YOU BEEN CALLING I A FINK" },

        { "since you started behaving unethically",
        //"WHAT ARE YOUR FEELINGS NOW"
          "YOU'RE NOT REALLY TALKING ABOUT ME - ARE YOU" },

        { "i despise you",
        //"PERHAPS IN YOUR FANTASY WE STILL DESPISE TOGETHER"
          "PERHAPS IN YOUR FANTASY WE DESPISE EACH OTHER" },

        { "perhaps i actually despise myself",
          "YOU DON'T SEEM QUITE CERTAIN" },

        { "i feel more unsure as the moments pass",
        //"DO YOU OFTEN FEEL MORE UNSURE AS THE MOMENTS PASS"
          "TELL ME MORE ABOUT SUCH FEELINGS" },

        { "you are losing control of your mind",
          "DOES IT PLEASE YOU TO BELIEVE I AM LOSING CONTROL OF MY MIND" },

        { "you are more to be pitied than censured",
          "DO YOU SOMETIMES WISH YOU WERE MORE TO BE PITIED THAN CENSURED" },

        { "i've had enough - goodbye",
        //"DOES ANY PART OF YOUR BODY HURT YOU"
          "DO YOU FEEL STRONGLY ABOUT DISCUSSING SUCH THINGS" },
    };

    elizascript::script s;
    elizascript::read(elizascript::CACM_1966_01_DOCTOR_script, s);
    elizalogic::eliza eliza(s.rules, s.mem_rule);
    for (const auto & exchg : boston_globe_1966_conversation)
        TEST_EQUAL(eliza.response(exchg.prompt), exchg.response);
}


DEF_TEST_FUNC(test_5_march_1965_convo)
{
    /* In Joseph Weizenbaum's MIT archive there is a folder titled
       "Conversation - March 5, 1965" containing a hand-annotated
       printout of an ELIZA conversation, along with a purple ink
       (presumably mimeographed) version of the same conversation -
       perhaps a lecture handout. See MIT archive 02-000311054.pdf.

       We don't have the ELIZA script used to generate this
       conversation. I've modified the script found in Weizenbaum's
       MIT archive 02-000311051.pdf just enough to recreate it. */

    const char * script =

    /*  Transcript of an ELIZA script printed in a listing following the ELIZA
        source code in MIT archive document 02-000311051.pdf. The listing was
        found in a folder titled COMPUTER CONVERSATIONS 1965 and has the header

            "PRINT,T0109,2531,.TAPE.,100      T0109 2531    1748.8     03/06"

        The date the listing was printed is therefore assumed to be 6 March 1965.

        This is a verbatim transcript except for whitespace, which has been
        changed for readability. In addition, changes to the script to make
        the March 5, 1965 conversation work are noted in comments. */

        "(HOW DO YOU DO.  I AM THE DOCTOR.  PLEASE SIT DOWN AT THE TYPEWRITER AND TELL ME YOUR PROBLEM.)\n"
        "\n"
        "(IF 3\n"
        "    ((0 IF 0)\n"
        "        (DO YOU THINK ITS LIKELY THAT 3)\n"
        "        (DO YOU WISH THAT 3)\n"
        "        (WHAT DO YOU THINK ABOUT 3)\n"
        "        (REALLY, 2 3)))\n"
        "\n"
        "(HOW\n"
        "    (=WHAT))\n"
        "\n"
        //[3] If WHEN links to WHAT you get the wrong response, "WHY DO YOU ASK."
        //    Remove this link and you get the required answer.
        //"(WHEN\n"
        //"    (=WHAT))\n"
        "\n"
        "(MEMORY MY\n"
        "    (0 YOUR 0 = LETS DISCUSS FURTHER WHY YOUR 3)\n"
        "    (0 YOUR 0 = EARLIER YOU SAID YOUR 3)\n"
        "    (0 YOUR 0 = BUT YOUR 3)\n"
        //[2] This one is a puzzle. The word being HASHed is "HERE". We know from the
        //    1966 script and published conversation that HASH("HERE", 2) = 3. (Assuming
        //    the same mechanism is being used in both.) So, I've just swapped this
        //    message for the (0 YOUR 0 = YOU SAID YOUR 3).
        //        "    (0 YOUR 0 = DOES THAT HAVE ANYTHING TO DO WITH THE FACT THAT YOUR 3))\n"
        "    (0 YOUR 0 = YOU SAID YOUR 3))\n"
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
        //[4] "AND MAYBE NOT" doesn't appear in the script, so I added it.
        "        (AND MAYBE NOT)\n"
        "        (YOU DON'T SEEM QUITE CERTAIN)\n"
        "        (WHY THE UNCERTAIN TONE)\n"
        "        (CAN'T YOU BE MORE POSITIVE)\n"
        "        (YOU AREN'T SURE)\n"
        "        (DON'T YOU KNOW)))\n"
        "\n"
        "(MAYBE\n"
        "    (=PERHAPS))\n"
        "\n"
        "(AM = ARE\n"
        "    ((0 ARE YOU 0)\n"
        "        (DO YOU BELIEVE YOU ARE 4)\n"
        "        (WOULD YOU WANT TO BE 4)\n"
        "        (YOU WISH I WOULD TELL YOU YOU ARE 4)\n"
        "        (WHAT WOULD IT MEAN IF YOU WERE 4))\n"
        "    ((0)\n"
        "        (WHY DO YOU SAY 'AM')\n"
        "        (I DON'T UNDERSTAND THAT)))\n"
        "\n"
        "(ARE = AM\n"
        "    ((0 AM I 0)\n"
        "        (WHY ARE YOU INTERESTED IN WHETHER I AM 4 OR NOT)\n"
        "        (WOULD YOU PREFER IF I WEREN'T 4)\n"
        "        (PERHAPS I AM 4 IN YOUR FANTASIES)\n"
        "        (DO YOU SOMETIMES THINK I AM 4))\n"
        "    ((0 AM 0)\n"
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
        "(WAS = WERE)\n"
        "(WERE = WAS)\n"
        "(ME = YOU)\n"
        "(YOU'RE = I'M)\n"
        "(I'M = YOU'RE)\n"
        "(MYSELF = YOURSELF)\n"
        "(YOURSELF = MYSELF)\n"
        "\n"
        "(MOTHER DLIST(/NOUN FAMILY))\n"
        "(FATHER DLIST(/NOUN FAMILY))\n"
        "(SISTER DLIST(/FAMILY))\n"
        "(BROTHER DLIST(/FAMILY))\n"
        "(WIFE DLIST(/FAMILY))\n"
        "(CHILDREN DLIST(/FAMILY))\n"
        "\n"
        "(I = YOU\n"
        "    ((0 YOU ARE 0 I 0)\n"
        "        (PERHAPS YOU ARE 4 SOMEONE ELSE)\n"
        "        (ARE YOU 4 ANYONE)\n"
        "        (ARE YOU 4 ANYONE IN YOUR FAMILY)\n"
        "        (PERHAPS YOU WISH I WERE 4 YOU 6))\n"
        "    ((0 YOU ARE 0)\n"
        "        (IS IT BECAUSE YOU ARE 4 THAT YOU CAME TO ME)\n"
        "        (HOW LONG HAVE YOU BEEN 4)\n"
        "        (DO YOU BELIEVE IT NORMAL TO BE 4)\n"
        "        (DO YOU ENJOY BEING 4))\n"
        "    ((0 YOU CAN'T 0)\n"
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
        "    ((0 I 0 YOU 0)\n"
        "        (WHY DO YOU THINK I 3 YOU)\n"
        "        (DID YOUR PARENTS 3 YOU))\n"
        "    ((0 I AM 0)\n"
        "        (WHAT MAKES YOU THINK I AM 4)\n"
        "        (DOES IT PLEASE YOU TO BELIEVE I AM 4)\n"
        "        (DO YOU SOMETIMES WISH YOU WERE 4)\n"
        "        (PERHAPS YOU WOULD LIKE TO BE 4))\n"
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
        "(MY = YOUR\n"
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
        "        (YOU WANT BE TO BE ABLE TO 4)\n"
        "        (PERHAPS YOU WOULD LIKE TO BE ABLE TO 4 YOURSELF))\n"
        "    ((0 CAN YOU 0)\n"
        "        (WHETHER OR NOT YOU CAN 4 DEPENDS ON YOU MORE THAN ON ME)\n"
        "        (DO YOU WANT TO BE ABLE TO 4)\n"
        "        (PERHAPS YOU DON'T WANT TO 4)))\n"
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
        "        (YOU WANT ME TO 5))\n"
        "    ((0 WHY CAN'T YOU 0)\n"
        "        (DO YOU THINK YOU SHOULD BE ABLE TO 5)\n"
        "        (DO YOU WANT TO BE ABLE TO 5)\n"
        "        (DO YOU BELIEVE THIS WILL HELP YOU TO 5)\n"
        "        (HAVE YOU ANY IDEA WHY YOU CAN'T 5))\n"
        "    (= WHAT))\n"
        "\n"
        "(EVERYONE 2\n"
        "    ((0)\n"
        //[1] JW may have added these responses at a later date, and/or the ordering
        //    of the possible responses in the script may have changed.
        //"        (REALLY, EVERYONE)\n"
        //"        (CAN YOU THINK OF ANYONE IN PARTICULAR)\n"
        //"        (WHO, FOR EXAMPLE)\n"
        //"        (YOU ARE THINKING OF A VERY SPECIAL PERSON)\n"
        "        (WHO, MAY I ASK)\n"
        "        (SOMEONE SPECIAL PERHAPS)\n"
        "        (YOU HAVE A PARTICULAR PERSON IN MIND, DON'T YOU)\n"
        "        (WHO DO YOU THINK YOU'RE TALKING ABOUT)))\n"
        "\n"
        "(EVERYBODY 2\n"
        "    (= EVERYONE))\n"
        "\n"
        "(NOBODY 2\n"
        "    (=EVERYONE))\n"
        "\n"
        "(NOONE 2\n"
        "    (=EVERYONE))\n"
        "\n"
        "(ALWAYS 1\n"
        "    ((0)\n"
        "        (CAN YOU THINK OF A SPECIFIC EXAMPLE)\n"
        "        (WHEN)\n"
        "        (WHAT INCIDENT ARE YOU THINKING OF)\n"
        "        (REALLY, ALWAYS)))\n"
        "\n"
        "()\n";

    const exchange conversation[] = {

        // --- exact conversation from Weizenbaum's MIT archive dated 5 March 1965 ---

        { "Doctor, I am terribly depressed.",
          "IS IT BECAUSE YOU ARE TERRIBLY DEPRESSED THAT YOU CAME TO ME" },

        { "Actually, my wife suggested I come here.",
          "TELL ME MORE ABOUT YOUR FAMILY" },

        { "I have no children.",
          "YOU SAY YOU HAVE NO CHILDREN" },

        { "I can't resign myself to fatherhood.",
          "HOW DO YOU KNOW YOU CAN'T RESIGN YOURSELF TO FATHERHOOD" },

        { "Well, my wife wants kids but I don't. That's all I can say about it.",
          "WHO ELSE IN YOUR FAMILY WANTS KIDS" },

        { "Everybody is always hinting and bugging me about it.",               //[1]
          "WHO, MAY I ASK" },

        { "My father talks about grandchildren all the time.",
          "YOUR FATHER" },

        { "He pokes his nose into our affairs much too much.",
          "I AM NOT SURE I UNDERSTAND YOU FULLY" },

        { "He's always trying to run the house.",
          "CAN YOU THINK OF A SPECIFIC EXAMPLE" },

        { "Apart from the children thing, my father wants me to change jobs.",
          "WHAT ELSE COMES TO MIND WHEN YOU THINK OF YOUR FATHER" },

        { "Nothing.",                                                           //[2]
          "YOU SAID YOUR WIFE SUGGESTED YOU COME HERE" },

        { "Yes.",
          "YOU SEEM QUITE POSITIVE" },

        { "She said that I should either get help or she would leave me.",
          "CAN YOU ELABORATE ON THAT" },

        { "My wife can't stand dad any longer.",
          "TELL ME MORE ABOUT YOUR FAMILY" },

        { "Mother is dead.",
          "EARLIER YOU SAID YOUR WIFE WANTS KIDS" },

        { "That's right. Can you understand how that bugs me.",
          "YOU BELIEVE I CAN UNDERSTAND HOW THAT BUGS YOU DON'T YOU" },

        { "I can't take it much longer.",
          "HAVE YOU TRIED" },

        { "I'm always patient.",
          "WHEN" },

        { "When she yells at me.",                                              //[3]
          "LETS DISCUSS FURTHER WHY YOUR FATHER TALKS ABOUT GRANDCHILDREN ALL THE TIME" },

        { "He's getting old. I guess he wants to be immortal.",
          "DO YOU SAY YOU GUESS HE WANTS TO BE IMMORTAL FOR SOME SPECIAL REASON" },

        { "He also talks about dying a lot.  Because he is sick, you know.",
          "IS THAT THE REAL REASON" },

        { "Maybe.",                                                             //[4]
          "AND MAYBE NOT" },

        { "I think he needs help more than I do.",
          "THAT'S QUITE INTERESTING" },

        { "I can't help him. I've tried all my life.",
          "PERHAPS YOU COULD HELP HIM NOW" },

        { "What do you mean.",
          "WHY DO YOU ASK" },

        { "I don't understand you.",
          "DON'T YOU REALLY UNDERSTAND I" }, // JW preceded this with a P in the margin

        { "No.",
          "ARE YOU SAYING 'NO' JUST TO BE NEGATIVE" },

        { "Are you suggesting its all my fault.",
          "WHY ARE YOU INTERESTED IN WHETHER I AM SUGGESTING ITS ALL YOUR FAULT OR NOT" },

        { "You are the expert, after all. I have to listen to you.",
          "WHAT MAKES YOU THINK I AM THE EXPERT" },

        { "Or are you a computer.",
          "WOULD YOU PREFER IF I WEREN'T A COMPUTER" },

        { "I don't trust computers. Anyway, no computer could talk as you do.",
          "WHY DON'T YOU TRUST COMPUTERS" },

        { "Because they're hardly human, that's why.",
          "DON'T ANY OTHER REASONS COME TO MIND" },

        // --- end of 5 March 1965 conversation from Weizenbaum's MIT archive ---
    };

    elizascript::script s;
    elizascript::read(script, s);
    elizalogic::eliza eliza(s.rules, s.mem_rule);

    for (const auto & exchg : conversation)
        TEST_EQUAL(eliza.response(exchg.prompt), exchg.response);
}


DEF_TEST_FUNC(test_02_000311052_pdf_convo)
{
    const exchange conversation[] = {

        // Conversation from MIT archive 02-000311052.pdf.

        { "Doctor, I have terrible news..",
          "YOU SAY YOU HAVE TERRIBLE NEWS" },

        { "Yes, there is a computer program at M.I.T. that'll ruin us..",
          "WHY DO YOU SAY THAT" },

        { "I have seen it working..",
          "YOU SAY YOU HAVE SEEN IT WORKING" },

        { "Its frightening - you must see it..",
          "YOU THINK I MUST SEE IT" },

        { "It listens to patients chatter just as you and I listen..",
          "YOU THINK I AND YOU LISTEN" },

        { "No, not usually - no need to get nasty though..",
          "WHY DO YOU SAY THAT" },

        { "Someone might hear us - don't you care..",
          "YOU THINK I CARE" },

        { "You sound depressed to me..",
          "IN YOUR PHANTASY - PERHAPS WE SOUND DEPRESSED TO EACH OTHER" },

        { "Yes - perhaps..",
          "I UNDERSTAND" },

        { "I do too..",
          "YOU SAY YOU DO TOO" },

        { "We are both in the same boat with respect to that computer..",
          "LETS CONTINUE TO TALK ABOUT YOU - SHALL WE" },

        { "You are a comfort..",
          "WHAT MAKES YOU THINK I AM A COMFORT" },

        { "At a time like this you want to talk about me..",
          "WHY DO YOU THINK I WANT TO TALK ABOUT YOU" },

        { "Because you share my feelings about that damned computer..",
          "WHAT DOES THAT SUGGEST TO YOU" },

        { "That, of course, you hate computers too..",
          "YOU THINK I HATE COMPUTERS TOO" },

        { "Why not, they're hardly human, are they..",
          "PERHAPS THATS WHAT WE ARE HERE TO FIND OUT" },

        { "No computer could converse as you can, for example..",
          "OH" },

        { "I cant understand how anyone could be fooled by a mere machine..",
          "CANT YOU REALLY" },

        { "No, after all there is nothing creative about a computer program..",
          "YOU ARE BEING NEGATIVE" },

        { "I am convinced I am correct..",
          "ARE YOU WORRIED TO HAVE BEEN CONVINCED YOU ARE CORRECT" },

        { "No, not in the least. Not at all. Not even a little..",
          "YOU ARE BEING NEGATIVE" },

        { "You said you agreed with me, didn't you..",
          "IS THAT WHAT YOU HEARD ME SAY" },

        { "Why dont you try to dissuade me - computers diddle with numbers, that's all..",
          "YOU WISH ME TO TRY TO DISSUADE YOU" },

        { "You cant..",
          "YOU THINK I CANT" },

        { "For example, could a computer in a conversation like this raise a new topic..",
          "YOU SEEM TO HAVE AVOIDED SPEAKING OF YOUR PARENTS ALTOGETHER" },

        { "Precisely as you just did..",
          "YOU THINK I JUST DID" },

        { "Yes, and that's what shows the difference between a man and a computer..",
          "I SEE" }
    };

    // We don't have the ELIZA script used to generate this conversation.
    // Maybe one day we'll find it. I'm curious to know how brittle it is,
    // or not. For example, is that YOU SEEM TO HAVE AVOIDED SPEAKING OF
    // YOUR PARENTS ALTOGETHER just the next NONE response?
}


DEF_TEST_FUNC(test_11_june_1964_prof_student_convo)
{
     const char * script =

    /*  Transcript of an ELIZA script printed in a listing following the ELIZA
        source code in MIT archive document 02-000311051.pdf. The listing was
        found in a folder titled COMPUTER CONVERSATIONS 1965 and has the header

            "PRINT,T0109,2531,.TAPE.,100      T0109 2531    1748.8     03/06"

        The date the listing was printed is therefore assumed to be 6 March 1965.

        This is a verbatim transcript except for whitespace, which has been
        changed for readability. In addition, changes to the script to make
        the March 5, 1965 conversation work are noted in comments. */

        "(HOW DO YOU DO.  I AM THE DOCTOR.  PLEASE SIT DOWN AT THE TYPEWRITER AND TELL ME YOUR PROBLEM.)\n"
        "\n"
        "(IF 3\n"
        "    ((0 IF 0)\n"
        "        (DO YOU THINK ITS LIKELY THAT 3)\n"
        "        (DO YOU WISH THAT 3)\n"
        "        (WHAT DO YOU THINK ABOUT 3)\n"
        "        (REALLY, 2 3)))\n"
        "\n"
        //"(HOW\n"
        //"    (=WHAT))\n"
        //"\n"
        //[3] If WHEN links to WHAT you get the wrong response, "WHY DO YOU ASK."
        //    Remove this link and you get the required answer.
        //"(WHEN\n"
        //"    (=WHAT))\n"
        "\n"
        "(MEMORY MY\n"
        "    (0 YOUR 0 = LETS DISCUSS FURTHER WHY YOUR 3)\n"
        "    (0 YOUR 0 = EARLIER YOU SAID YOUR 3)\n"
        "    (0 YOUR 0 = LETS EXPLORE FURTHER WHY YOUR 3)\n"
        //[2] This one is a puzzle. The word being HASHed is "HERE". We know from the
        //    1966 script and published conversation that HASH("HERE", 2) = 3. (Assuming
        //    the same mechanism is being used in both.) So, I've just swapped this
        //    message for the (0 YOUR 0 = YOU SAID YOUR 3).
        //        "    (0 YOUR 0 = DOES THAT HAVE ANYTHING TO DO WITH THE FACT THAT YOUR 3))\n"
        "    (0 YOUR 0 = YOU SAID YOUR 3))\n"
        "\n"
        "(NONE\n"
        "    ((0)\n"
        "        (THAT IS INTERESTING)"
        "        (HMMM)"
        "        (HMMM)"
        "        (I AM NOT SURE I UNDERSTAND YOU FULLY)\n"
        "        (PLEASE GO ON)\n"
        "        (WHAT DOES THAT SUGGEST TO YOU)\n"
        "        (DO YOU FEEL STRONGLY ABOUT DISCUSSING SUCH THINGS)))\n"
        "\n"
        "(PERHAPS\n"
        "    ((0)\n"
        "        (IS THAT THE REAL REASON)\n"
        "        (YOU DON'T SEEM QUITE CERTAIN)\n"
        "        (WHY THE UNCERTAIN TONE)\n"
        "        (CAN'T YOU BE MORE POSITIVE)\n"
        "        (YOU AREN'T SURE)\n"
        "        (DON'T YOU KNOW)))\n"
        "\n"
        //"(MAYBE\n"
        //"    (=PERHAPS))\n"
        "\n"
        "(AM = ARE\n"
        "    ((0 ARE YOU 0)\n"
        "        (DO YOU BELIEVE YOU ARE 4)\n"
        "        (WOULD YOU WANT TO BE 4)\n"
        "        (YOU WISH I WOULD TELL YOU YOU ARE 4)\n"
        "        (WHAT WOULD IT MEAN IF YOU WERE 4))\n"
        "    ((0)\n"
        "        (WHY DO YOU SAY 'AM')\n"
        "        (I DON'T UNDERSTAND THAT)))\n"
        "\n"
        "(ARE = AM\n"
        "    ((0 AM I 0)\n"
        "        (WHY ARE YOU INTERESTED IN WHETHER I AM 4 OR NOT)\n"
        "        (WOULD YOU PREFER IF I WEREN'T 4)\n"
        "        (PERHAPS I AM 4 IN YOUR FANTASIES)\n"
        "        (DO YOU SOMETIMES THINK I AM 4))\n"
        "    ((0 AM 0)\n"
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
        "(WAS = WERE)\n"
        "(WERE = WAS)\n"
        "(ME = YOU)\n"
        "(CANT = CAN'T)\n"
        //"(DONT = DON'T)\n"
        "(YOU'RE = I'M)\n"
        "(I'M = YOU'RE)\n"
        "(MYSELF = YOURSELF)\n"
        "(YOURSELF = MYSELF)\n"
        "\n"
        "(MOTHER DLIST(/NOUN FAMILY))\n"
        "(FATHER DLIST(/NOUN FAMILY))\n"
        "(SISTER DLIST(/FAMILY))\n"
        "(BROTHER DLIST(/FAMILY))\n"
        "(WIFE DLIST(/FAMILY))\n"
        "(CHILDREN DLIST(/FAMILY))\n"
        "\n"
        "(I = YOU\n"
        "    ((0 YOU ARE 0 I 0)\n"
        "        (PERHAPS YOU ARE 4 SOMEONE ELSE)\n"
        "        (ARE YOU 4 ANYONE)\n"
        "        (ARE YOU 4 ANYONE IN YOUR FAMILY)\n"
        "        (PERHAPS YOU WISH I WERE 4 YOU 6))\n"
        "    ((0 YOU ARE 0)\n"
        "        (IS IT BECAUSE YOU ARE 4 THAT YOU ARE COMING TO ME)\n"
        "        (HOW LONG HAVE YOU BEEN 4)\n"
        "        (DO YOU BELIEVE IT NORMAL TO BE 4)\n"
        "        (DO YOU ENJOY BEING 4))\n"
        "    ((0 YOU CAN'T 0)\n"
        "        (HAVE YOU TRIED)\n"
        "        (HOW DO YOU KNOW YOU CAN'T 4)\n"
        "        (PERHAPS YOU COULD 4 NOW)\n"
        "        (DO YOU REALLY WANT TO BE ABLE TO 4))\n"
        "    ((0 YOU DONT 0)\n"
        "        (DO YOU WANT TO 4 ME)\n"
        "        (WHY DON'T YOU 4)\n"
        "        (DO YOU WISH TO BE ABLE TO 4)\n"
        "        (DOES THAT TROUBLE YOU))\n"
        "    ((0 YOU FEEL 0)\n"
        "        (TELL ME MORE ABOUT SUCH FEELINGS)\n"
        "        (DO YOU OFTEN FEEL 4)\n"
        "        (DO YOU ENJOY FEELING 4)\n"
        "        (OF WHAT DOES FEELING 4 REMIND YOU))\n"
        "    ((0 YOU 0 I 0)\n"
        "        (CLEARLY YOU WANT TO 3 ME))\n"
        "    ((0 YOU 0)\n"
        "        (YOU SAY YOU 3)))\n"
        "\n"
        "(YOU = I\n"
        "    ((0 I 0 YOU 0)\n"
        "        (PERHAPS YOU 3 ME))\n"
        //"        (DID YOUR PARENTS 3 YOU))\n"
        "    ((0 I AM 0)\n"
        "        (WHAT MAKES YOU THINK I AM 4))\n"
        "    ((0 I 0)\n"
        "        (YOU THINK I 3)))\n"
        "\n"
        "(YES\n"
        "    ((0)\n"
        "        (I SEE)\n"
        "        (I SEE)\n"
        "        (I SEE)\n"
        "        (AHA)))\n"
        "\n"
        "(NO\n"
        "    ((0)\n"
        "        (OH)\n"
        "        (ARE YOU SAYING 'NO' JUST TO BE NEGATIVE)\n"
        "        (YOU ARE BEING A BIT NEGATIVE)\n"
        "        (WHY NOT)\n"
        "        (WHY 'NO')))\n"
        "\n"
        "(MY = YOUR\n"
        "    ((0 YOUR 0 (/FAMILY) 0)\n"
        "        (TELL ME MORE ABOUT YOUR FAMILY)\n"
        "        (WHO ELSE IN YOUR FAMILY 5)\n"
        "        (YOUR 4)\n"
        "        (WHAT ELSE COMES TO MIND WHEN YOU THINK OF YOUR 4))\n"
        "    ((0 YOUR 0)\n"
        "        (WHY DO YOU THINK YOUR 3)\n"
        "        (REALLY - YOUR 3)\n"
        "        (WHY DO YOU SAY YOUR 3)\n"
        "        (DOES THAT SUGGEST ANYTHING ELSE WHICH BELONGS TO YOU)\n"
        "        (IS IT IMPORTANT TO YOU THAT 2 3)))\n"
        "\n"
        "(CAN\n"
        "    ((0 CAN I 0)\n"
        "        (YOU BELIEVE I CAN 4 - DONT YOU)\n"
        "        (YOU WANT BE TO BE ABLE TO 4)\n"
        "        (PERHAPS YOU WOULD LIKE TO BE ABLE TO 4 YOURSELF))\n"
        "    ((0 CAN YOU 0)\n"
        "        (WHETHER OR NOT YOU CAN 4 DEPENDS ON YOU MORE THAN ON ME)\n"
        "        (DO YOU WANT TO BE ABLE TO 4)\n"
        "        (PERHAPS YOU DON'T WANT TO 4)))\n"
        "\n"
        /*"(WHAT\n"
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
        "\n"*/
        "(BECAUSE\n"
        "    ((0)\n"
        "        (IS THAT THE REAL REASON)\n"
        "        (DON'T ANY OTHER REASONS COME TO MIND)\n"
        "        (DOES THAT REASON SEEM TO EXPLAIN ANYTHING ELSE)\n"
        "        (WHAT OTHER REASONS MIGHT THERE BE)))\n"
        "\n"
        "(WHY\n"
        "    ((0 WHY DONT I 0 YOU 0)\n"
        "        (YOU WISH ME TO 5 YOU)\n"
        "        (PERHAPS I WILL 5 IN GOOD TIME)\n"
        "        (SHOULD YOU 5 YOURSELF)\n"
        "        (YOU WANT ME TO 5))\n"
        "    ((0 WHY CAN'T YOU 0)\n"
        "        (DO YOU THINK YOU SHOULD BE ABLE TO 5)\n"
        "        (DO YOU WANT TO BE ABLE TO 5)\n"
        "        (DO YOU BELIEVE THIS WILL HELP YOU TO 5)\n"
        "        (HAVE YOU ANY IDEA WHY YOU CAN'T 5))\n"
        "    ((0 WHY DO I 0)\n"
        "        (YOU IMAGINE THAT I 5))\n"
        "    ((0)\n"
        "        (PERHAPS THATS WHAT WE ARE HERE TO FIND OUT)))\n"
        "\n"
        "(EVERYONE 2\n"
        "    ((0)\n"
        //[1] JW may have added these responses at a later date, and/or the ordering
        //    of the possible responses in the script may have changed.
        //"        (REALLY, EVERYONE)\n"
        //"        (CAN YOU THINK OF ANYONE IN PARTICULAR)\n"
        //"        (WHO, FOR EXAMPLE)\n"
        //"        (YOU ARE THINKING OF A VERY SPECIAL PERSON)\n"
        "        (WHO, MAY I ASK)\n"
        "        (SOMEONE SPECIAL PERHAPS)\n"
        "        (YOU HAVE A PARTICULAR PERSON IN MIND, DON'T YOU)\n"
        "        (WHO DO YOU THINK YOU'RE TALKING ABOUT)))\n"
        "\n"
        "(EVERYBODY 2\n"
        "    (= EVERYONE))\n"
        "\n"
        "(NOBODY 2\n"
        "    (=EVERYONE))\n"
        "\n"
        "(NOONE 2\n"
        "    (=EVERYONE))\n"
        "\n"
        "(ALWAYS 1\n"
        "    ((0)\n"
        "        (CAN YOU THINK OF A SPECIFIC EXAMPLE)\n"
        "        (WHEN)\n"
        "        (WHAT INCIDENT ARE YOU THINKING OF)\n"
        "        (REALLY, ALWAYS)))\n"
        "\n"
        "()\n";

    const exchange conversationx[] = {

        /*  Conversation from MIT archive 02-000311056.pdf titled

                Computer Output - June 11, 1964 - Program HELP

            The lines were bracketed and prefixed with either STUDENT:
            or PROF: e.g.

                STUDENT:  (WHY DO YOU INSULT ME)

                PROF:     (YOU IMAGINE THAT I INSULT YOU)
        */

        { "I AM HAVING TROUBLE WRITING PROGRAMS",
          "IS IT BECAUSE YOU ARE HAVING TROUBLE WRITING PROGRAMS THAT YOU ARE COMING TO ME" },

        { "YES",
          "I SEE" },

        { "MY PROGRAMS CONFUSE THE WORDS YOU AND ME",
          "WHY DO YOU THINK YOUR PROGRAMS CONFUSE THE WORDS I AND YOU" },

        { "I DONT KNOW, - YOU SEE WHAT I MEAN THOUGH", //////////////////////////////////
          "DO YOU WANT TO KNOW ME" },

        { "WELL, THAT ISNT REALLY WHAT I'M HERE FOR",
          "THAT IS INTERESTING" },

        { "APART FROM THAT, CAN YOU HELP ME",
          "YOU BELIEVE I CAN HELP YOU - DONT YOU" },

        { "I TRUST YOU CAN HELP, - YOU'VE BEEN HIGHLY RECOMMENDED",
          "CLEARLY YOU WANT TO TRUST ME" },

        { "YOU SOUND MORE LIKE A PSYCHIATRIST THAN A PROGRAMMER",
          "YOU THINK I SOUND MORE LIKE A PSYCHIATRIST THAN A PROGRAMMER" },

        { "EITHER THAT, OR YOU DONT KNOW MUCH ABOUT COMPUTERS",
          "YOU THINK I DONT KNOW MUCH ABOUT COMPUTERS" },

        { "THAT SUSPICION IS BEGINNING TO COME TO MY MIND",
          "REALLY - YOUR MIND" },

        { "WHY DO YOU INSULT ME",
          "YOU IMAGINE THAT I INSULT YOU" },

        { "WELL THAT CRACK SET ME OFF A LITTLE",
          "HMMM" },

        { "YOU ARE A PROGRAMMER",
          "WHAT MAKES YOU THINK I AM A PROGRAMMER" },

        { "YOU'RE RIGHT, THERE ISN'T MUCH EVIDENCE FOR IT, - IS THERE",
          "LETS EXPLORE FURTHER WHY YOUR PROGRAMS CONFUSE THE WORDS I AND YOU" },

        { "PERHAPS ITS BECAUSE THEY HAVE SIMILAR BIT PATTERNS OR SOMETHING",
          "IS THAT THE REAL REASON" },

        { "I DOUBT IT",
          "YOU SAY YOU DOUBT IT" },

        { "WELL, AS YOU KNOW, COMPUTERS DONT WORK LIKE THAT",
        //"YOU THINK I KNOW COMPUTERS DONT WORK LIKE THAT"
          "YOU THINK I KNOW" },

        { "NO - I FINALLY DECIDED YOU DONT KNOW ANYTHING ABOUT HOW COMPUTERS WORK",
          "OH" },

        { "I'M CONVINCED YOU'RE QUITE SICK, YOU NEED HELP",
          "YOU THINK I NEED HELP" },

        { "YES INDEED, - I KNOW OF A COMPUTER PROGRAM THAT CAN CURE YOU",
          "I SEE" },

        { "YOU SHOULD GO TO SEE THE PEOPLE AT PROJECT MAC",
          "YOU THINK I SHOULD GO TO SEE THE PEOPLE AT PROJECT MAC" },

        { "BUT DONT TRY TO TELL THEM YOU ARE A COMPUTER EXPERT",
          "WHAT MAKES YOU THINK I AM A COMPUTER EXPERT" },

        { "NOTHING, - ABSOLUTELY NOTHING",
          "HMMM" },

        { "YOU ARE DISAPPOINTED",
          "WHAT MAKES YOU THINK I AM DISAPPOINTED" },

        { "THE WAY YOU PLEAD WITH ME",
          "PERHAPS YOU PLEAD WITH ME" },

        { "I CANT FOLLOW YOUR REASONING",
          "HAVE YOU TRIED" },

        { "I HAVE",
          "YOU SAY YOU HAVE" },

        { "YES, - BUT IT DOESN'T HELP MUCH",
          "I SEE" },

        { "WHY DONT YOU COME WITH ME TO SEE THAT LOVELY PROGRAM AT PROJECT MAC",
          "YOU WISH ME TO COME WITH YOU" },

        { "YES, - I'M SURE WE WILL BOTH FEEL BETTER AFTER YOU'VE HAD SOME REAL THERAPY",
          "AHA" },

        { "THERE, - YOU FEEL BETTER ALREADY, - COME ON NOW",
        // the conversation ended at this point with the PROF's response not shown
          "YOU THINK I FEEL BETTER ALREADY - COME ON NOW" },
    };


    const exchange conversation[] = {

        { "I AM VERY DEPRESSED",
          "HOW LONG HAVE YOU BEEN VERY DEPRESSED" },

        { "EVER SINCE MY DAD STARTED DRINKING",
          "REALLY - YOUR DAD STARTED DRINKING" },

        { "YES",
          "I UNDERSTAND" },

        { "HOW CAN YOU UNDERSTAND",
          "YOU BELIEVE I CAN UNDERSTAND - DONT YOU" },

        { "WHY SHOULD I BELIEVE IN ANYONE",
          "PERHAPS THATS WHAT WE ARE HERE TO FIND OUT" },

        { "MAYBE YOU ARE AN ALCOHOLIC",
          "WHAT MAKES YOU THINK I AM AN ALCOHOLIC" },

        { "YOU SEEM A LOT LIKE MY FATHER",
          "YOU THINK I SEEM A LOT LIKE YOUR FATHER" },

        { "WELL, NO - NOT REALLY",
          "WHY DO YOU SAY THAT" },

        { "FOR ONE THING - YOU DONT YELL AT ME",
          "PERHAPS YOU DONT YELL AT ME" },

        { "I DONT YELL AT YOU - OR ANYONE, FOR THAT MATTER",
          "DO YOU WANT TO YELL AT ME" },

        { "I CANT GET ANGRY WITH ANYONE",
          "HAVE YOU TRIED" },

        { "ONLY SECRETLY - IN MY DREAMS",
          "REALLY - YOUR DREAMS" },

        { "I DREAM THE WILDEST DREAMS",
          "YOU SAY YOU DREAM THE WILDEST DREAMS" },

        { "I FEEL VERY POWERFUL - IN DREAMS, THAT IS",
          "TELL ME MORE ABOUT SUCH FEELINGS" },

        { "THEY FRIGHTEN ME - I WAKE UP SCREAMING",
          "YOU SAY YOU WAKE UP SCREAMING" },

        { "MY WHOLE BODY SHAKES",
          "REALLY - YOUR WHOLE BODY SHAKES" },

        { "YES - ITS ALWAYS A TERRIBLE EXPERIENCE",
          "AHA" },

        { "WHY DO YOU MAKE FUN OF ME",
          "YOU IMAGINE THAT I MAKE FUN OF YOU" },

        { "NO - I KNOW YOU WERE LAUGHING AT ME",
          "OH" },

        { "WHY DONT YOU PRESCRIBE SOMETHING FOR ME SO I CAN RELAX",
          "YOU WISH ME TO PRESCRIBE SOMETHING FOR YOU" },

        { "ISNT THAT WHAT A DOCTOR IS SUPPOSED TO DO",
          "LETS EXPLORE FURTHER WHY YOUR DAD STARTED DRINKING" },

        { "HE STARTED JUST AFTER MY MOTHER LEFT HOME",
          "REALLY - YOUR MOTHER LEFT HOME" },

        { "BECAUSE OF THE DRINKING AND ALL, I GUESS",
          "LETS EXPLORE FURTHER WHY YOUR MOTHER LEFT HOME" },

        { "ACTUALLY - DAD HAD STARTED TO RUN AROUND",
          "HMMM" },

        { "SINCE THEN, I CANT GET MY FEELINGS ABOUT HIM STRAIGHT",
          "PERHAPS YOU COULD GET YOUR FEELINGS ABOUT HIM STRAIGHT IF YOU SAW YOUR PROBLEMS MORE REALISTICALLY" },

        { "CAN YOU HELP ME",
          "YOU BELIEVE I CAN HELP YOU - DONT YOU" },

        { "YOU SAID YOU UNDERSTOOD ME AND MY DEPRESSION",
          "IS THAT WHAT YOU HEARD ME SAY" },

        { "I'M NOT SURE NOW - YOU ARE ANGRY WITH ME",
          "WHY DO YOU THINK I AM ANGRY WITH YOU" },

        { "YOU ACCUSED ME OF NOT LISTENING",
          "YOU THINK I ACCUSED YOU OF NOT LISTENING" },

        { "YES - AND IT UPSETS ME",
          "I SEE" },

        { "YOU REMIND ME OF MY FATHER",
          "YOU THINK I REMIND YOU OF YOUR FATHER" },

        { "YOU'RE YELLING AT ME JUST AS HE DID",
          "THAT IS INTERESTING" },

        { "I DONT KNOW HOW TO EXPRESS MY FEELINGS TO YOU",
          "DO YOU WANT TO KNOW HOW TO EXPRESS YOUR FEELINGS TO ME" },

        { "YES - ITS ANGER - I AM FINALLY ANGRY",
          "I UNDERSTAND" },

        { "YOU'RE A WONDERFUL DOCTOR - YOU CURED ME",
          "PERHAPS YOU CURED ME" },

        { "I'M NOT ANGRY ANYMORE - BACK TO THE COMPUTER FACTORY FOR ME",
          "WHAT ELSE DOES THAT SUGGEST TO YOU" },

        { "IT FEELS GOOD NOT TO BE ANGRY AND ALSO BE CURED",
          "THAT IS INTERESTING" },

        { "NOW THAT I UNDERSTAND YOU AND DAD, I GUESS ITS TIME FOR A DRINK",
          "CLEARLY YOU WANT TO UNDERSTAND ME" },

        { "AND I DO - BELIEVE ME - SO LONG NOW",
        // the conversation ended at this point with the DOCTOR's response not shown
          "YOU SAY YOU DO" },
    };




    elizascript::script s;
    elizascript::read(script, s);
    elizalogic::eliza eliza(s.rules, s.mem_rule);

    /*  It seems likely that the version of ELIZA used to generate
        this conversation recognised dash as a subclause-separator.
        For example, in the exchange

            THEY FRIGHTEN ME - I WAKE UP SCREAMING
            YOU SAY YOU WAKE UP SCREAMING

        if dash is not a delimiter one would expect the response to be

            YOU SAY THEY FRIGHTEN YOU - YOU WAKE UP SCREAMING
    */
    eliza.set_delimeters({ "-", ",", ".", "BUT" });

    //eliza.set_use_limit(false);
    elizalogic::string_tracer trace;
    eliza.set_tracer(&trace);

    /*for (const auto & exchg : conversation) {
        std::cout << exchg.prompt << '\n';
        //TEST_EQUAL(eliza.response(exchg.prompt), exchg.response);
        const std::string response(eliza.response(exchg.prompt));
        std::cout << trace.text();
        std::cout << response << '\n';
        TEST_EQUAL(response, exchg.response);
    }*/
}


}//namespace elizatest



void sleep_ms(long ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
};


// write given s to std::cout, followed by newline
void writeln(const std::string & s)
{
    /* For fun, output given s at 14 characters per second,
       the speed of an IBM 2741 teletypewriter from 1965.
       In an interview with Pamela McCorduck, recorded
       on 6 March 1975, Weizenbaum talks of the terminal
       he had in his home:

       "Then I came to MIT in '63 and the next spectacular
       thing was of course ELIZA. And the history of that
       is interesting too. Soon after I came here, very soon -
       like within a couple of months I think - I was given
       a console, a computer console, at home. I lived in
       Concord - still do. It was at the time a 2741 tied
       to a 7094 CTSS system here."

       See the Carnegie Mellon University archives file
       mccorduck_weizenbaum_1975_03_06_001_a_access.mp3
       at 17:30. */

    const long cps = 14; // the IBM 2741 printed at ~14.1 cps

    for (const auto c : s) {
        std::cout << c << std::flush;
        sleep_ms(1000/cps);
    }

    std::cout << std::endl;
}


#if defined(_WIN32)
const std::string option_escape("/");
#else
const std::string option_escape("--");
#endif


bool is_option(const std::string & s)
{
    return s.compare(0, option_escape.size(), option_escape) == 0;
}


std::string as_option(const std::string & o)
{
    return option_escape + o;
}


std::string pad(std::string s)
{
    const size_t width = 16;
    if (s.size() < width)
        s += std::string(width - s.size(), ' ');
    return s;
}


bool parse_cmdline(
    int argc, const char * argv[],
    bool & showscript,
    bool & nobanner,
    bool & quick,
    bool & help,
    bool & port,
    std::string & port_name,
    std::string & script_filename)
{
    showscript = nobanner = help = port = false;
    quick = true;
    script_filename.clear();
    for (int i = 1; i < argc; ++i) {
        if (is_option(argv[i])) {
            if (as_option("help") == argv[i])
                help = true;
            else if (as_option("showscript") == argv[i])
                showscript = true;
            else if (as_option("nobanner") == argv[i])
                nobanner = true;
            else if (as_option("quick") == argv[i])
                quick = true;
            else if (as_option("slow") == argv[i])
                quick = false;
#ifdef SUPPORT_SERIAL_IO
            else if (as_option("port") == argv[i]) {
                ++i;
                if (i == argc)
                    return false;
                port = true;
                port_name = argv[i];
            }
#endif
            else
                return false;
        }
        else if (script_filename.empty())
            script_filename = argv[i];
        else
            return false;
    }
    return true;
}





//#include "unpublished_script_tests.cpp"

int main(int argc, const char * argv[])
{
    try {
        bool showscript, nobanner, quick, help, port, traceauto = false;
        std::string port_name, script_filename;
        const std::string command_help{
           "  <blank line>    quit\n"
           "  *               print trace of most recent exchange\n"
           "  **              print the transformation rules used in the most recent reply\n"
           "  *cacm           replay conversation from Weizenbaum's Jan 1966 CACM paper\n"
           "  *help           show this list of commands\n"
           "  *key            show all keywords in the current script (with precedence)\n"
           "  *key KEYWORD    show the transformation rule for the given KEYWORD\n"
           "  *traceoff       turn off tracing\n"
           "  *traceon        turn on tracing; enter '*' after any exchange to see trace\n"
           "  *traceauto      turn on tracing; trace shown after every exchange\n"
           "  *tracepre       show input sentence prior to applying transformation\n"
           "                  (for watching the operation of Turing machines)\n"
        };

        if (!parse_cmdline(argc, argv, showscript, nobanner, quick, help, port, port_name, script_filename) || help) {
            (help ? std::cout : std::cerr)
                << "Usage: ELIZA [options] [<filename>]\n"
                << "\n"
                << "  " << pad(as_option("nobanner"))   << "don't display startup banner\n"
#ifdef SUPPORT_SERIAL_IO
#if defined(_WIN32)
                << "  " << pad(as_option("port COMn"))  << "use serial port COMn (e.g. COM2)\n"
#else
                << "  " << pad(as_option("port DEV"))   << "use serial port DEV (e.g. /dev/cu.PL2303G-USBtoUART10)\n"
#endif
#endif
                << "  " << pad(as_option("quick"))      << "print at full speed (default)\n"
                << "  " << pad(as_option("showscript")) << "print Weizenbaum's 1966 DOCTOR script\n"
                << "  " << pad("")                      << "e.g. ELIZA " << as_option("showscript") << " > script.txt\n"
                << "  " << pad(as_option("slow"))       << "print at IBM 2741 TTY speed (14 characters per second)\n"
                << "  " << pad("<filename>")            << "use named script file (UTF-8) instead of built-in DOCTOR\n"
                << "  " << pad("")                      << "e.g. ELIZA script.txt\n"
                << "\nIn a conversation with ELIZA, these inputs have special meaning:\n"
                << command_help;
            return help ? EXIT_SUCCESS : EXIT_FAILURE;
        }

        if (showscript) {
            // just output Weizenbaum's DOCTOR script
            std::cout << elizascript::CACM_1966_01_DOCTOR_script;
            return EXIT_SUCCESS;
        }

        if (!nobanner) {
            std::cout
                << "-----------------------------------------------------------------\n"
                << "      ELIZA -- A Computer Program for the Study of Natural\n"
                << "         Language Communication Between Man and Machine\n"
                << "DOCTOR script (c) 1966 Association for Computing Machinery, Inc.\n"
                << "  ELIZA implementation (v0.97) by Anthony Hay, 2022  (CC0 1.0)\n"
                << "-----------------------------------------------------------------\n"
                << "Use command line '" << argv[0] << " " << as_option("help") << "' for usage information.\n";
        }

        RUN_TESTS(); // run all the tests defined with DEF_TEST_FUNC


        elizascript::script eliza_script;
        if (script_filename.empty()) {
            // use default 'internal' 1966 CACM published script
            if (!nobanner)
                std::cout << "No script filename given; using built-in 1966 DOCTOR script.\n";
            elizascript::read(elizascript::CACM_1966_01_DOCTOR_script, eliza_script);
        }
        else {
            // use the named script file
            std::ifstream script_file(script_filename);
            if (!script_file.is_open()) {
                std::cerr << argv[0] << ": failed to open script file '"
                          << script_filename << "'\n";
                return EXIT_FAILURE;
            }
            if (!nobanner)
                std::cout << "Using script file '" << script_filename << "'\n\n\n";
            elizascript::read<std::ifstream>(script_file, eliza_script);
        }

        if (!nobanner)
            std::cout << "Enter a blank line to quit.\n\n\n";


        elizalogic::null_tracer notrace;
        elizalogic::string_tracer trace;
        elizalogic::pre_tracer pretrace;

        elizalogic::eliza eliza(eliza_script.rules, eliza_script.mem_rule);
        eliza.set_tracer(&trace);

#ifdef SUPPORT_SERIAL_IO
        serial_io serial_port;
        if (port) {
            if (serial_port.open(port_name, "")) {
                std::cout
                    << "Switching to serial port "
                    << port_name << '\n';
            }
            else {
                std::cerr << serial_port.last_error_text() << '\n';
                return EXIT_FAILURE;
            }
        }
        auto print = [&](const std::string & s) {
            if (port) {
                serial_port.write(s);
                serial_port.write("\r\n");
            }
            else if (quick)
                std::cout << s << std::endl;
            else
                writeln(s);
        };
        auto input = [&](std::string & s) {
            if (port)
                s = serial_port.getline();
            else
                std::getline(std::cin, s);
        };
#else
        auto print = [&](const std::string & s) {
            if (quick)
                std::cout << s << std::endl;
            else
                writeln(s);
        };
        auto input = [&](std::string & s) {
            std::getline(std::cin, s);
        };
#endif

        print(join(eliza_script.hello_message));

        for (int cacm_index = -1;;) {
            std::string userinput;

            print("");
            input(userinput);

            if (userinput.empty()) {
                if (cacm_index >= 0) {
                    userinput = elizatest::cacm_1966_conversation[cacm_index++].prompt;
                    print(userinput);
                }
                else
                    break;
            }
            if (userinput[0] == '*') {
                const stringlist cmd_line{ split(to_upper(userinput)) };
                const std::string command{ cmd_line[0] };
                if (command == "*") {
                    std::cout << trace.text();
                }
                else if (command == "**") {
                    std::cout << trace.script();
                }
                else if (command == "*TRACEON") {
                    eliza.set_tracer(&trace);
                    traceauto = false;
                    std::cout << "tracing enabled; enter '*' after any exchange to see trace\n";
                }
                else if (command == "*TRACEAUTO") {
                    eliza.set_tracer(&trace);
                    traceauto = true;
                    std::cout << "tracing enabled\n";
                }
                else if (command == "*TRACEOFF") {
                    eliza.set_tracer(&notrace);
                    trace.clear();
                    traceauto = false;
                    std::cout << "tracing disabled\n";
                }
                else if (command == "*TRACEPRE") {
                    eliza.set_tracer(&pretrace);
                    trace.clear();
                    traceauto = false;
                    std::cout << "tracing PRE enabled\n";
                }
                else if (command == "*CACM") {
                    std::cout <<
                        "Replaying conversation from Weizenbaum's January 1966 CACM paper.\n"
                        "Hit enter to see each exchange (use *traceauto to see the trace).\n";
                    cacm_index = 0;
                }
                else if (command == "*KEY") {
                    if (cmd_line.size() == 2) {
                        // print out the rule associated with the given keyword
                        std::string keyword = cmd_line[1];
                        if (keyword == "NONE")
                            keyword = elizalogic::special_rule_none;
                        const auto r = eliza_script.rules.find(keyword);
                        if (r != eliza_script.rules.end()) {
                            const auto & rule = r->second;
                            std::cout << rule->to_string();
                        }
                        else if (cmd_line[1] == "MEMORY")
                            std::cout << eliza_script.mem_rule->to_string();
                        else
                            std::cout << "No '" << cmd_line[1] << "' keyword found in current script\n";
                    }
                    else {
                        // print a list of all keywords
                        using pair = std::pair<std::string, int>;
                        std::vector<pair> v;
                        for (const auto & [key, rule] : eliza_script.rules) {
                            if (key == elizalogic::special_rule_none)
                                continue;
                            else
                                v.emplace_back(key, rule->precedence());
                        }
                        std::sort(v.begin(), v.end(),
                            [](const pair & a, const pair & b) {
                                return a.second > b.second || (a.second == b.second && a.first < b.first);
                            });
                        for (const auto & p : v)
                            std::cout << std::setw(3) << p.second << " " << p.first << "\n";
                        std::cout << "(" << v.size() << " keywords, plus MEMORY and NONE)\n";
                    }
                }
                else
                    std::cout << "Unknown command. Commands are\n" << command_help;
                continue;
            }

            const std::string response{eliza.response(userinput)};

            if (!quick) {
                // The doctor takes a moment to reflect before replying.
                // (Weizenbaum developed ELIZA on an IBM 7094 running CTSS.
                // It's quite likely it took a second or two before responding
                // to the user's statements.)
                sleep_ms(1500);
            }

            if (traceauto)
                std::cout << trace.text();

            print(response);

            if (cacm_index >= elizatest::cacm_1966_conversation_size) {
                std::cout << "\n<end of CACM conversation>\n";
                cacm_index = -1;
            }
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

// I've tried to make this respond to user input exactly as the original
// would have in 1966. I've also tried to communicate how ELIZA works and
// to make it usable.
