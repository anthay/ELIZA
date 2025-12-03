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
    https://github.com/anthay/ELIZA


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
    There are also references to the original MAD-SLIP ELIZA code running
    on a CTSS/7094 emulator available in Rupert Lane's repository
    https://github.com/rupertl/eliza-ctss. (Note that this version
    of the original MAD-SLIP code, which is on a printout dated
    "030665" (presumably 6 March 1965), is missing features described
    in the 1966 CACM paper, such as the keyword stack.)

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
#include <locale>



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
    const char * filename, const size_t line_num, const char * /*function_name*/)
{
    ++test_count;
    if (!(value == expected_value)) {
        ++fault_count;
        // e.g. eliza.cpp(2025) : test failed '1 == 2'
        std::cout
            << filename << '(' << line_num
            << ") : test failed '" << value
            << " == " << expected_value
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
    static_assert(std::numeric_limits<unsigned char>::max() == std::size(hollerith_encoding) - 1);
    const unsigned char uc{ static_cast<unsigned char>(c) };
    return hollerith_encoding[uc] != hollerith_undefined;
}


// return the given UTF-8 encoded string as a UTF-32 encoded string
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


// return the given UTF-32 code point as a UTF-8 encoded string
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


// Return the UTF-32 code point that represents the uppercase equivalent of
// the given UTF-32 code point. If there is no uppercase equivalent, the given
// code point is returned unchanged.
// Works only for characters, upper and lower, that are represented by a
// single code point.
uint32_t uppercase_utf32(uint32_t c32)
{
    // From the toupper table in ISO/IEC 30112 WD10, dated 2014.
    // https://www.open-std.org/JTC1/SC35/WG5/docs/30112d10.pdf
    // Accessed 16 November 2025.
    static const uint32_t lower_upper[] = {
        0x0061UL,0x0041UL,    0x0062UL,0x0042UL,    0x0063UL,0x0043UL,    0x0064UL,0x0044UL,
        0x0065UL,0x0045UL,    0x0066UL,0x0046UL,    0x0067UL,0x0047UL,    0x0068UL,0x0048UL,
        0x0069UL,0x0049UL,    0x006AUL,0x004AUL,    0x006BUL,0x004BUL,    0x006CUL,0x004CUL,
        0x006DUL,0x004DUL,    0x006EUL,0x004EUL,    0x006FUL,0x004FUL,    0x0070UL,0x0050UL,
        0x0071UL,0x0051UL,    0x0072UL,0x0052UL,    0x0073UL,0x0053UL,    0x0074UL,0x0054UL,
        0x0075UL,0x0055UL,    0x0076UL,0x0056UL,    0x0077UL,0x0057UL,    0x0078UL,0x0058UL,
        0x0079UL,0x0059UL,    0x007AUL,0x005AUL,    0x00B5UL,0x039CUL,    0x00E0UL,0x00C0UL,
        0x00E1UL,0x00C1UL,    0x00E2UL,0x00C2UL,    0x00E3UL,0x00C3UL,    0x00E4UL,0x00C4UL,
        0x00E5UL,0x00C5UL,    0x00E6UL,0x00C6UL,    0x00E7UL,0x00C7UL,    0x00E8UL,0x00C8UL,
        0x00E9UL,0x00C9UL,    0x00EAUL,0x00CAUL,    0x00EBUL,0x00CBUL,    0x00ECUL,0x00CCUL,
        0x00EDUL,0x00CDUL,    0x00EEUL,0x00CEUL,    0x00EFUL,0x00CFUL,    0x00F0UL,0x00D0UL,
        0x00F1UL,0x00D1UL,    0x00F2UL,0x00D2UL,    0x00F3UL,0x00D3UL,    0x00F4UL,0x00D4UL,
        0x00F5UL,0x00D5UL,    0x00F6UL,0x00D6UL,    0x00F8UL,0x00D8UL,    0x00F9UL,0x00D9UL,
        0x00FAUL,0x00DAUL,    0x00FBUL,0x00DBUL,    0x00FCUL,0x00DCUL,    0x00FDUL,0x00DDUL,
        0x00FEUL,0x00DEUL,    0x00FFUL,0x0178UL,    0x0101UL,0x0100UL,    0x0103UL,0x0102UL,
        0x0105UL,0x0104UL,    0x0107UL,0x0106UL,    0x0109UL,0x0108UL,    0x010BUL,0x010AUL,
        0x010DUL,0x010CUL,    0x010FUL,0x010EUL,    0x0111UL,0x0110UL,    0x0113UL,0x0112UL,
        0x0115UL,0x0114UL,    0x0117UL,0x0116UL,    0x0119UL,0x0118UL,    0x011BUL,0x011AUL,
        0x011DUL,0x011CUL,    0x011FUL,0x011EUL,    0x0121UL,0x0120UL,    0x0123UL,0x0122UL,
        0x0125UL,0x0124UL,    0x0127UL,0x0126UL,    0x0129UL,0x0128UL,    0x012BUL,0x012AUL,
        0x012DUL,0x012CUL,    0x012FUL,0x012EUL,    0x0131UL,0x0049UL,    0x0133UL,0x0132UL,
        0x0135UL,0x0134UL,    0x0137UL,0x0136UL,    0x013AUL,0x0139UL,    0x013CUL,0x013BUL,
        0x013EUL,0x013DUL,    0x0140UL,0x013FUL,    0x0142UL,0x0141UL,    0x0144UL,0x0143UL,
        0x0146UL,0x0145UL,    0x0148UL,0x0147UL,    0x014BUL,0x014AUL,    0x014DUL,0x014CUL,
        0x014FUL,0x014EUL,    0x0151UL,0x0150UL,    0x0153UL,0x0152UL,    0x0155UL,0x0154UL,
        0x0157UL,0x0156UL,    0x0159UL,0x0158UL,    0x015BUL,0x015AUL,    0x015DUL,0x015CUL,
        0x015FUL,0x015EUL,    0x0161UL,0x0160UL,    0x0163UL,0x0162UL,    0x0165UL,0x0164UL,
        0x0167UL,0x0166UL,    0x0169UL,0x0168UL,    0x016BUL,0x016AUL,    0x016DUL,0x016CUL,
        0x016FUL,0x016EUL,    0x0171UL,0x0170UL,    0x0173UL,0x0172UL,    0x0175UL,0x0174UL,
        0x0177UL,0x0176UL,    0x017AUL,0x0179UL,    0x017CUL,0x017BUL,    0x017EUL,0x017DUL,
        0x017FUL,0x0053UL,    0x0180UL,0x0243UL,    0x0183UL,0x0182UL,    0x0185UL,0x0184UL,
        0x0188UL,0x0187UL,    0x018CUL,0x018BUL,    0x0192UL,0x0191UL,    0x0195UL,0x01F6UL,
        0x0199UL,0x0198UL,    0x019AUL,0x023DUL,    0x019EUL,0x0220UL,    0x01A1UL,0x01A0UL,
        0x01A3UL,0x01A2UL,    0x01A5UL,0x01A4UL,    0x01A8UL,0x01A7UL,    0x01ADUL,0x01ACUL,
        0x01B0UL,0x01AFUL,    0x01B4UL,0x01B3UL,    0x01B6UL,0x01B5UL,    0x01B9UL,0x01B8UL,
        0x01BDUL,0x01BCUL,    0x01BFUL,0x01F7UL,    0x01C5UL,0x01C4UL,    0x01C6UL,0x01C4UL,
        0x01C8UL,0x01C7UL,    0x01C9UL,0x01C7UL,    0x01CBUL,0x01CAUL,    0x01CCUL,0x01CAUL,
        0x01CEUL,0x01CDUL,    0x01D0UL,0x01CFUL,    0x01D2UL,0x01D1UL,    0x01D4UL,0x01D3UL,
        0x01D6UL,0x01D5UL,    0x01D8UL,0x01D7UL,    0x01DAUL,0x01D9UL,    0x01DCUL,0x01DBUL,
        0x01DDUL,0x018EUL,    0x01DFUL,0x01DEUL,    0x01E1UL,0x01E0UL,    0x01E3UL,0x01E2UL,
        0x01E5UL,0x01E4UL,    0x01E7UL,0x01E6UL,    0x01E9UL,0x01E8UL,    0x01EBUL,0x01EAUL,
        0x01EDUL,0x01ECUL,    0x01EFUL,0x01EEUL,    0x01F2UL,0x01F1UL,    0x01F3UL,0x01F1UL,
        0x01F5UL,0x01F4UL,    0x01F9UL,0x01F8UL,    0x01FBUL,0x01FAUL,    0x01FDUL,0x01FCUL,
        0x01FFUL,0x01FEUL,    0x0201UL,0x0200UL,    0x0203UL,0x0202UL,    0x0205UL,0x0204UL,
        0x0207UL,0x0206UL,    0x0209UL,0x0208UL,    0x020BUL,0x020AUL,    0x020DUL,0x020CUL,
        0x020FUL,0x020EUL,    0x0211UL,0x0210UL,    0x0213UL,0x0212UL,    0x0215UL,0x0214UL,
        0x0217UL,0x0216UL,    0x0219UL,0x0218UL,    0x021BUL,0x021AUL,    0x021DUL,0x021CUL,
        0x021FUL,0x021EUL,    0x0223UL,0x0222UL,    0x0225UL,0x0224UL,    0x0227UL,0x0226UL,
        0x0229UL,0x0228UL,    0x022BUL,0x022AUL,    0x022DUL,0x022CUL,    0x022FUL,0x022EUL,
        0x0231UL,0x0230UL,    0x0233UL,0x0232UL,    0x023CUL,0x023BUL,    0x0242UL,0x0241UL,
        0x0247UL,0x0246UL,    0x0249UL,0x0248UL,    0x024BUL,0x024AUL,    0x024DUL,0x024CUL,
        0x024FUL,0x024EUL,    0x0250UL,0x2C6FUL,    0x0251UL,0x2C6DUL,    0x0253UL,0x0181UL,
        0x0254UL,0x0186UL,    0x0256UL,0x0189UL,    0x0257UL,0x018AUL,    0x0259UL,0x018FUL,
        0x025BUL,0x0190UL,    0x0260UL,0x0193UL,    0x0263UL,0x0194UL,    0x0268UL,0x0197UL,
        0x0269UL,0x0196UL,    0x026BUL,0x2C62UL,    0x026FUL,0x019CUL,    0x0271UL,0x2C6EUL,
        0x0272UL,0x019DUL,    0x0275UL,0x019FUL,    0x027DUL,0x2C64UL,    0x0280UL,0x01A6UL,
        0x0283UL,0x01A9UL,    0x0288UL,0x01AEUL,    0x0289UL,0x0244UL,    0x028AUL,0x01B1UL,
        0x028BUL,0x01B2UL,    0x028CUL,0x0245UL,    0x0292UL,0x01B7UL,    0x0345UL,0x0399UL,
        0x0371UL,0x0370UL,    0x0373UL,0x0372UL,    0x0377UL,0x0376UL,    0x037BUL,0x03FDUL,
        0x037CUL,0x03FEUL,    0x037DUL,0x03FFUL,    0x03ACUL,0x0386UL,    0x03ADUL,0x0388UL,
        0x03AEUL,0x0389UL,    0x03AFUL,0x038AUL,    0x03B1UL,0x0391UL,    0x03B2UL,0x0392UL,
        0x03B3UL,0x0393UL,    0x03B4UL,0x0394UL,    0x03B5UL,0x0395UL,    0x03B6UL,0x0396UL,
        0x03B7UL,0x0397UL,    0x03B8UL,0x0398UL,    0x03B9UL,0x0399UL,    0x03BAUL,0x039AUL,
        0x03BBUL,0x039BUL,    0x03BCUL,0x039CUL,    0x03BDUL,0x039DUL,    0x03BEUL,0x039EUL,
        0x03BFUL,0x039FUL,    0x03C0UL,0x03A0UL,    0x03C1UL,0x03A1UL,    0x03C2UL,0x03A3UL,
        0x03C3UL,0x03A3UL,    0x03C4UL,0x03A4UL,    0x03C5UL,0x03A5UL,    0x03C6UL,0x03A6UL,
        0x03C7UL,0x03A7UL,    0x03C8UL,0x03A8UL,    0x03C9UL,0x03A9UL,    0x03CAUL,0x03AAUL,
        0x03CBUL,0x03ABUL,    0x03CCUL,0x038CUL,    0x03CDUL,0x038EUL,    0x03CEUL,0x038FUL,
        0x03D0UL,0x0392UL,    0x03D1UL,0x0398UL,    0x03D5UL,0x03A6UL,    0x03D6UL,0x03A0UL,
        0x03D9UL,0x03D8UL,    0x03DBUL,0x03DAUL,    0x03DDUL,0x03DCUL,    0x03DFUL,0x03DEUL,
        0x03E1UL,0x03E0UL,    0x03E3UL,0x03E2UL,    0x03E5UL,0x03E4UL,    0x03E7UL,0x03E6UL,
        0x03E9UL,0x03E8UL,    0x03EBUL,0x03EAUL,    0x03EDUL,0x03ECUL,    0x03EFUL,0x03EEUL,
        0x03F0UL,0x039AUL,    0x03F1UL,0x03A1UL,    0x03F2UL,0x03F9UL,    0x03F5UL,0x0395UL,
        0x03F8UL,0x03F7UL,    0x03FBUL,0x03FAUL,    0x0430UL,0x0410UL,    0x0431UL,0x0411UL,
        0x0432UL,0x0412UL,    0x0433UL,0x0413UL,    0x0434UL,0x0414UL,    0x0435UL,0x0415UL,
        0x0436UL,0x0416UL,    0x0437UL,0x0417UL,    0x0438UL,0x0418UL,    0x0439UL,0x0419UL,
        0x043AUL,0x041AUL,    0x043BUL,0x041BUL,    0x043CUL,0x041CUL,    0x043DUL,0x041DUL,
        0x043EUL,0x041EUL,    0x043FUL,0x041FUL,    0x0440UL,0x0420UL,    0x0441UL,0x0421UL,
        0x0442UL,0x0422UL,    0x0443UL,0x0423UL,    0x0444UL,0x0424UL,    0x0445UL,0x0425UL,
        0x0446UL,0x0426UL,    0x0447UL,0x0427UL,    0x0448UL,0x0428UL,    0x0449UL,0x0429UL,
        0x044AUL,0x042AUL,    0x044BUL,0x042BUL,    0x044CUL,0x042CUL,    0x044DUL,0x042DUL,
        0x044EUL,0x042EUL,    0x044FUL,0x042FUL,    0x0450UL,0x0400UL,    0x0451UL,0x0401UL,
        0x0452UL,0x0402UL,    0x0453UL,0x0403UL,    0x0454UL,0x0404UL,    0x0455UL,0x0405UL,
        0x0456UL,0x0406UL,    0x0457UL,0x0407UL,    0x0458UL,0x0408UL,    0x0459UL,0x0409UL,
        0x045AUL,0x040AUL,    0x045BUL,0x040BUL,    0x045CUL,0x040CUL,    0x045DUL,0x040DUL,
        0x045EUL,0x040EUL,    0x045FUL,0x040FUL,    0x0461UL,0x0460UL,    0x0463UL,0x0462UL,
        0x0465UL,0x0464UL,    0x0467UL,0x0466UL,    0x0469UL,0x0468UL,    0x046BUL,0x046AUL,
        0x046DUL,0x046CUL,    0x046FUL,0x046EUL,    0x0471UL,0x0470UL,    0x0473UL,0x0472UL,
        0x0475UL,0x0474UL,    0x0477UL,0x0476UL,    0x0479UL,0x0478UL,    0x047BUL,0x047AUL,
        0x047DUL,0x047CUL,    0x047FUL,0x047EUL,    0x0481UL,0x0480UL,    0x048BUL,0x048AUL,
        0x048DUL,0x048CUL,    0x048FUL,0x048EUL,    0x0491UL,0x0490UL,    0x0493UL,0x0492UL,
        0x0495UL,0x0494UL,    0x0497UL,0x0496UL,    0x0499UL,0x0498UL,    0x049BUL,0x049AUL,
        0x049DUL,0x049CUL,    0x049FUL,0x049EUL,    0x04A1UL,0x04A0UL,    0x04A3UL,0x04A2UL,
        0x04A5UL,0x04A4UL,    0x04A7UL,0x04A6UL,    0x04A9UL,0x04A8UL,    0x04ABUL,0x04AAUL,
        0x04ADUL,0x04ACUL,    0x04AFUL,0x04AEUL,    0x04B1UL,0x04B0UL,    0x04B3UL,0x04B2UL,
        0x04B5UL,0x04B4UL,    0x04B7UL,0x04B6UL,    0x04B9UL,0x04B8UL,    0x04BBUL,0x04BAUL,
        0x04BDUL,0x04BCUL,    0x04BFUL,0x04BEUL,    0x04C2UL,0x04C1UL,    0x04C4UL,0x04C3UL,
        0x04C6UL,0x04C5UL,    0x04C8UL,0x04C7UL,    0x04CAUL,0x04C9UL,    0x04CCUL,0x04CBUL,
        0x04CEUL,0x04CDUL,    0x04CFUL,0x04C0UL,    0x04D1UL,0x04D0UL,    0x04D3UL,0x04D2UL,
        0x04D5UL,0x04D4UL,    0x04D7UL,0x04D6UL,    0x04D9UL,0x04D8UL,    0x04DBUL,0x04DAUL,
        0x04DDUL,0x04DCUL,    0x04DFUL,0x04DEUL,    0x04E1UL,0x04E0UL,    0x04E3UL,0x04E2UL,
        0x04E5UL,0x04E4UL,    0x04E7UL,0x04E6UL,    0x04E9UL,0x04E8UL,    0x04EBUL,0x04EAUL,
        0x04EDUL,0x04ECUL,    0x04EFUL,0x04EEUL,    0x04F1UL,0x04F0UL,    0x04F3UL,0x04F2UL,
        0x04F5UL,0x04F4UL,    0x04F7UL,0x04F6UL,    0x04F9UL,0x04F8UL,    0x04FBUL,0x04FAUL,
        0x04FDUL,0x04FCUL,    0x04FFUL,0x04FEUL,    0x0501UL,0x0500UL,    0x0503UL,0x0502UL,
        0x0505UL,0x0504UL,    0x0507UL,0x0506UL,    0x0509UL,0x0508UL,    0x050BUL,0x050AUL,
        0x050DUL,0x050CUL,    0x050FUL,0x050EUL,    0x0511UL,0x0510UL,    0x0513UL,0x0512UL,
        0x0515UL,0x0514UL,    0x0517UL,0x0516UL,    0x0519UL,0x0518UL,    0x051BUL,0x051AUL,
        0x051DUL,0x051CUL,    0x051FUL,0x051EUL,    0x0521UL,0x0520UL,    0x0523UL,0x0522UL,
        0x0561UL,0x0531UL,    0x0562UL,0x0532UL,    0x0563UL,0x0533UL,    0x0564UL,0x0534UL,
        0x0565UL,0x0535UL,    0x0566UL,0x0536UL,    0x0567UL,0x0537UL,    0x0568UL,0x0538UL,
        0x0569UL,0x0539UL,    0x056AUL,0x053AUL,    0x056BUL,0x053BUL,    0x056CUL,0x053CUL,
        0x056DUL,0x053DUL,    0x056EUL,0x053EUL,    0x056FUL,0x053FUL,    0x0570UL,0x0540UL,
        0x0571UL,0x0541UL,    0x0572UL,0x0542UL,    0x0573UL,0x0543UL,    0x0574UL,0x0544UL,
        0x0575UL,0x0545UL,    0x0576UL,0x0546UL,    0x0577UL,0x0547UL,    0x0578UL,0x0548UL,
        0x0579UL,0x0549UL,    0x057AUL,0x054AUL,    0x057BUL,0x054BUL,    0x057CUL,0x054CUL,
        0x057DUL,0x054DUL,    0x057EUL,0x054EUL,    0x057FUL,0x054FUL,    0x0580UL,0x0550UL,
        0x0581UL,0x0551UL,    0x0582UL,0x0552UL,    0x0583UL,0x0553UL,    0x0584UL,0x0554UL,
        0x0585UL,0x0555UL,    0x0586UL,0x0556UL,    0x1D7DUL,0x2C63UL,    0x1E01UL,0x1E00UL,
        0x1E03UL,0x1E02UL,    0x1E05UL,0x1E04UL,    0x1E07UL,0x1E06UL,    0x1E09UL,0x1E08UL,
        0x1E0BUL,0x1E0AUL,    0x1E0DUL,0x1E0CUL,    0x1E0FUL,0x1E0EUL,    0x1E11UL,0x1E10UL,
        0x1E13UL,0x1E12UL,    0x1E15UL,0x1E14UL,    0x1E17UL,0x1E16UL,    0x1E19UL,0x1E18UL,
        0x1E1BUL,0x1E1AUL,    0x1E1DUL,0x1E1CUL,    0x1E1FUL,0x1E1EUL,    0x1E21UL,0x1E20UL,
        0x1E23UL,0x1E22UL,    0x1E25UL,0x1E24UL,    0x1E27UL,0x1E26UL,    0x1E29UL,0x1E28UL,
        0x1E2BUL,0x1E2AUL,    0x1E2DUL,0x1E2CUL,    0x1E2FUL,0x1E2EUL,    0x1E31UL,0x1E30UL,
        0x1E33UL,0x1E32UL,    0x1E35UL,0x1E34UL,    0x1E37UL,0x1E36UL,    0x1E39UL,0x1E38UL,
        0x1E3BUL,0x1E3AUL,    0x1E3DUL,0x1E3CUL,    0x1E3FUL,0x1E3EUL,    0x1E41UL,0x1E40UL,
        0x1E43UL,0x1E42UL,    0x1E45UL,0x1E44UL,    0x1E47UL,0x1E46UL,    0x1E49UL,0x1E48UL,
        0x1E4BUL,0x1E4AUL,    0x1E4DUL,0x1E4CUL,    0x1E4FUL,0x1E4EUL,    0x1E51UL,0x1E50UL,
        0x1E53UL,0x1E52UL,    0x1E55UL,0x1E54UL,    0x1E57UL,0x1E56UL,    0x1E59UL,0x1E58UL,
        0x1E5BUL,0x1E5AUL,    0x1E5DUL,0x1E5CUL,    0x1E5FUL,0x1E5EUL,    0x1E61UL,0x1E60UL,
        0x1E63UL,0x1E62UL,    0x1E65UL,0x1E64UL,    0x1E67UL,0x1E66UL,    0x1E69UL,0x1E68UL,
        0x1E6BUL,0x1E6AUL,    0x1E6DUL,0x1E6CUL,    0x1E6FUL,0x1E6EUL,    0x1E71UL,0x1E70UL,
        0x1E73UL,0x1E72UL,    0x1E75UL,0x1E74UL,    0x1E77UL,0x1E76UL,    0x1E79UL,0x1E78UL,
        0x1E7BUL,0x1E7AUL,    0x1E7DUL,0x1E7CUL,    0x1E7FUL,0x1E7EUL,    0x1E81UL,0x1E80UL,
        0x1E83UL,0x1E82UL,    0x1E85UL,0x1E84UL,    0x1E87UL,0x1E86UL,    0x1E89UL,0x1E88UL,
        0x1E8BUL,0x1E8AUL,    0x1E8DUL,0x1E8CUL,    0x1E8FUL,0x1E8EUL,    0x1E91UL,0x1E90UL,
        0x1E93UL,0x1E92UL,    0x1E95UL,0x1E94UL,    0x1E9BUL,0x1E60UL,    0x1EA1UL,0x1EA0UL,
        0x1EA3UL,0x1EA2UL,    0x1EA5UL,0x1EA4UL,    0x1EA7UL,0x1EA6UL,    0x1EA9UL,0x1EA8UL,
        0x1EABUL,0x1EAAUL,    0x1EADUL,0x1EACUL,    0x1EAFUL,0x1EAEUL,    0x1EB1UL,0x1EB0UL,
        0x1EB3UL,0x1EB2UL,    0x1EB5UL,0x1EB4UL,    0x1EB7UL,0x1EB6UL,    0x1EB9UL,0x1EB8UL,
        0x1EBBUL,0x1EBAUL,    0x1EBDUL,0x1EBCUL,    0x1EBFUL,0x1EBEUL,    0x1EC1UL,0x1EC0UL,
        0x1EC3UL,0x1EC2UL,    0x1EC5UL,0x1EC4UL,    0x1EC7UL,0x1EC6UL,    0x1EC9UL,0x1EC8UL,
        0x1ECBUL,0x1ECAUL,    0x1ECDUL,0x1ECCUL,    0x1ECFUL,0x1ECEUL,    0x1ED1UL,0x1ED0UL,
        0x1ED3UL,0x1ED2UL,    0x1ED5UL,0x1ED4UL,    0x1ED7UL,0x1ED6UL,    0x1ED9UL,0x1ED8UL,
        0x1EDBUL,0x1EDAUL,    0x1EDDUL,0x1EDCUL,    0x1EDFUL,0x1EDEUL,    0x1EE1UL,0x1EE0UL,
        0x1EE3UL,0x1EE2UL,    0x1EE5UL,0x1EE4UL,    0x1EE7UL,0x1EE6UL,    0x1EE9UL,0x1EE8UL,
        0x1EEBUL,0x1EEAUL,    0x1EEDUL,0x1EECUL,    0x1EEFUL,0x1EEEUL,    0x1EF1UL,0x1EF0UL,
        0x1EF3UL,0x1EF2UL,    0x1EF5UL,0x1EF4UL,    0x1EF7UL,0x1EF6UL,    0x1EF9UL,0x1EF8UL,
        0x1EFBUL,0x1EFAUL,    0x1EFDUL,0x1EFCUL,    0x1EFFUL,0x1EFEUL,    0x1F00UL,0x1F08UL,
        0x1F01UL,0x1F09UL,    0x1F02UL,0x1F0AUL,    0x1F03UL,0x1F0BUL,    0x1F04UL,0x1F0CUL,
        0x1F05UL,0x1F0DUL,    0x1F06UL,0x1F0EUL,    0x1F07UL,0x1F0FUL,    0x1F10UL,0x1F18UL,
        0x1F11UL,0x1F19UL,    0x1F12UL,0x1F1AUL,    0x1F13UL,0x1F1BUL,    0x1F14UL,0x1F1CUL,
        0x1F15UL,0x1F1DUL,    0x1F20UL,0x1F28UL,    0x1F21UL,0x1F29UL,    0x1F22UL,0x1F2AUL,
        0x1F23UL,0x1F2BUL,    0x1F24UL,0x1F2CUL,    0x1F25UL,0x1F2DUL,    0x1F26UL,0x1F2EUL,
        0x1F27UL,0x1F2FUL,    0x1F30UL,0x1F38UL,    0x1F31UL,0x1F39UL,    0x1F32UL,0x1F3AUL,
        0x1F33UL,0x1F3BUL,    0x1F34UL,0x1F3CUL,    0x1F35UL,0x1F3DUL,    0x1F36UL,0x1F3EUL,
        0x1F37UL,0x1F3FUL,    0x1F40UL,0x1F48UL,    0x1F41UL,0x1F49UL,    0x1F42UL,0x1F4AUL,
        0x1F43UL,0x1F4BUL,    0x1F44UL,0x1F4CUL,    0x1F45UL,0x1F4DUL,    0x1F51UL,0x1F59UL,
        0x1F53UL,0x1F5BUL,    0x1F55UL,0x1F5DUL,    0x1F57UL,0x1F5FUL,    0x1F60UL,0x1F68UL,
        0x1F61UL,0x1F69UL,    0x1F62UL,0x1F6AUL,    0x1F63UL,0x1F6BUL,    0x1F64UL,0x1F6CUL,
        0x1F65UL,0x1F6DUL,    0x1F66UL,0x1F6EUL,    0x1F67UL,0x1F6FUL,    0x1F70UL,0x1FBAUL,
        0x1F71UL,0x1FBBUL,    0x1F72UL,0x1FC8UL,    0x1F73UL,0x1FC9UL,    0x1F74UL,0x1FCAUL,
        0x1F75UL,0x1FCBUL,    0x1F76UL,0x1FDAUL,    0x1F77UL,0x1FDBUL,    0x1F78UL,0x1FF8UL,
        0x1F79UL,0x1FF9UL,    0x1F7AUL,0x1FEAUL,    0x1F7BUL,0x1FEBUL,    0x1F7CUL,0x1FFAUL,
        0x1F7DUL,0x1FFBUL,    0x1F80UL,0x1F88UL,    0x1F81UL,0x1F89UL,    0x1F82UL,0x1F8AUL,
        0x1F83UL,0x1F8BUL,    0x1F84UL,0x1F8CUL,    0x1F85UL,0x1F8DUL,    0x1F86UL,0x1F8EUL,
        0x1F87UL,0x1F8FUL,    0x1F90UL,0x1F98UL,    0x1F91UL,0x1F99UL,    0x1F92UL,0x1F9AUL,
        0x1F93UL,0x1F9BUL,    0x1F94UL,0x1F9CUL,    0x1F95UL,0x1F9DUL,    0x1F96UL,0x1F9EUL,
        0x1F97UL,0x1F9FUL,    0x1FA0UL,0x1FA8UL,    0x1FA1UL,0x1FA9UL,    0x1FA2UL,0x1FAAUL,
        0x1FA3UL,0x1FABUL,    0x1FA4UL,0x1FACUL,    0x1FA5UL,0x1FADUL,    0x1FA6UL,0x1FAEUL,
        0x1FA7UL,0x1FAFUL,    0x1FB0UL,0x1FB8UL,    0x1FB1UL,0x1FB9UL,    0x1FB3UL,0x1FBCUL,
        0x1FBEUL,0x0399UL,    0x1FC3UL,0x1FCCUL,    0x1FD0UL,0x1FD8UL,    0x1FD1UL,0x1FD9UL,
        0x1FE0UL,0x1FE8UL,    0x1FE1UL,0x1FE9UL,    0x1FE5UL,0x1FECUL,    0x1FF3UL,0x1FFCUL,
        0x214EUL,0x2132UL,    0x2170UL,0x2160UL,    0x2171UL,0x2161UL,    0x2172UL,0x2162UL,
        0x2173UL,0x2163UL,    0x2174UL,0x2164UL,    0x2175UL,0x2165UL,    0x2176UL,0x2166UL,
        0x2177UL,0x2167UL,    0x2178UL,0x2168UL,    0x2179UL,0x2169UL,    0x217AUL,0x216AUL,
        0x217BUL,0x216BUL,    0x217CUL,0x216CUL,    0x217DUL,0x216DUL,    0x217EUL,0x216EUL,
        0x217FUL,0x216FUL,    0x2184UL,0x2183UL,    0x24D0UL,0x24B6UL,    0x24D1UL,0x24B7UL,
        0x24D2UL,0x24B8UL,    0x24D3UL,0x24B9UL,    0x24D4UL,0x24BAUL,    0x24D5UL,0x24BBUL,
        0x24D6UL,0x24BCUL,    0x24D7UL,0x24BDUL,    0x24D8UL,0x24BEUL,    0x24D9UL,0x24BFUL,
        0x24DAUL,0x24C0UL,    0x24DBUL,0x24C1UL,    0x24DCUL,0x24C2UL,    0x24DDUL,0x24C3UL,
        0x24DEUL,0x24C4UL,    0x24DFUL,0x24C5UL,    0x24E0UL,0x24C6UL,    0x24E1UL,0x24C7UL,
        0x24E2UL,0x24C8UL,    0x24E3UL,0x24C9UL,    0x24E4UL,0x24CAUL,    0x24E5UL,0x24CBUL,
        0x24E6UL,0x24CCUL,    0x24E7UL,0x24CDUL,    0x24E8UL,0x24CEUL,    0x24E9UL,0x24CFUL,
        0x2C30UL,0x2C00UL,    0x2C31UL,0x2C01UL,    0x2C32UL,0x2C02UL,    0x2C33UL,0x2C03UL,
        0x2C34UL,0x2C04UL,    0x2C35UL,0x2C05UL,    0x2C36UL,0x2C06UL,    0x2C37UL,0x2C07UL,
        0x2C38UL,0x2C08UL,    0x2C39UL,0x2C09UL,    0x2C3AUL,0x2C0AUL,    0x2C3BUL,0x2C0BUL,
        0x2C3CUL,0x2C0CUL,    0x2C3DUL,0x2C0DUL,    0x2C3EUL,0x2C0EUL,    0x2C3FUL,0x2C0FUL,
        0x2C40UL,0x2C10UL,    0x2C41UL,0x2C11UL,    0x2C42UL,0x2C12UL,    0x2C43UL,0x2C13UL,
        0x2C44UL,0x2C14UL,    0x2C45UL,0x2C15UL,    0x2C46UL,0x2C16UL,    0x2C47UL,0x2C17UL,
        0x2C48UL,0x2C18UL,    0x2C49UL,0x2C19UL,    0x2C4AUL,0x2C1AUL,    0x2C4BUL,0x2C1BUL,
        0x2C4CUL,0x2C1CUL,    0x2C4DUL,0x2C1DUL,    0x2C4EUL,0x2C1EUL,    0x2C4FUL,0x2C1FUL,
        0x2C50UL,0x2C20UL,    0x2C51UL,0x2C21UL,    0x2C52UL,0x2C22UL,    0x2C53UL,0x2C23UL,
        0x2C54UL,0x2C24UL,    0x2C55UL,0x2C25UL,    0x2C56UL,0x2C26UL,    0x2C57UL,0x2C27UL,
        0x2C58UL,0x2C28UL,    0x2C59UL,0x2C29UL,    0x2C5AUL,0x2C2AUL,    0x2C5BUL,0x2C2BUL,
        0x2C5CUL,0x2C2CUL,    0x2C5DUL,0x2C2DUL,    0x2C5EUL,0x2C2EUL,    0x2C61UL,0x2C60UL,
        0x2C65UL,0x023AUL,    0x2C66UL,0x023EUL,    0x2C68UL,0x2C67UL,    0x2C6AUL,0x2C69UL,
        0x2C6CUL,0x2C6BUL,    0x2C73UL,0x2C72UL,    0x2C76UL,0x2C75UL,    0x2C81UL,0x2C80UL,
        0x2C83UL,0x2C82UL,    0x2C85UL,0x2C84UL,    0x2C87UL,0x2C86UL,    0x2C89UL,0x2C88UL,
        0x2C8BUL,0x2C8AUL,    0x2C8DUL,0x2C8CUL,    0x2C8FUL,0x2C8EUL,    0x2C91UL,0x2C90UL,
        0x2C93UL,0x2C92UL,    0x2C95UL,0x2C94UL,    0x2C97UL,0x2C96UL,    0x2C99UL,0x2C98UL,
        0x2C9BUL,0x2C9AUL,    0x2C9DUL,0x2C9CUL,    0x2C9FUL,0x2C9EUL,    0x2CA1UL,0x2CA0UL,
        0x2CA3UL,0x2CA2UL,    0x2CA5UL,0x2CA4UL,    0x2CA7UL,0x2CA6UL,    0x2CA9UL,0x2CA8UL,
        0x2CABUL,0x2CAAUL,    0x2CADUL,0x2CACUL,    0x2CAFUL,0x2CAEUL,    0x2CB1UL,0x2CB0UL,
        0x2CB3UL,0x2CB2UL,    0x2CB5UL,0x2CB4UL,    0x2CB7UL,0x2CB6UL,    0x2CB9UL,0x2CB8UL,
        0x2CBBUL,0x2CBAUL,    0x2CBDUL,0x2CBCUL,    0x2CBFUL,0x2CBEUL,    0x2CC1UL,0x2CC0UL,
        0x2CC3UL,0x2CC2UL,    0x2CC5UL,0x2CC4UL,    0x2CC7UL,0x2CC6UL,    0x2CC9UL,0x2CC8UL,
        0x2CCBUL,0x2CCAUL,    0x2CCDUL,0x2CCCUL,    0x2CCFUL,0x2CCEUL,    0x2CD1UL,0x2CD0UL,
        0x2CD3UL,0x2CD2UL,    0x2CD5UL,0x2CD4UL,    0x2CD7UL,0x2CD6UL,    0x2CD9UL,0x2CD8UL,
        0x2CDBUL,0x2CDAUL,    0x2CDDUL,0x2CDCUL,    0x2CDFUL,0x2CDEUL,    0x2CE1UL,0x2CE0UL,
        0x2CE3UL,0x2CE2UL,    0x2D00UL,0x10A0UL,    0x2D01UL,0x10A1UL,    0x2D02UL,0x10A2UL,
        0x2D03UL,0x10A3UL,    0x2D04UL,0x10A4UL,    0x2D05UL,0x10A5UL,    0x2D06UL,0x10A6UL,
        0x2D07UL,0x10A7UL,    0x2D08UL,0x10A8UL,    0x2D09UL,0x10A9UL,    0x2D0AUL,0x10AAUL,
        0x2D0BUL,0x10ABUL,    0x2D0CUL,0x10ACUL,    0x2D0DUL,0x10ADUL,    0x2D0EUL,0x10AEUL,
        0x2D0FUL,0x10AFUL,    0x2D10UL,0x10B0UL,    0x2D11UL,0x10B1UL,    0x2D12UL,0x10B2UL,
        0x2D13UL,0x10B3UL,    0x2D14UL,0x10B4UL,    0x2D15UL,0x10B5UL,    0x2D16UL,0x10B6UL,
        0x2D17UL,0x10B7UL,    0x2D18UL,0x10B8UL,    0x2D19UL,0x10B9UL,    0x2D1AUL,0x10BAUL,
        0x2D1BUL,0x10BBUL,    0x2D1CUL,0x10BCUL,    0x2D1DUL,0x10BDUL,    0x2D1EUL,0x10BEUL,
        0x2D1FUL,0x10BFUL,    0x2D20UL,0x10C0UL,    0x2D21UL,0x10C1UL,    0x2D22UL,0x10C2UL,
        0x2D23UL,0x10C3UL,    0x2D24UL,0x10C4UL,    0x2D25UL,0x10C5UL,    0xFF41UL,0xFF21UL,
        0xFF42UL,0xFF22UL,    0xFF43UL,0xFF23UL,    0xFF44UL,0xFF24UL,    0xFF45UL,0xFF25UL,
        0xFF46UL,0xFF26UL,    0xFF47UL,0xFF27UL,    0xFF48UL,0xFF28UL,    0xFF49UL,0xFF29UL,
        0xFF4AUL,0xFF2AUL,    0xFF4BUL,0xFF2BUL,    0xFF4CUL,0xFF2CUL,    0xFF4DUL,0xFF2DUL,
        0xFF4EUL,0xFF2EUL,    0xFF4FUL,0xFF2FUL,    0xFF50UL,0xFF30UL,    0xFF51UL,0xFF31UL,
        0xFF52UL,0xFF32UL,    0xFF53UL,0xFF33UL,    0xFF54UL,0xFF34UL,    0xFF55UL,0xFF35UL,
        0xFF56UL,0xFF36UL,    0xFF57UL,0xFF37UL,    0xFF58UL,0xFF38UL,    0xFF59UL,0xFF39UL,
        0xFF5AUL,0xFF3AUL,    0x10428UL,0x10400UL,  0x10429UL,0x10401UL,  0x1042AUL,0x10402UL,
        0x1042BUL,0x10403UL,  0x1042CUL,0x10404UL,  0x1042DUL,0x10405UL,  0x1042EUL,0x10406UL,
        0x1042FUL,0x10407UL,  0x10430UL,0x10408UL,  0x10431UL,0x10409UL,  0x10432UL,0x1040AUL,
        0x10433UL,0x1040BUL,  0x10434UL,0x1040CUL,  0x10435UL,0x1040DUL,  0x10436UL,0x1040EUL,
        0x10437UL,0x1040FUL,  0x10438UL,0x10410UL,  0x10439UL,0x10411UL,  0x1043AUL,0x10412UL,
        0x1043BUL,0x10413UL,  0x1043CUL,0x10414UL,  0x1043DUL,0x10415UL,  0x1043EUL,0x10416UL,
        0x1043FUL,0x10417UL,  0x10440UL,0x10418UL,  0x10441UL,0x10419UL,  0x10442UL,0x1041AUL,
        0x10443UL,0x1041BUL,  0x10444UL,0x1041CUL,  0x10445UL,0x1041DUL,  0x10446UL,0x1041EUL,
        0x10447UL,0x1041FUL,  0x10448UL,0x10420UL,  0x10449UL,0x10421UL,  0x1044AUL,0x10422UL,
        0x1044BUL,0x10423UL,  0x1044CUL,0x10424UL,  0x1044DUL,0x10425UL,  0x1044EUL,0x10426UL,
        0x1044FUL,0x10427UL,
    };
    constexpr size_t last_lower_upper = std::size(lower_upper) - 2;

    size_t i = 0;
    while (i < last_lower_upper and c32 > lower_upper[i])
        i += 2;
    if (c32 == lower_upper[i])
        return lower_upper[i + 1];
    return c32;

    // It's adequate for the purposes of ELIZA, OK?
}


// return given string uppercased and with certain punctuation filtered
std::string eliza_uppercase(const std::string & utf8_string)
{
    /*  Make reasonable efforts to convert the user's text into
        ELIZA-compatible characters. ELIZA was written expecting only
        characters in the BCD character set, so these issues didn't
        arise. This reinterpretation is a matter of taste.

        People may type fancy quotation marks and apostrophes, or copy
        and paste into ELIZA text from documents containing such characters.
        Punctuation, apart from comma and full stop, may get attached to
        words so that they are not recognised: e.g. COMPUTER! is not a
        keyword. We'll remove or reinterpret such punctuation.

        E.g. from https://www.wired.com/story/ai-mother-eliza-claude-therapy/
        dated 27 October 2025, where CLAUDE SONNET 4 is talking to an
        implementation of ELIZA/DOCTOR and says
            "I'm not entirely certain---maybe the nervousness was there before,
             but I only became aware of it when I sat down in this chair."
        Perhaps an '---' (EM DASH) is best interpreted as a comma, otherwise
        someone pasting that sentence into ELIZA/DOCTOR will get the response
            "IS IT BECAUSE YOU ARE NOT ENTIRELY CERTAIN---MAYBE THE NERVOUSNESS
             WAS THERE BEFORE THAT YOU CAME TO ME"
        instead of
            "IS IT BECAUSE YOU ARE NOT ENTIRELY CERTAIN THAT YOU CAME TO ME"
        which seems a little better. */

    std::string result;
    std::u32string utf32(utf8_to_utf32(utf8_string));
    for (auto ch : utf32) {
        const uint32_t c32 = static_cast<uint32_t>(ch);
        switch (c32) {
        case 0x2019:        // 'RIGHT SINGLE QUOTATION MARK' (U+2019)
            result += '\''; //   => 'APOSTROPHE' (U+0027)
            break;          // [hoping I’m will become I'M, for example]

        case 0x2018:        // 'LEFT SINGLE QUOTATION MARK' (U+2018)
        case 0x0060:        // 'GRAVE ACCENT' (U+0060) [backtick]
        case 0x0022:        // 'QUOTATION MARK' (U+0022)
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
            result += ' ';  //   => 'SPACE' (U+0020)
            break;

        case 0x0021:        // 'EXCLAMATION MARK' (U+0021)
        case 0x003F:        // 'QUESTION MARK' (U+003F)
            result += '.';  //   => 'FULL STOP' (U+002E)
            break;

        case 0x00A1:        // 'INVERTED EXCLAMATION MARK' (U+00A1)
        case 0x00BF:        // 'INVERTED QUESTION MARK' (U+00BF)
            result += ' ';  //   => 'SPACE' (U+0020)
            break;

        case 0x003A:        // 'COLON' (U+003A)
        case 0x003B:        // 'SEMICOLON' (U+003B)
        case 0x2013:        // 'EN DASH' (U+2013)
        case 0x2014:        // 'EM DASH' (U+2014)
            result += ',';  //   => 'COMMA' (U+002C)
            break;

        case 0x00DF:        // 'LATIN SMALL LETTER SHARP S' (U+00DF)
            result += "SS";
            break;
        case 0xFB00:        // 'LATIN SMALL LIGATURE FF' (U+FB00)
            result += "FF";
            break;
        case 0xFB01:        // 'LATIN SMALL LIGATURE FI' (U+FB01)
            result += "FI";
            break;
        case 0xFB02:        // 'LATIN SMALL LIGATURE FL' (U+FB02)
            result += "FL";
            break;
        case 0xFB03:        // 'LATIN SMALL LIGATURE FFI' (U+FB03)
            result += "FFI";
            break;
        case 0xFB04:        // 'LATIN SMALL LIGATURE FFL' (U+FB04)
            result += "FFL";
            break;
        case 0xFB05:        // 'LATIN SMALL LIGATURE LONG S T' (U+FB05)
        case 0xFB06:        // 'LATIN SMALL LIGATURE ST' (U+FB06)
            result += "ST";
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
    TEST_EQUAL(eliza_uppercase("Hello! How are you?"),
                               "HELLO. HOW ARE YOU.");
    TEST_EQUAL(eliza_uppercase("Æmilia, æsop & Phœbë"),
                               "ÆMILIA, ÆSOP & PHŒBË");
    TEST_EQUAL(eliza_uppercase("à â ç é è ê ë î ï ô ù û ü ÿ æ œ eﬃgy Straße"),
                               "À Â Ç É È Ê Ë Î Ï Ô Ù Û Ü Ÿ Æ Œ EFFIGY STRASSE");
    TEST_EQUAL(eliza_uppercase("Maroš Šefčovič"),
                               "MAROŠ ŠEFČOVIČ");
    TEST_EQUAL(eliza_uppercase("Сайфи Кудаш Гилемдар Зигандарович"),
                               "САЙФИ КУДАШ ГИЛЕМДАР ЗИГАНДАРОВИЧ");
    TEST_EQUAL(eliza_uppercase("¡pónk!"),
                               " PÓNK.");

    // 'RIGHT SINGLE QUOTATION MARK' (U+2019)
    TEST_EQUAL(eliza_uppercase("I’m depressed"), "I'M DEPRESSED");          // good
    // 'LEFT SINGLE QUOTATION MARK' (U+2018
    // 'RIGHT SINGLE QUOTATION MARK' (U+2019)
    TEST_EQUAL(eliza_uppercase("I'm ‘depressed’"), "I'M  DEPRESSED'");      // oh dear
    // 'QUOTATION MARK' (U+0022)
    TEST_EQUAL(eliza_uppercase("I'm \"depressed\""), "I'M  DEPRESSED ");    // good
    // 'GRAVE ACCENT' (U+0060) [backtick]
    TEST_EQUAL(eliza_uppercase("I'm `depressed`"), "I'M  DEPRESSED ");      // good
    // 'LEFT-POINTING DOUBLE ANGLE QUOTATION MARK' (U+00AB)
    // 'RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK' (U+00BB)
    TEST_EQUAL(eliza_uppercase("I'm «depressed»"), "I'M  DEPRESSED ");      // good
    // 'SINGLE LOW-9 QUOTATION MARK' (U+201A)
    // 'SINGLE HIGH-REVERSED-9 QUOTATION MARK' (U+201B)
    TEST_EQUAL(eliza_uppercase("I'm ‚depressed‛"), "I'M  DEPRESSED ");      // good
    // 'LEFT DOUBLE QUOTATION MARK' (U+201C)
    // 'RIGHT DOUBLE QUOTATION MARK' (U+201D)
    TEST_EQUAL(eliza_uppercase("I'm “depressed”"), "I'M  DEPRESSED ");      // good
    // 'DOUBLE LOW-9 QUOTATION MARK' (U+201E)
    // 'DOUBLE HIGH-REVERSED-9 QUOTATION MARK' (U+201F)
    TEST_EQUAL(eliza_uppercase("I'm „depressed‟"), "I'M  DEPRESSED ");      // good
    // 'SINGLE LEFT-POINTING ANGLE QUOTATION MARK' (U+2039)
    // 'SINGLE RIGHT-POINTING ANGLE QUOTATION MARK' (U+203A)
    TEST_EQUAL(eliza_uppercase("I'm ‹depressed›"), "I'M  DEPRESSED ");      // good

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
        "\xEF\xBD\x9A"      // 'FULLWIDTH LATIN SMALL LETTER Z' (U+FF5A)
        "\xF0\x90\x91\x8F"  // 'DESERET SMALL LETTER EW' (U+1044F)
        "\xEA\x9E\xB7"      // 'LATIN SMALL LETTER OMEGA' (U+A7B7)
        "omega"),
        "ALPHA"
        "\xE2\xB1\xAD"      // 'LATIN CAPITAL LETTER ALPHA' (U+2C6D)
        "\xC3\x87"          // 'LATIN CAPITAL LETTER C WITH CEDILLA' (U+00C7)
        "\xEF\xBF\xBE"      // not a valid unicode character
        "\xEF\xBF\xBF"      // not a valid unicode character
        "\xF0\x90\x80\x80"  // 'LINEAR B SYLLABLE B008 A' (U+10000)
        "\xF0\x90\x80\x81"  // 'LINEAR B SYLLABLE B038 E' (U+10001)
        "\xF4\x8F\xBF\xBD"  // Unicode Character '<Plane 16 Private Use, Last>' (U+10FFFD)
        "\xEF\xBC\xBA"      // 'FULLWIDTH LATIN CAPITAL LETTER Z' (U+FF3A)
        "\xF0\x90\x90\xA7"  // 'DESERET CAPITAL LETTER EW' (U+10427)
        "\xEA\x9E\xB7"      // 'LATIN SMALL LETTER OMEGA' (U+A7B7) [i.e. not uppercased because uppercase_utf32() is not comprehensive]
        "OMEGA");

    TEST_EQUAL(eliza_uppercase(
        "\xE2\x80\x93"      // 'EN DASH' (U+2013)
        "I'm not entirely certain"
        "\xE2\x80\x94"      // 'EM DASH' (U+2014)
        "maybe the nervousness was there before"
        ),
        ","                 // 'COMMA' (U+002C)
        "I'M NOT ENTIRELY CERTAIN"
        ","                 // 'COMMA' (U+002C)
        "MAYBE THE NERVOUSNESS WAS THERE BEFORE"
        );
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
        return t != tags.end() and t->second == stringlist{tag_six_char_matching_behavior};
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
            wildcard_len = static_cast<int>(word_array.size()) - w_begin - fixed_len;
            // if it doesn't match at the end, it doesn't match at all
            wildcard_end = wildcard_len;
        }
        else {
            // this is not the last segment of the whole pattern: it must
            // consume the smallest number of words possible
            wildcard_len = 0;
            // work forwards from the minimum wildcard length to the maximum possible
            wildcard_end = static_cast<int>(word_array.size()) - w_begin - fixed_len;
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
    // My understanding of the SLIP code is that the "ITS" term matches the
    // first ITS in the input, not the second, and I have confirmed this is
    // the case with the original ELIZA code on the 7094 emulator.
    words = { "MARY", "HAD", "A", "LITTLE", "LAMB", "ITS", "PROBABILITY", "AND", "ITS", "LIKELYHOOD", "WERE", "ZERO" };
    pattern = { "1", "0", "2", "ITS", "0" };
    expected = { "MARY", "HAD A", "LITTLE LAMB", "ITS", "PROBABILITY AND ITS LIKELYHOOD WERE ZERO" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);
    // The original ELIZA YMATCH has some issues: the following
    // test gives this incorrect output on the emulator:
    //   "MARY HAD A LITTLE LAMB ITS PROBABILITY AND ITS LIKELYHOOD WERE", "ZERO", "000000", "", "", ""
    words = { "MARY", "HAD", "A", "LITTLE", "LAMB", "ITS", "PROBABILITY", "AND", "ITS", "LIKELYHOOD", "WERE", "ZERO" };
    pattern = { "0", "1", "0", "1", "0", "1" };
    expected = { "", "MARY", "", "HAD", "A LITTLE LAMB ITS PROBABILITY AND ITS LIKELYHOOD WERE", "ZERO" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);
    // ...and fails to match the following at all:
    words = { "MARY", "HAD", "A", "LITTLE", "LAMB", "ITS", "PROBABILITY", "AND", "ITS", "LIKELYHOOD", "WERE", "ZERO" };
    pattern = { "0", "1", "0", "ITS", "0", "1" };
    expected = { "", "MARY", "HAD A LITTLE LAMB", "ITS", "PROBABILITY AND ITS LIKELYHOOD WERE", "ZERO" };
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);
    // Some more hit-and-miss results from the original ELIZA YMATCH:
    words = { "RED" };
    pattern = { "0", "0" };
    expected = { "", "RED" };                                                   // original gives "RED", "001001"
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);
    words = { "RED", "RED" };
    pattern = { "0", "0" };
    expected = { "", "RED RED" };                                               // original gives "RED RED", "" followed by a PROTECTION MODE VIOLATION AT 34624. crash
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);
    words = { "RED", "RED", "RED" };
    pattern = { "0", "0" };
    expected = { "", "RED RED RED" };                                           // original gives "RED RED RED", "000000"
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);
    words = { "GREEN" };
    pattern = { "GREEN", "0", "0" };
    expected = { "GREEN", "", "" };                                             // original gives "GREEN", "", ""
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);
    words = { "GREEN", "GREEN" };
    pattern = { "GREEN", "0", "0" };
    expected = { "GREEN", "", "GREEN" };                                        // original gives "GREEN", "GREEN", ""
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);
    words = { "GREEN", "GREEN", "GREEN" };
    pattern = { "GREEN", "0", "0" };
    expected = { "GREEN", "", "GREEN GREEN" };                                  // original gives "GREEN", "GREEN GREEN", ""
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);
    words = { "PINK" };
    pattern = { "0", "PINK", "0" };
    expected = { "", "PINK", "" };                                              // original gives "", "PINK", ""
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);
    words = { "PINK", "PINK" };
    pattern = { "0", "PINK", "0" };
    expected = { "", "PINK", "PINK" };                                          // original gives "", "PINK", "PINK"
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);
    words = { "PINK", "PINK", "PINK" };
    pattern = { "0", "PINK", "0" };
    expected = { "", "PINK", "PINK PINK" };                                     // original gives "", "PINK", "PINK PINK"
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);
    words = { "BLUE" };
    pattern = { "0", "0", "BLUE" };
    expected = { "", "", "BLUE" };                                              // original gives no match
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);
    words = { "BLUE", "BLUE" };
    pattern = { "0", "0", "BLUE" };
    expected = { "", "BLUE", "BLUE" };                                          // original gives no match
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);
    words = { "BLUE", "BLUE", "BLUE" };
    pattern = { "0", "0", "BLUE" };
    expected = { "", "BLUE BLUE", "BLUE" };                                     // original gives no match
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);
    words = { "YELLOW" };
    pattern = { "YELLOW", "0", "0", "YELLOW" };
    expected = { };                                                             // original gives no match
    TEST_EQUAL(match({}, pattern, words, matching_components), false);
    TEST_EQUAL(matching_components, expected);
    words = { "YELLOW", "YELLOW" };
    pattern = { "YELLOW", "0", "0", "YELLOW" };
    expected = { "YELLOW", "", "", "YELLOW" };                                  // original gives no match followed by PROTECTION MODE VIOLATION AT 34625. crash
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);
    words = { "YELLOW", "YELLOW", "YELLOW" };
    pattern = { "YELLOW", "0", "0", "YELLOW" };
    expected = { "YELLOW", "", "YELLOW", "YELLOW" };                            // original gives no match
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);
    words = { "YELLOW", "YELLOW", "YELLOW", "YELLOW" };
    pattern = { "YELLOW", "0", "0", "YELLOW" };
    expected = { "YELLOW", "", "YELLOW YELLOW", "YELLOW" };                     // original gives no match
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);
    words = { "ORANGE" };
    pattern = { "ORANGE", "0", "ORANGE", "0", "ORANGE" };
    expected = { };                                                             // original gives no match
    TEST_EQUAL(match({}, pattern, words, matching_components), false);
    TEST_EQUAL(matching_components, expected);
    words = { "ORANGE", "ORANGE" };
    pattern = { "ORANGE", "0", "ORANGE", "0", "ORANGE" };
    expected = { };                                                             // original gives no match
    TEST_EQUAL(match({}, pattern, words, matching_components), false);
    TEST_EQUAL(matching_components, expected);
    words = { "ORANGE", "ORANGE", "ORANGE" };
    pattern = { "ORANGE", "0", "ORANGE", "0", "ORANGE" };
    expected = { "ORANGE", "", "ORANGE", "", "ORANGE" };                        // original gives  "ORANGE", "", "ORANGE", "", "ORANGE"
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);
    words = { "ORANGE", "ORANGE", "ORANGE", "ORANGE" };
    pattern = { "ORANGE", "0", "ORANGE", "0", "ORANGE" };
    expected = { "ORANGE", "", "ORANGE", "ORANGE", "ORANGE" };                  // original gives  "ORANGE", "", "ORANGE", "", "ORANGE"
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);
    words = { "ORANGE", "ORANGE", "ORANGE", "ORANGE", "ORANGE" };
    pattern = { "ORANGE", "0", "ORANGE", "0", "ORANGE" };
    expected = { "ORANGE", "", "ORANGE", "ORANGE ORANGE", "ORANGE" };           // original gives  "ORANGE", "", "ORANGE", "", "ORANGE"
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);
    words = { "ORANGE", "ORANGE", "ORANGE", "ORANGE", "ORANGE", "ORANGE" };
    pattern = { "ORANGE", "0", "ORANGE", "0", "ORANGE" };
    expected = { "ORANGE", "", "ORANGE", "ORANGE ORANGE ORANGE", "ORANGE" };    // original gives  "ORANGE", "", "ORANGE", "", "ORANGE"
    TEST_EQUAL(match({}, pattern, words, matching_components), true);
    TEST_EQUAL(matching_components, expected);
    words = { "ORANGE", "THE", "RAIN", "ORANGE", "IN", "SPAIN", "ORANGE" };
    pattern = { "ORANGE", "0", "ORANGE", "0", "ORANGE" };
    expected = { "ORANGE", "THE RAIN", "ORANGE", "IN SPAIN", "ORANGE" };        // original gives  "ORANGE", "THE RAIN", "ORANGE", "IN SPAIN", "ORANGE"
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
        else if (n == 0 or static_cast<size_t>(n) > components.size()) {
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


// Return true iff all decimal numbers in the given reassembly rule are
// valid indexes into the decomposed parts defined by the given decomposition
// rule. If not, a suitable error message is returned in index_out_of_range_msg.
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
        if (n == 0 or static_cast<size_t>(n) > last_dissassembly_part_index) {
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
            const unsigned char uc{ static_cast<unsigned char>(c) };
            result |= hollerith_encoding[uc];
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

// Return an n-bit hash value for the given 36-bit datum d, for n in [0..15].
// Uses the von Neumann middle square method and recreates the SLIP HASH function.
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
    assert(0 <= n and n <= 15);

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
        if (word_substitution_.empty() or word != keyword_)
            return action::inapplicable;
        word = word_substitution_;
        return action::complete;
    }

    // replace 'word' with substitute specified by script rule, if any
    virtual std::string word_substitute(const std::string & word) const
    {
        if (word_substitution_.empty() or word != keyword_)
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

    bool empty() const { return keyword_.empty() or trans_.empty(); }

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
        return !trans_.empty() or !link_keyword_.empty();
    }

    virtual action apply_transformation(
        stringlist & words, const tagmap & tags, std::string & link_keyword)
    {
        trace_begin(words);
        stringlist constituents;
        auto rule = trans_.begin();
        while (rule != trans_.end() and !match(tags, rule->decomposition, words, constituents))
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
        if (reassembly_rule.size() == 1 and reassembly_rule[0] == "NEWKEY")
            return action::newkey; // yes, try the next highest priority keyword, if any

        // is it the special-case reassembly rule (=XXXX)?
        if (reassembly_rule.size() == 2 and reassembly_rule[0].size() == 1 and reassembly_rule[0][0] == '=') {
            link_keyword = reassembly_rule[1];
            return action::linkkey; // yes, try the specified keyword
        }

        // is it the special-case reassembly rule (PRE (reassembly) (=reference))
        // (note: this is the only reassembly_rule that is still in a list)
        if (!reassembly_rule.empty() and reassembly_rule[0] == "(") {
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
            if (t.size() > 1 and t.front() == '/')
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
    return c == ',' or c == '.';
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
        if (ch == ' ' or std::find(punctuation.begin(), punctuation.end(), ch) != punctuation.end()) {
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
    //void set_use_limit(bool f) { limit_ = 2; }

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
            if ((!use_limit_ or limit_ == 4) and mem_rule_->memory_exists()) {
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

            assert(act == rule_base::action::linkkey or act == rule_base::action::newkey);

            if (act == rule_base::action::linkkey)
                keystack.push_front(link_keyword); // rule links to another; loop

            else if (keystack.empty()) {
                // newkey means try next highest keyword, but keystack is empty.
                // The 1966 CACM ELIZA paper, page 41, implies in this situation
                // a NONE message is used. The conversations in the Quarton pilot
                // study suggests that a built-in message is used.
                if (!on_newkey_fail_use_none_ and use_nomatch_msgs_) {
                    trace_->newkey_failed("built-in nomatch");
                    return nomatch_msgs_[limit_ - 1];
                }
                trace_->newkey_failed("NONE");
                break; // (use NONE message)
            }

            // try again with the keyword at the top of the stack
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
    bool symbol(const char * v) const { return t == typ::symbol and value == v; }
    bool number()               const { return t == typ::number; }
    bool open()                 const { return t == typ::open_bracket; }
    bool close()                const { return t == typ::close_bracket; }
    bool eof()                  const { return t == typ::eof; }

    bool operator==(const token & rhs) const { return t == rhs.t and value == rhs.value; }
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
    const static size_t buflen_{ 512 };
    uint8_t buf_[buflen_]{ 0 };
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
            while (peekch(ch) and is_digit(ch)) {
                t.value.push_back(ch);
                nextch(ch);
            }
            return t;
        }

        // everything else is a symbol
        token t(token::typ::symbol);
        t.value.push_back(ch);
        while (peekch(ch) and !non_symbol(ch) and ch != '=') {
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
        return ch <= 0x20 or ch == 0x7F;
        // this must hold: is_newline(ch) implies is_whitespace(ch)
    }

    inline bool is_newline(uint8_t ch) const
    {
        return ch == '\x0A'     // LF
            or ch == '\x0B'     // VT
            or ch == '\x0C'     // FF
            or ch == '\x0D';    // CR
    }

    inline void consume_newline(uint8_t ch)
    {
        if (ch == '\x0D'        // CR
            and peekch(ch)
            and ch == '\x0A')   // LF
            nextch(ch);
        ++line_number_;
    }

    inline bool is_digit(uint8_t ch) const
    {
        return unsigned(ch) - '0' < 10;
    }

    inline bool non_symbol(uint8_t ch) const
    {
        return ch == '(' or ch == ')' or ch == ';' or is_whitespace(ch);
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
        for (const auto [line_number, referenced_keyword] : occurrences_of_references_) {
            const auto r = script_.rules.find(referenced_keyword);
            if (r == std::end(script_.rules)) {
                std::string msg("Script error on line ");
                msg += std::to_string(line_number);
                msg += ": '=";
                msg += referenced_keyword;
                msg += "' referenced keyword does not exist";
                throw std::runtime_error(msg);
            }
            if (!r->second->has_transformation()) {
                std::string msg("Script error on line ");
                msg += std::to_string(line_number);
                msg += ": '=";
                msg += referenced_keyword;
                msg += "' referenced keyword has no associated transformation rules";
                throw std::runtime_error(msg);
            }
        }
    }

private:
    tokenizer<T> tok_;
    script & script_;

    struct ref {
        size_t line_number;
        std::string referenced_keyword;
        ref(size_t line, const std::string & keyword)
        : line_number(line), referenced_keyword(keyword)
        {}
    };
    std::vector<ref> occurrences_of_references_;


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
            for (t = tok_.nexttok(); !t.symbol("=") and !t.eof(); t = tok_.nexttok())
                decomposition.push_back(t.value);
            if (decomposition.empty())
                throw std::runtime_error(errormsg("expected 'decompose_terms = reassemble_terms'" MEMFORM));
            if (!t.symbol("="))
                throw std::runtime_error(errormsg("expected '='" MEMFORM));

            stringlist reassembly;
            for (t = tok_.nexttok(); !t.close() and !t.eof(); t = tok_.nexttok())
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
        if (!tok_.peektok().symbol("PRE")) {
            stringlist reassembly(rdlist(false));
            if (!reassembly.empty() and reassembly[0] == "=") {
                if (reassembly.size() != 2)
                    throw std::runtime_error(errormsg("expected reference keyword to follow '='"));
                occurrences_of_references_.emplace_back(ref(tok_.line(), reassembly[1]));
            }
            return reassembly;
        }

        // It's a PRE reassembly, e.g. (PRE (I ARE 3) (=YOU))
        tok_.nexttok(); // skip "PRE"
        stringlist pre{ "(", "PRE" };
        stringlist reconstruct = rdlist();
        stringlist reference = rdlist();
        if (reference.size() != 2 or reference[0] != "=")
            throw std::runtime_error(errormsg("expected '(=reference)' in PRE rule"));
        occurrences_of_references_.emplace_back(ref(tok_.line(), reference[1]));
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
                    occurrences_of_references_.emplace_back(ref(tok_.line(), t.value));

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
    "\n"
    "\n"
    "; (See https://github.com/anthay/ELIZA for details of this implementation.)\n"
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

const exchange weizenbaum_1966_cacm_conversation[] = {

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
const int weizenbaum_1966_cacm_conversation_size = std::size(weizenbaum_1966_cacm_conversation);




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

        "(REFERENCE\n"
        "    ((0)\n"
        "        (REFERENCE)))\n"

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

        "(REFERENCE\n"
        "    ((0)\n"
        "        (REFERENCE)))\n"

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

    TEST_EQUAL(s.rules.size(), (size_t)29);
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
    status = read_script("()\n(NONE\n((0)()))\r\n(MEMORY KEY(0 = A)(0 = B)(0 = C)(0 = D))\n(KEY((0)(=K)))\n(K4=KEY)");
    TEST_EQUAL(status, "Script error on line 5: '=K' referenced keyword does not exist");
    status = read_script("()\n(NONE\n((0)()))\r\n(MEMORY KEY(0 = A)(0 = B)(0 = C)(0 = D))\n(KEY((0)(=K4)))\n(K4=KEY)");
    TEST_EQUAL(status, "Script error on line 5: '=K4' referenced keyword has no associated transformation rules");
    status = read_script("()\n(NONE\n((0)()))\r\n(MEMORY KEY(0 = A)(0 = B)(0 = C)(0 = D))\n(KEY((0)(=K4)))\n(K4=KEY((0)(HELLO)))");
    TEST_EQUAL(status, "success");
    status = read_script("()\n(NONE\n((0)()))\r\n(MEMORY KEY(0 = A)(0 = B)(0 = C)(0 = D))\n(KEY((0)(=K4))\n(=K))\n(K4=KEY((0)(HELLO)))");
    TEST_EQUAL(status, "Script error on line 6: '=K' referenced keyword does not exist");
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
    for (const auto & [prompt, response] : elizatest::weizenbaum_1966_cacm_conversation)
        TEST_EQUAL(eliza.response(prompt), response);


    // how the conversation might have continued...
    // (to extend the test coverage a little)
    const exchange imagined_continuation_hay_2023[] = {
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

    for (const auto & [prompt, response] : imagined_continuation_hay_2023)
        TEST_EQUAL(eliza.response(prompt), response);
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
    for (const auto & [prompt, response] : alt_men_are_all_alike_convo)
        TEST_EQUAL(eliza.response(prompt), response);
}



DEF_TEST_FUNC(test_every_DOCTOR_response)
{
    const exchange comprehensive_convo[] = {

        // (SORRY
        //     ((0)
        //         (PLEASE DON'T APOLIGIZE)
        //         (APOLOGIES ARE NOT NECESSARY)
        //         (WHAT FEELINGS DO YOU HAVE WHEN YOU APOLOGIZE)
        //         (I'VE TOLD YOU THAT APOLOGIES ARE NOT REQUIRED)))
        { "Sorry, I'm a bit nervous.",
          "PLEASE DON'T APOLIGIZE" },

        // (DONT = DON'T)
        // (CANT = CAN'T)
        // (WONT = WON'T)

        // (REMEMBER 5
        //     ((0 YOU REMEMBER 0)
        //          (DO YOU OFTEN THINK OF 4)
        //          (DOES THINKING OF 4 BRING ANYTHING ELSE TO MIND)
        //          (WHAT ELSE DO YOU REMEMBER)
        //          (WHY DO YOU REMEMBER 4 JUST NOW)
        //          (WHAT IN THE PRESENT SITUATION REMINDS YOU OF 4)
        //          (WHAT IS THE CONNECTION BETWEEN ME AND 4))
        //     ((0 DO I REMEMBER 0)
        //          (DID YOU THINK I WOULD FORGET 5)
        //          (WHY DO YOU THINK I SHOULD RECALL 5 NOW)
        //          (WHAT ABOUT 5)
        //          (=WHAT)
        //          (YOU MENTIONED 5))
        //     ((0)
        //          (NEWKEY)))
        { "I remember The house where I was born, The little window where the sun Came peeping in at morn.",
          "DO YOU OFTEN THINK OF THE HOUSE WHERE YOU WAS BORN" },
        { "Do you remember the Gulf of Mexico?",
          "DID YOU THINK I WOULD FORGET THE GULF OF MEXICO" },

        // (IF 3
        //     ((0 IF 0)
        //         (DO YOU THINK ITS LIKELY THAT 3)
        //         (DO YOU WISH THAT 3)
        //         (WHAT DO YOU THINK ABOUT 3)
        //         (REALLY, 2 3)))
        { "What if the moon is made of cheese?",
          "DO YOU THINK ITS LIKELY THAT THE MOON IS MADE OF CHEESE" },

        // (DREAMT 4
        //     ((0 YOU DREAMT 0)
        //         (REALLY, 4)
        //         (HAVE YOU EVER FANTASIED 4 WHILE YOU WERE AWAKE)
        //         (HAVE YOU DREAMT 4 BEFORE)
        //         (=DREAM)
        //         (NEWKEY)))
        { "I dreamt that I dwelt in marble halls",
          "REALLY, THAT YOU DWELT IN MARBLE HALLS" },

        // (DREAMED = DREAMT 4
        //     (=DREAMT))
        { "I dreamed of you.",
          "HAVE YOU EVER FANTASIED OF I WHILE YOU WERE AWAKE" },

        // (DREAM 3
        //     ((0)
        //         (WHAT DOES THAT DREAM SUGGEST TO YOU)
        //         (DO YOU DREAM OFTEN)
        //         (WHAT PERSONS APPEAR IN YOUR DREAMS)
        //         (DON'T YOU BELIEVE THAT DREAM HAS SOMETHING TO DO WITH YOUR PROBLEM)
        //         (NEWKEY)))
        { "in my dream i was pregnant, but I'm male.",
          "WHAT DOES THAT DREAM SUGGEST TO YOU" },

        // (DREAMS = DREAM 3
        //     (=DREAM))
        { "My dreams are weird.",
          "DO YOU DREAM OFTEN" },

        // (HOW
        //     (=WHAT))
        { "How do you do?",
          "WHY DO YOU ASK" },

        // (WHEN
        //     (=WHAT))
        { "When am I going to be a grown-up?",
          "DOES THAT QUESTION INTEREST YOU" },

        // (ALIKE 10
        //     (=DIT))
        { "he and I are so alike!",
          "IN WHAT WAY" },

        // (SAME 10
        //     (=DIT))
        { "things are not the same between us as they once were",
          "WHAT RESEMBLANCE DO YOU SEE" },

        // (CERTAINLY
        //     (=YES))
        { "Certainly not!",
          "YOU SEEM QUITE POSITIVE" },

        // (FEEL DLIST(/BELIEF))
        // (THINK DLIST(/BELIEF))
        // (BELIEVE DLIST(/BELIEF))
        // (WISH DLIST(/BELIEF))

        // (MEMORY MY
        //     (0 YOUR 0 = LETS DISCUSS FURTHER WHY YOUR 3)
        //     (0 YOUR 0 = EARLIER YOU SAID YOUR 3)
        //     (0 YOUR 0 = BUT YOUR 3)
        //     (0 YOUR 0 = DOES THAT HAVE ANYTHING TO DO WITH THE FACT THAT YOUR 3))

        // (NONE
        //     ((0)
        //         (I AM NOT SURE I UNDERSTAND YOU FULLY)
        //         (PLEASE GO ON)
        //         (WHAT DOES THAT SUGGEST TO YOU)
        //         (DO YOU FEEL STRONGLY ABOUT DISCUSSING SUCH THINGS)))

        // (PERHAPS
        //     ((0)
        //         (YOU DON'T SEEM QUITE CERTAIN)
        //         (WHY THE UNCERTAIN TONE)
        //         (CAN'T YOU BE MORE POSITIVE)
        //         (YOU AREN'T SURE)
        //         (DON'T YOU KNOW)))
        { "Perhaps she'll die!",
          "YOU DON'T SEEM QUITE CERTAIN" },

        // (MAYBE
        //     (=PERHAPS))
        { "Maybe it's because I'm a Londoner",
          "WHY THE UNCERTAIN TONE" },

        // (NAME 15
        //     ((0)
        //         (I AM NOT INTERESTED IN NAMES)
        //         (I'VE TOLD YOU BEFORE, I DON'T CARE ABOUT NAMES - PLEASE CONTINUE)))
        { "A rose by any other name would smell as sweet",
          "I AM NOT INTERESTED IN NAMES" },

        // (DEUTSCH
        //     (=XFREMD))
        // (FRANCAIS
        //     (=XFREMD))
        // (ITALIANO
        //     (=XFREMD))
        // (ESPANOL
        //     (=XFREMD))

        // (XFREMD
        //     ((0)
        //         (I AM SORRY, I SPEAK ONLY ENGLISH)))
        { "Parlez-vous Italiano mademoiselle?",
          "I AM SORRY, I SPEAK ONLY ENGLISH" },

        // (HELLO
        //     ((0)
        //         (HOW DO YOU DO.  PLEASE STATE YOUR PROBLEM)))
        { "Hello there!",
          "HOW DO YOU DO. PLEASE STATE YOUR PROBLEM" },

        // (COMPUTER 50
        //     ((0)
        //         (DO COMPUTERS WORRY YOU)
        //         (WHY DO YOU MENTION COMPUTERS)
        //         (WHAT DO YOU THINK MACHINES HAVE TO DO WITH YOUR PROBLEM)
        //         (DON'T YOU THINK COMPUTERS CAN HELP PEOPLE)
        //         (WHAT ABOUT MACHINES WORRIES YOU)
        //         (WHAT DO YOU THINK ABOUT MACHINES)))
        { "what kind of computer flew to the moon",
          "DO COMPUTERS WORRY YOU" },

        // (MACHINE 50
        //     (=COMPUTER))
        { "You have the soul of a new machine.",
          "WHY DO YOU MENTION COMPUTERS" },

        // (MACHINES 50
        //     (=COMPUTER))
        { "Do you think we will find a cybernetic ecology where we are all watched over by machines of loving grace?",
          "WHAT DO YOU THINK MACHINES HAVE TO DO WITH YOUR PROBLEM" },

        // (COMPUTERS 50
        //     (=COMPUTER))
        { "Pablo Picasso said \"Computers are useless. They can only give you answers.\"",
          "DON'T YOU THINK COMPUTERS CAN HELP PEOPLE" },

        // (AM = ARE
        //     ((0 ARE YOU 0)
        //         (DO YOU BELIEVE YOU ARE 4)
        //         (WOULD YOU WANT TO BE 4)
        //         (YOU WISH I WOULD TELL YOU YOU ARE 4)
        //         (WHAT WOULD IT MEAN IF YOU WERE 4)
        //         (=WHAT))
        //     ((0)
        //         (WHY DO YOU SAY 'AM')
        //         (I DON'T UNDERSTAND THAT)))
        { "Am I a lost cause?",
          "DO YOU BELIEVE YOU ARE A LOST CAUSE" },
        { "Never am, no.",
          "WHY DO YOU SAY 'AM'" },

        // (ARE
        //     ((0 ARE I 0)
        //         (WHY ARE YOU INTERESTED IN WHETHER I AM 4 OR NOT)
        //         (WOULD YOU PREFER IF I WEREN'T 4)
        //         (PERHAPS I AM 4 IN YOUR FANTASIES)
        //         (DO YOU SOMETIMES THINK I AM 4)
        //         (=WHAT))
        //     ((0 ARE 0)
        //         (DID YOU THINK THEY MIGHT NOT BE 3)
        //         (WOULD YOU LIKE IT IF THEY WERE NOT 3)
        //         (WHAT IF THEY WERE NOT 3)
        //         (POSSIBLY THEY ARE 3)))
        { "Are you mad?",
          "WHY ARE YOU INTERESTED IN WHETHER I AM MAD OR NOT" },
        { "Humans eh? Are we nuts?",
          "DID YOU THINK THEY MIGHT NOT BE WE NUTS" },

        // (YOUR = MY
        //     ((0 MY 0)
        //         (WHY ARE YOU CONCERNED OVER MY 3)
        //         (WHAT ABOUT YOUR OWN 3)
        //         (ARE YOU WORRIED ABOUT SOMEONE ELSES 3)
        //         (REALLY, MY 3)))
        { "Your words are starting to annoy me.",
          "WHY ARE YOU CONCERNED OVER MY WORDS ARE STARTING TO ANNOY YOU" },

        // (WAS 2
        //     ((0 WAS YOU 0)
        //         (WHAT IF YOU WERE 4)
        //         (DO YOU THINK YOU WERE 4)
        //         (WERE YOU 4)
        //         (WHAT WOULD IT MEAN IF YOU WERE 4)
        //         (WHAT DOES ' 4 ' SUGGEST TO YOU)
        //         (=WHAT))
        //     ((0 YOU WAS 0)
        //         (WERE YOU REALLY)
        //         (WHY DO YOU TELL ME YOU WERE 4 NOW)
        //         (PERHAPS I ALREADY KNEW YOU WERE 4))
        //     ((0 WAS I 0)
        //         (WOULD YOU LIKE TO BELIEVE I WAS 4)
        //         (WHAT SUGGESTS THAT I WAS 4)
        //         (WHAT DO YOU THINK)
        //         (PERHAPS I WAS 4)
        //         (WHAT IF I HAD BEEN 4))
        //     ((0)
        //         (NEWKEY)))
        { "Was I wrong to speak my mind?",
          "WHAT IF YOU WERE WRONG TO SPEAK YOUR MIND" },
        { "I was telling you about my crazy idea",
          "WERE YOU REALLY" },
        { "That was you trying to make me feel better?",
          "WOULD YOU LIKE TO BELIEVE I WAS TRYING TO MAKE YOU FEEL BETTER" },
        { "That was the problem we had to solve when we found out.",
          "WHAT IS IT YOU REALLY WANT TO KNOW" },

        // (WERE = WAS
        //     (=WAS))
        { "Were you going to tell me how to feel better about myself?",
          "WHAT SUGGESTS THAT I WAS GOING TO TELL YOU HOW TO FEEL BETTER ABOUT YOURSELF" },

        // (ME = YOU)

        // (YOU'RE = I'M
        //     ((0 I'M 0)
        //         (PRE (I ARE 3) (=YOU))))
        { "You're mad at me",
          "WHAT MAKES YOU THINK I AM MAD AT YOU" },

        // (I'M = YOU'RE
        //     ((0 YOU'RE 0)
        //         (PRE (YOU ARE 3) (=I))))
        { "I'm a good girl, I am.",
          "IS IT BECAUSE YOU ARE A GOOD GIRL THAT YOU CAME TO ME" },

        // (MYSELF = YOURSELF)
        // (YOURSELF = MYSELF)

        // (MOTHER DLIST(/NOUN FAMILY))
        // (MOM = MOTHER DLIST(/ FAMILY))
        // (DAD = FATHER DLIST(/ FAMILY))
        // (FATHER DLIST(/NOUN FAMILY))
        // (SISTER DLIST(/FAMILY))
        // (BROTHER DLIST(/FAMILY))
        // (WIFE DLIST(/FAMILY))
        // (CHILDREN DLIST(/FAMILY))

        // (I = YOU
        //     ((0 YOU (* WANT NEED) 0)
        //         (WHAT WOULD IT MEAN TO YOU IF YOU GOT 4)
        //         (WHY DO YOU WANT 4)
        //         (SUPPOSE YOU GOT 4 SOON)
        //         (WHAT IF YOU NEVER GOT 4)
        //         (WHAT WOULD GETTING 4 MEAN TO YOU)
        //         (WHAT DOES WANTING 4 HAVE TO DO WITH THIS DISCUSSION))
        //     ((0 YOU ARE 0 (*SAD UNHAPPY DEPRESSED SICK ) 0)
        //         (I AM SORRY TO HEAR YOU ARE 5)
        //         (DO YOU THINK COMING HERE WILL HELP YOU NOT TO BE 5)
        //         (I'M SURE ITS NOT PLEASANT TO BE 5)
        //         (CAN YOU EXPLAIN WHAT MADE YOU 5))
        //     ((0 YOU ARE 0 (*HAPPY ELATED GLAD BETTER) 0)
        //         (HOW HAVE I HELPED YOU TO BE 5)
        //         (HAS YOUR TREATMENT MADE YOU 5)
        //         (WHAT MAKES YOU 5 JUST NOW)
        //         (CAN YOU EXPLAIN WHY YOU ARE SUDDENLY 5))
        //     ((0 YOU WAS 0)
        //         (=WAS))
        //     ((0 YOU (/BELIEF) YOU 0)
        //         (DO YOU REALLY THINK SO)
        //         (BUT YOU ARE NOT SURE YOU 5)
        //         (DO YOU REALLY DOUBT YOU 5))
        //     ((0 YOU 0 (/BELIEF) 0 I 0)
        //         (=YOU))
        //     ((0 YOU ARE 0)
        //         (IS IT BECAUSE YOU ARE 4 THAT YOU CAME TO ME)
        //         (HOW LONG HAVE YOU BEEN 4)
        //         (DO YOU BELIEVE IT NORMAL TO BE 4)
        //         (DO YOU ENJOY BEING 4))
        //     ((0 YOU (* CAN'T CANNOT) 0)
        //         (HOW DO YOU KNOW YOU CAN'T 4)
        //         (HAVE YOU TRIED)
        //         (PERHAPS YOU COULD 4 NOW)
        //         (DO YOU REALLY WANT TO BE ABLE TO 4))
        //     ((0 YOU DON'T 0)
        //         (DON'T YOU REALLY 4)
        //         (WHY DON'T YOU 4)
        //         (DO YOU WISH TO BE ABLE TO 4)
        //         (DOES THAT TROUBLE YOU))
        //     ((0 YOU FEEL 0)
        //         (TELL ME MORE ABOUT SUCH FEELINGS)
        //         (DO YOU OFTEN FEEL 4)
        //         (DO YOU ENJOY FEELING 4)
        //         (OF WHAT DOES FEELING 4 REMIND YOU))
        //     ((0 YOU 0 I 0)
        //         (PERHAPS IN YOUR FANTASY WE 3 EACH OTHER)
        //         (DO YOU WISH TO 3 ME)
        //         (YOU SEEM TO NEED TO 3 ME)
        //         (DO YOU 3 ANYONE ELSE))
        //     ((0)
        //         (YOU SAY 1)
        //         (CAN YOU ELABORATE ON THAT)
        //         (DO YOU SAY 1 FOR SOME SPECIAL REASON)
        //         (THAT'S QUITE INTERESTING)))
        { "I need to understand why I'm unhappy",
          "WHAT WOULD IT MEAN TO YOU IF YOU GOT TO UNDERSTAND WHY YOU'RE UNHAPPY" },
        { "I know I am sick in the head",
          "I AM SORRY TO HEAR YOU ARE SICK" },
        { "I am not a happy bunny",
          "HOW HAVE I HELPED YOU TO BE HAPPY" },
        { "I were flummoxed",
          "WHY DO YOU TELL ME YOU WERE FLUMMOXED NOW" },
        { "I think I should go.",
          "DO YOU REALLY THINK SO" },
        { "I feel you understand me.",
          "WHY DO YOU THINK I UNDERSTAND YOU" },
        { "I am so lonely now.",
          "HOW LONG HAVE YOU BEEN SO LONELY NOW" },
        { "I cannot believe he really loves me.",
          "HOW DO YOU KNOW YOU CAN'T BELIEVE HE REALLY LOVES YOU" },
        { "I don't get out much now I'm old.",
          "DON'T YOU REALLY GET OUT MUCH NOW YOU'RE OLD" },
        { "I feel lost",
          "TELL ME MORE ABOUT SUCH FEELINGS" },
        { "I hate you",
          "PERHAPS IN YOUR FANTASY WE HATE EACH OTHER" },
        { "I can talk the hind leg off a donkey.",
          "YOU SAY YOU CAN TALK THE HIND LEG OFF A DONKEY" },

        // (YOU = I
        //     ((0 I REMIND YOU OF 0)
        //         (=DIT))
        //     ((0 I ARE 0)
        //         (WHAT MAKES YOU THINK I AM 4)
        //         (DOES IT PLEASE YOU TO BELIEVE I AM 4)
        //         (DO YOU SOMETIMES WISH YOU WERE 4)
        //         (PERHAPS YOU WOULD LIKE TO BE 4))
        //     ((0 I 0 YOU)
        //         (WHY DO YOU THINK I 3 YOU)
        //         (YOU LIKE TO THINK I 3 YOU - DON'T YOU)
        //         (WHAT MAKES YOU THINK I 3 YOU)
        //         (REALLY, I 3 YOU)
        //         (DO YOU WISH TO BELIEVE I 3 YOU)
        //         (SUPPOSE I DID 3 YOU - WHAT WOULD THAT MEAN)
        //         (DOES SOMEONE ELSE BELIEVE I 3 YOU))
        //     ((0 I 0)
        //         (WE WERE DISCUSSING YOU - NOT ME)
        //         (OH, I 3)
        //         (YOU'RE NOT REALLY TALKING ABOUT ME - ARE YOU)
        //         (WHAT ARE YOUR FEELINGS NOW)))
        { "You remind me of the old days.",
          "WHAT DOES THAT SIMILARITY SUGGEST TO YOU" },
        { "You are wrong about that!",
          "DOES IT PLEASE YOU TO BELIEVE I AM WRONG ABOUT THAT" },
        { "Clearly you hate me.",
          "YOU LIKE TO THINK I HATE YOU - DON'T YOU" },
        { "You failed to grasp the nettle.",
          "WE WERE DISCUSSING YOU - NOT ME" },

        // (YES
        //     ((0)
        //         (YOU SEEM QUITE POSITIVE)
        //         (YOU ARE SURE)
        //         (I SEE)
        //         (I UNDERSTAND)))
        { "Yes, I think so.",
          "YOU ARE SURE" },

        // (NO
        //     ((0)
        //         (ARE YOU SAYING 'NO' JUST TO BE NEGATIVE)
        //         (YOU ARE BEING A BIT NEGATIVE)
        //         (WHY NOT)
        //         (WHY 'NO')))
        { "No, I don't want to go out with you.",
          "ARE YOU SAYING 'NO' JUST TO BE NEGATIVE" },

        // (MY = YOUR 2
        //     ((0 YOUR 0 (/FAMILY) 0)
        //         (TELL ME MORE ABOUT YOUR FAMILY)
        //         (WHO ELSE IN YOUR FAMILY 5)
        //         (YOUR 4)
        //         (WHAT ELSE COMES TO MIND WHEN YOU THINK OF YOUR 4))
        //     ((0 YOUR 0)
        //         (YOUR 3)
        //         (WHY DO YOU SAY YOUR 3)
        //         (DOES THAT SUGGEST ANYTHING ELSE WHICH BELONGS TO YOU)
        //         (IS IT IMPORTANT TO YOU THAT 2 3)))
        { "My brother ate my hamster.",
          "TELL ME MORE ABOUT YOUR FAMILY" },
        { "Oh my head hurts!",
          "YOUR HEAD HURTS" },

        // (CAN
        //     ((0 CAN I 0)
        //         (YOU BELIEVE I CAN 4 DON'T YOU)
        //         (=WHAT)
        //         (YOU WANT ME TO BE ABLE TO 4)
        //         (PERHAPS YOU WOULD LIKE TO BE ABLE TO 4 YOURSELF))
        //     ((0 CAN YOU 0)
        //         (WHETHER OR NOT YOU CAN 4 DEPENDS ON YOU MORE THAN ON ME)
        //         (DO YOU WANT TO BE ABLE TO 4)
        //         (PERHAPS YOU DON'T WANT TO 4)
        //         (=WHAT)))
        { "Can you believe it!",
          "YOU BELIEVE I CAN BELIEVE IT DON'T YOU" },
        { "Can I have a glass of water?",
          "WHETHER OR NOT YOU CAN HAVE A GLASS OF WATER DEPENDS ON YOU MORE THAN ON ME" },

        // (WHAT
        //     ((0)
        //         (WHY DO YOU ASK)
        //         (DOES THAT QUESTION INTEREST YOU)
        //         (WHAT IS IT YOU REALLY WANT TO KNOW)
        //         (ARE SUCH QUESTIONS MUCH ON YOUR MIND)
        //         (WHAT ANSWER WOULD PLEASE YOU MOST)
        //         (WHAT DO YOU THINK)
        //         (WHAT COMES TO YOUR MIND WHEN YOU ASK THAT)
        //         (HAVE YOU ASKED SUCH QUESTIONS BEFORE)
        //         (HAVE YOU ASKED ANYONE ELSE)))
        { "What an idiot.",
          "ARE SUCH QUESTIONS MUCH ON YOUR MIND" },

        // (BECAUSE
        //     ((0)
        //         (IS THAT THE REAL REASON)
        //         (DON'T ANY OTHER REASONS COME TO MIND)
        //         (DOES THAT REASON SEEM TO EXPLAIN ANYTHING ELSE)
        //         (WHAT OTHER REASONS MIGHT THERE BE)))
        { "Because then she would know I loved her.",
          "IS THAT THE REAL REASON" },

        // (WHY
        //     ((0 WHY DON'T I 0)
        //         (DO YOU BELIEVE I DON'T 5)
        //         (PERHAPS I WILL 5 IN GOOD TIME)
        //         (SHOULD YOU 5 YOURSELF)
        //         (YOU WANT ME TO 5)
        //         (=WHAT))
        //     ((0 WHY CAN'T YOU 0)
        //         (DO YOU THINK YOU SHOULD BE ABLE TO 5)
        //         (DO YOU WANT TO BE ABLE TO 5)
        //         (DO YOU BELIEVE THIS WILL HELP YOU TO 5)
        //         (HAVE YOU ANY IDEA WHY YOU CAN'T 5)
        //         (=WHAT))
        //     (=WHAT))
        { "Why don't you just switch off your television set and go and do something less boring instead?",
          "DO YOU BELIEVE I DON'T JUST SWITCH OFF MY TELEVISION SET AND GO AND DO SOMETHING LESS BORING INSTEAD" },
        { "why cant i eat cookies in bed",
          "DO YOU THINK YOU SHOULD BE ABLE TO EAT COOKIES IN BED" },
        { "why can't people get along?",
          "WHAT ANSWER WOULD PLEASE YOU MOST" },

        // (EVERYONE 2
        //     ((0 (* EVERYONE EVERYBODY NOBODY NOONE) 0)
        //         (REALLY, 2)
        //         (SURELY NOT 2)
        //         (CAN YOU THINK OF ANYONE IN PARTICULAR)
        //         (WHO, FOR EXAMPLE)
        //         (YOU ARE THINKING OF A VERY SPECIAL PERSON)
        //         (WHO, MAY I ASK)
        //         (SOMEONE SPECIAL PERHAPS)
        //         (YOU HAVE A PARTICULAR PERSON IN MIND, DON'T YOU)
        //         (WHO DO YOU THINK YOU'RE TALKING ABOUT)))
        { "Everyone can get lost!",
          "REALLY, EVERYONE" },

        // (EVERYBODY 2
        //     (= EVERYONE))
        { "Everybody hates me.",
          "SURELY NOT EVERYBODY" },

        // (NOBODY 2
        //     (= EVERYONE))
        { "Nobody loves me.",
          "CAN YOU THINK OF ANYONE IN PARTICULAR" },

        // (NOONE 2
        //     (= EVERYONE))
        { "Noone cares",
          "WHO, FOR EXAMPLE" },

        // (ALWAYS 1
        //     ((0)
        //         (CAN YOU THINK OF A SPECIFIC EXAMPLE)
        //         (WHEN)
        //         (WHAT INCIDENT ARE YOU THINKING OF)
        //         (REALLY, ALWAYS)))
        { "The postman always rings twice.",
          "CAN YOU THINK OF A SPECIFIC EXAMPLE" },

        // (LIKE 10
        //     ((0 (*AM IS ARE WAS) 0 LIKE 0)
        //         (=DIT))
        //     ((0)
        //         (NEWKEY)))
        { "She was not like the others.",
          "WHAT OTHER CONNECTIONS DO YOU SEE" },
        { "I dig you man!",
          "DO YOU WISH TO DIG ME" }
    };


    elizascript::script s;
    elizascript::read(elizascript::CACM_1966_01_DOCTOR_script, s);
    elizalogic::eliza eliza(s.rules, s.mem_rule);
    for (const auto & [prompt, response] : comprehensive_convo)
        TEST_EQUAL(eliza.response(prompt), response);
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
    for (const auto & [prompt, response] : boston_globe_1966_conversation)
        TEST_EQUAL(eliza.response(prompt), response);
}


DEF_TEST_FUNC(test_weizenbaum_march_1965_conversations)
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
        "        (YOU WANT ME TO BE ABLE TO 4)\n"
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

    const exchange weizenbaum_5_march_1965_conversation[] = {

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

    {
        elizascript::script s;
        elizascript::read(script, s);
        elizalogic::eliza eliza(s.rules, s.mem_rule);

        for (const auto & [prompt, response] : weizenbaum_5_march_1965_conversation)
            TEST_EQUAL(eliza.response(prompt), response);
    }


    const exchange weizenbaum_3_march_1965_conversation[] = {

        /* A page of printer output from the Edmund Berkeley Papers in the
           archives of the Charles Babbage Institute (CBI 50 Box 47 Folder 35)
           via a blogpost by Rebecca Roach, Associate Professor of
           Contemporary Literature, University of Birmingham, UK.
           https://sites.google.com/view/elizaarchaeology/blog/7-guest-post-berkeley-and-weizenbaum

           The page is stamped: MAR 3 1965

           There is a handwritten label:  <-- JOE WEIZENBAUM
           (Presumably meaning received from Joe Weizenbaum?)

           The text begins:
                R EBUT
                W 1138.0
           (I think R EBUT is the user asking to run the program called EBUT and W 1138.0
           is the CTSS reply meaning it started waiting for the program to load at 11:38:00.)

           This is followed by the following exchanges. Where the CACM script response is
           different to the 3 March response the latter is shown as a comment. */

        { "I AM AFRAID I AM GOING TO DIE SOON.",
          "IS IT BECAUSE YOU ARE AFRAID YOU ARE GOING TO DIE SOON THAT YOU CAME TO ME" },

        { "YES",
          "YOU SEEM QUITE POSITIVE" },

        { "YES",
          "YOU ARE SURE" },

        { "MY DREAMS REMIND ME THAT I SHALL DIE BEFORE THREE MORE YEARS GO BY",
          "YOUR DREAMS REMIND YOU THAT YOU SHALL DIE BEFORE THREE MORE YEARS GO BY" },

        { "AN D I THINK THAT MY PREMONITIONS WILL COME TRUE",
          "YOU SAY AN D YOU THINK THAT YOUR PREMONITIONS WILL COME TRUE" },

        { "YES",
          "I SEE" },

        { "CAN YOU HELP ME",
          "YOU BELIEVE I CAN HELP YOU DON'T YOU" },

        { "YES",
          "I UNDERSTAND" },

        { "CAN YOU HELP ME",
          "YOU WANT ME TO BE ABLE TO HELP YOU" },

        { "YES, I WOULD LIKE YOU TO USGGEST SOMETING FOR ME TO DO OR THINK ABOUT",
          "YOU SEEM QUITE POSITIVE" },

        { "I DON'T KNOW WHAT TO BELIEVE.",
          "DON'T YOU REALLY KNOW WHAT TO BELIEVE" },

        { "WHAT CAN MAN BELIEVE IN IF NOT LIFE ITSELF",
          "DO YOU THINK ITS LIKELY THAT NOT LIFE ITSELF" },

        { "THAT'S EXEACTLY THE POINT. I AM DYING. YOU ARE PHILOSOPHISING.",
          "HOW LONG HAVE YOU BEEN DYING" },

        { "SINCE I WAS BORN.",
          "CAN YOU ELABORATE ON THAT" },

        { "I HAVE ALWAYS HAD A FEAR OF DEATH.  I DON'T THINK ITS NORMAL.",
          "CAN YOU THINK OF A SPECIFIC EXAMPLE" },

        { "MY DREAMS ARE RICH WITH EXAMPLES. I TOLD YOU THAT ALREADY",
          "WHY DO YOU SAY YOUR DREAMS AM RICH WITH EXAMPLES" },

        // At this point in the conversation Weizenbaum enters '+', which
        // invokes the CHANGE function. (CHANGE is present in the MIT archive
        // ELIZA source code but not described in the CACM ELIZA paper.)
        // ELIZA responds PLEASE INSTRUCT ME and Weizenbaum issues the 
        // TYPE ALWAYS and TYPE I commands, to view the transformation rules
        // associated with ALWAYS and I, followed by the START command to
        // resume the conversation...

        { "I AM TESTING",
          "DO YOU BELIEVE IT NORMAL TO BE TESTING" },

        { "TODAY I HATE YOU DESPERATELY",
          "PERHAPS IN YOUR FANTASY WE HATE EACH OTHER" },

        { "I HATE AND LOVE YOU AT THE SAME TIME",
          "DO YOU WISH TO HATE AND LOVE ME" },

        // end of conversation
    };


    {
        elizascript::script s;
        elizascript::read(script, s);
        elizalogic::eliza eliza(s.rules, s.mem_rule);

        for (const auto & [prompt, response] : weizenbaum_3_march_1965_conversation)
            TEST_EQUAL(eliza.response(prompt), response);
    }
}


DEF_TEST_FUNC(test_weizenbaum_july_1977_conversation)
{
    const exchange weizenbaum_july_1977_conversation[] = {

        /* "Here is part of the printout of a dialogue between patient and computer
            on the DOCTOR program of a large commercial time-sharing company in
            New York. A careful reading, says Joseph Weizenbaum, the program's
            originator, shows the computer/therapist is not really "intelligent"
            but merely following preprogrammed language rules."
            THE LAST DREAM
            by Joseph Weizenbaum
            Page 34 of Across the Board: The Conference Board Magazine Vol. XIV No. 7
            JULY 1977
        */

        //"HOW DO YOU DO. PLEASE STATE YOUR PROBLEM"

        { "MY MOTHER MADE ME COME HERE. I DON'T KNOW WHAT MY PROBLEM IS.",
          "TELL ME MORE ABOUT YOUR FAMILY" },

        { "I HAVE TWO OLDER BROTHERS",
          "YOU SAY YOU HAVE TWO OLDER BROTHERS" },

        { "AND A LITTLE SISTER.  I LIKE HER A LOT",
          "CAN YOU ELABORATE ON THAT" },

        { "YES. I FEEL CLOSE TO MY SISTER, BUT NOT TO MY PARENTS",
          "YOU SEEM QUITE POSITIVE" },

        { "I AM.",
          "IS IT BECAUSE YOU ARE THAT YOU CAME TO ME" },

        { "NO. IT'S BECAUSE I'M UNHAPPY.",
          "ARE YOU SAYING 'NO' JUST TO BE NEGATIVE" },

        { "NO. BECAUSE IT'S TRUE.",
          "YOU ARE BEING A BIT NEGATIVE" },

        { "I ALWAYS AM",
          "CAN YOU THINK OF A SPECIFIC EXAMPLE" },

        { "YES. YESTERDAY I DOUBTED WHETHER I SHOULD COME HERE",
          "YOU ARE SURE" },

        { "YES. I WAS SCARED",
          "I SEE" },

        { "I AM SCARED OF DOCTORS",
          "HOW LONG HAVE YOU BEEN SCARED OF DOCTORS" },

        { "FOREVER. AND I AM SCARED OF MACHINES TOO",
          "DO COMPUTERS WORRY YOU" },

        { "YES. I DON'T UNDERSTAND THEM",
          "I UNDERSTAND" },

        { "HOW CAN YOU?",
          "WHY DO YOU ASK" },

        { "BECAUSE I DON'T THINK YOU CAN UNDERSTAND ME OR HELP ME",
          "IS THAT THE REAL REASON" },

        { "YES.",
          "YOU SEEM QUITE POSITIVE" },

        { "I AM RIGHT NOW. I DON'T ALWAYS FEEL THIS WAY, THOUGH.",
          "DO YOU BELIEVE IT NORMAL TO BE RIGHT NOW" },

        { "I'M NOT SURE I KNOW WHASTNORMAL IS",
          "DO YOU ENJOY BEING NOT SURE YOU KNOW WHASTNORMAL IS" },

        { "THIS IS SILLY",
          //"LETS DISCUSS FURTHER WHY YOUR MOTHER MADE YOU COME HERE"
          //(The order of the MEMORY rules is not the same as in the CACM script?)
          "DOES THAT HAVE ANYTHING TO DO WITH THE FACT THAT YOUR MOTHER MADE YOU COME HERE" },

        { "YOU CHANGED THE SUBJECT. WHY?",
          "WE WERE DISCUSSING YOU - NOT ME" },

        { "OKAY. I GUESS YOU'RE RIGHT. MY MOTHER IS SO PUSHY--LIKE YOU.",
          "DO YOU SAY YOU GUESS I'M RIGHT FOR SOME SPECIAL REASON" },

        { "YES, YOU'RE THE DOCTOR,YOU SHOULD KNOW",
          "YOU ARE SURE" },

        { "OF COURSE. DOCTORS ARE TRAINED, THEY SHOULD UNDERSTAND",
          "DID YOU THINK THEY MIGHT NOT BE TRAINED" },

        { "NOL,I KNOW THEY'RE TRAINED. BUT I'M NT SURE YOU WILL LIKE ME",
          "THAT'S QUITE INTERESTING" },

        { "MOST PEOPLE DON'T LIKE ME",
          "I AM NOT SURE I UNDERSTAND YOU FULLY" },

        { "I AM NOT POPULAR",
          "IS IT BECAUSE YOU ARE NOT POPULAR THAT YOU CAME TO ME" },

        { "YES. CAN YOU HELP ME?",
          "I SEE" },

        { "I HAVE TO GO NOW",
          "YOU SAY YOU HAVE TO GO NOW" },

        { "YES. MY TIME IS UP",
          "I UNDERSTAND" },

        { "GOOD BYE. SEE YOU NEXT WEEK.",
          "OH, I NEXT WEEK" },

        // end of conversation between a definitely real patient and the computer
    };

    elizascript::script s;
    elizascript::read(elizascript::CACM_1966_01_DOCTOR_script, s);
    elizalogic::eliza eliza(s.rules, s.mem_rule);
    for (const auto & [prompt, response] : weizenbaum_july_1977_conversation)
        TEST_EQUAL(eliza.response(prompt), response);
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
#ifndef SUPPORT_SERIAL_IO
    (void)port_name; // avoid "unreferenced formal parameter" warning
#endif
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


        if (!parse_cmdline(argc, argv, showscript, nobanner, quick, help, port, port_name, script_filename) or help) {
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
                << " ELIZA implementation (v1.00) by Anthony Hay, 2020-25  (CC0 1.0)\n"
                << "-----------------------------------------------------------------\n"
                << "Use command line option '" << as_option("help") << "' for usage information.\n";
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
        (void)port_name; // avoid "unreferenced formal parameter" warning
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
                    userinput = elizatest::weizenbaum_1966_cacm_conversation[cacm_index++].prompt;
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
                                return a.second > b.second or (a.second == b.second and a.first < b.first);
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

            if (cacm_index >= elizatest::weizenbaum_1966_cacm_conversation_size) {
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
