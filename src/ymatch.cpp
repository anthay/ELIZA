/*  Here are parts of the SLIP library code from the file 02-000311065.pdf
    in Joseph Weizenbaum's MIT archive reimagined in C++. The purpose
    is to try to aid my understanding of the SLIP library and how ELIZA
    operates.

    This code plays no part in the eliza.cpp implementation of ELIZA.

    This code is incomplete and buggy.

    Anthony C. Hay, March 2024, Devon, UK
*/

#include <cstddef>
#include <vector>
#include <iostream>
#include <cassert>
#include <typeinfo>
#include <sstream>
#include <array>


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











constexpr unsigned char hollerith_undefined = 0xFFu; // (must be > 63)


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


bool hollerith_defined(char c)
{
    static_assert(std::numeric_limits<unsigned char>::min() == 0);
    static_assert(std::numeric_limits<unsigned char>::max() == 255);

    return hollerith_encoding[static_cast<unsigned char>(c)] != hollerith_undefined;
}

uint_least64_t last_chunk_as_bcd(std::string s)
{
    uint_least64_t result = 0;

    auto append = [&](char c) {
        assert(hollerith_defined(c));
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
















namespace slip {


/*  Just enough SLIP to implement YMATCH.
    Based on information in 02-000311065.pdf in Joseph Weizenbaum's MIT archive,
    his 1963 CACM paper, and the The University of Michigan's SLIP manual. */

enum idtype {
    id_datum,       // 0: datum is not a name of a list
    id_list_name,   // 1: datum is a name of a list (i.e. address of list header)
    id_list_header, // 2: word pair is a header of a list
    id_list_reader  // 3: word pair is a reader
};


using machine_word = uint_least64_t;

const unsigned int total_words = 32768;
const machine_word total_address_space_mask = 077777ULL;

std::vector<machine_word> words(total_words);

/*
 Two primitives with identical functions exist whose pur-
 pose is to fetch the contents of words indirectly. The
 duplication serves to avoid difficulties introduced by the
 fixed/floating-point conventions of FORTRAN.
 4. CONT(A)         are functions which have as their values the
 and                information stored in the word the machine
 5. INHALT(A)       address of which appears as an integer in A.
 --Symmetric List Processor, CACM, September 1963

 prinit.fap:
 CONT   CLA*    1,4                                                              000470
        STA     *+1                                                              000480
        CLA     **                                                               000490
        TRA     2,4                                                              000500

 */
machine_word & cont(machine_word address)
{
    return words[address & total_address_space_mask];
}




machine_word sign(machine_word w)   { return (w & 0400000000000ULL) >> 35; }
machine_word id  (machine_word w)   { return (w & 0030000000000ULL) >> 30; }
machine_word lnkl(machine_word w)   { return (w & 0007777700000ULL) >> 15; }
machine_word lnkr(machine_word w)   { return (w & 0000000077777ULL); }
bool positive(machine_word w)       { return (w & 0400000000000ULL) == 0; }
bool negative(machine_word w)       { return (w & 0400000000000ULL) != 0; }

void set_sign(machine_word & w, machine_word v) { w &= ~0400000000000ULL; w |= (v & 01ULL) << 35; }
void set_id  (machine_word & w, machine_word v) { w &= ~0030000000000ULL; w |= (v & 03ULL) << 30; }
void set_lnkl(machine_word & w, machine_word v) { w &= ~0007777700000ULL; w |= (v & 077777ULL) << 15; }
void set_lnkr(machine_word & w, machine_word v) { w &= ~0000000077777ULL; w |= (v & 077777ULL); }

// note use count is in the datum word of a header cell
machine_word usecount(machine_word w)   { return (w & 0000000077777ULL); }
void set_usecount(machine_word & w, machine_word v) { w &= ~0000000077777ULL; w |= (v & 077777ULL); }


void mrkpos(machine_word addr) { set_sign(cont(addr), 0); }
void mrkneg(machine_word addr) { set_sign(cont(addr), 1); }

void make_cell(
    machine_word address,
    machine_word id,
    machine_word left,
    machine_word right,
    machine_word datum = 0)
{
    machine_word & w = cont(address);
    set_sign(w, 0);
    set_id(w, id);
    set_lnkl(w, left);
    set_lnkr(w, right);
    cont(address + 1) = datum;
}

const machine_word unchanged = 0400000000001ULL; // -1

machine_word setdir(
    machine_word id,
    machine_word left,
    machine_word right,
    machine_word & w)
{
    if (id != unchanged)
        set_id(w, id);
    if (left != unchanged)
        set_lnkl(w, left);
    if (right != unchanged)
        set_lnkr(w, right);
    return w;
}

machine_word setind(
    machine_word id,
    machine_word left,
    machine_word right,
    machine_word addr)
{
    return setdir(id, left, right, cont(addr));
}

/*
             EXTERNAL FUNCTION (LST)                                            001230
             NORMAL MODE IS INTEGER                                             001240
             ENTRY TO LCNTR.                                                    001250
             LEVEL = LNKR.(CONT.(LST+1))                                        001260
             T'O DONE                                                           001270
             ENTRY TO LSTNAM.                                                   001280
             LEVEL = LNKL.(CONT.(LST+1))                                        001290
             SETDIR.(0,LEVEL,LEVEL,LEVEL)                                       001300
DONE         FUNCTION RETURN LEVEL                                              001310
             END OF FUNCTION                                                    001320
 */
machine_word lcntr(machine_word lst)
{
    return lnkr(cont(lst + 1));
}
machine_word lstnam(machine_word lst)
{
    machine_word level = lnkl(cont(lst + 1));
    setdir(id_datum, level, level, level);
    return level;
}

void strind(machine_word obj, machine_word addr)
{
    cont(addr) = obj;
}



machine_word lavs; // (list of available space)

unsigned number_of_free_cells()
{
    unsigned count = 0;
    for (machine_word addr = lnkr(cont(lavs)); addr != 0; addr = lnkr(cont(addr)))
        ++count;
    return count;
}

unsigned initas()
{
    // make all words into one list of cells with the first cell being the list header
    lavs = 2;
    const machine_word last_cell = (total_words / 2) * 2 - 2;
    make_cell(lavs, id_list_header, last_cell, lavs + 2);
    for (machine_word address = lavs + 2; address < last_cell; address += 2)
        make_cell(address, id_datum, address - 2, address + 2);
    make_cell(last_cell, id_datum, last_cell - 2, 0);
    return total_words/2 - 1;
}

/*
             EXTERNAL FUNCTION (LST)                                            001130
             NORMAL MODE IS INTEGER                                             001140
             ENTRY TO LISTMT.                                                   001150
             W'R CONT.(LST) .E. CONT.(LNKR.(CONT.(LST))),T'O ZERO               001160
             TEST = 1                                                           001170
             T'O DONE                                                           001180
ZERO         TEST=0                                                             001190
DONE         FUNCTION RETURN TEST                                               001200
             END OF FUNCTION                                                    001210
 */
// return 0 if list with given lst is empty; else return 1
machine_word listmt(machine_word lst)
{
    return cont(lst) != cont(lnkr(cont(lst)));
}


machine_word iralst(machine_word lst);

/*
        FUNCTION NUCELL(X)
        COMMON AVSL
            M = LNKR(AVSL)
            IF (M)1,2,1
      2 PRINT 901
        STOP
      1 IF (ID(CONT(M))-1)3,4,3
      4     CALL IRALST(CONT(M+1))
      3     CALL SETDIR(-1,-1,LNKR(CONT(M)),AVSL)
            CALL STRIND(0,M)
            CALL STRIND(0,M+1)
           NUCELL = M
        RETURN
    901 FORMAT (1H1,6X,55HLIST OF AVAILABLE SPACE EXHAUSTED - PROGRAM TERM
       1INATED  )
        END
 */
machine_word nucell()
{
    machine_word m = lnkr(lavs);
    if (m == 0)
        throw std::runtime_error("nucell(): LIST OF AVAILABLE SPACE EXHAUSTED - PROGRAM TERMINATED");
    if (id(cont(m)) == id_list_name)
        iralst(cont(m + 1));
    setdir(unchanged, unchanged, lnkr(cont(m)), lavs);
    strind(0, m);
    strind(0, m + 1);
    return m;
}


/*
        SUBROUTINE RCELL(CELL)
        COMMON AVSL
        CALL SETIND(-1,-1,CELL,LNKL(AVSL))
        CALL SETDIR(-1,CELL,-1,AVSL)
        CALL SETIND(-1,-1,0,CELL)
        RETURN
        END
 */
void rcell(machine_word cell)
{
    setind(unchanged, unchanged, cell, lnkl(lavs));
    setdir(unchanged, cell, unchanged, lavs);
    setind(unchanged, unchanged, 0, cell);
}



/*
             EXTERNAL FUNCTION(ADDR)                                            001970
             NORMAL MODE IS INTEGER                                             001980
             ENTRY TO REMOVE.                                                   001990
             W'R ID.(CONT.(ADDR)) .E. 2, T'O HEADER                             002000
             IT = CONT.(ADDR +1)                                                002010
             LEFT=LNKL.(CONT.(ADDR))                                            002020
             RIGHT=LNKR.(CONT.(ADDR))                                           002030
             EXECUTE RCELL.(ADDR)                                               002040
             EXECUTE SETIND.(-1,-1,RIGHT,LEFT)                                  002050
             EXECUTE SETIND.(-1,LEFT,-1,RIGHT)                                  002060
             T'O DONE                                                           002070
HEADER       IT=0                                                               002080
             EXECUTE PRNTP.(MESSG)                                              002090
             VECTOR VALUES MESSG=$HEADER REMOVE$,777777777777K                  002100
DONE         FUNCTION RETURN IT                                                 002110
             END OF FUNCTION                                                    002120
 */
machine_word remove(machine_word addr)
{
    if (id(cont(addr)) == id_list_header) {
        std::cout << "remove(): HEADER REMOVE\n";
        return 0;
    }
    const machine_word it = cont(addr + 1);
    machine_word left = lnkl(cont(addr));
    machine_word right = lnkr(cont(addr));
    rcell(addr);
    setind(unchanged, unchanged, right, left);
    setind(unchanged, left, unchanged, right);
    return it;
}

/*
             EXTERNAL FUNCTION (ADDR)                                           001340
             NORMAL MODE IS INTEGER                                             001350
             ENTRY TO LIST.                                                     001360
             CELL=NUCELL.(CELL)                                                 001370
             EXECUTE SETDIR.(0,CELL,CELL,CELL)                                  001380
             EXECUTE SETIND.(2,CELL,CELL,CELL)                                  001390
             W'R ADDR .E. 9, T'O DONE                                           001400
             ADDR=CELL                                                          001410
             EXECUTE SETIND.(-1,-1,1,CELL + 1)                                  001420
DONE         FUNCTION RETURN CELL                                               001430
             END OF FUNCTION                                                    001440
*/
machine_word list(machine_word & cell)
{
    cell = nucell();
    setdir(id_datum, cell, cell, cell);
    setind(id_list_header, cell, cell, cell);
    setind(unchanged, unchanged, 1, cell + 1);
    return cell;
}
machine_word list9()
{
    machine_word cell = nucell();
    setdir(id_datum, cell, cell, cell);
    setind(id_list_header, cell, cell, cell);
    return cell;
}

machine_word iqual(machine_word k, machine_word l)
{
    return !(lnkl(k) == lnkl(l)
            && lnkr(k) == lnkr(l)
            && id(k) == id(l));
}


/*
             EXTERNAL FUNCTION (CANDAT)                                         001460
             NORMAL MODE IS INTEGER                                             001470
             ENTRY TO NAMTST.                                                   001480
             LST=CANDAT                                                         001490
             LIMIT=GETMEM.(0)                                                   001500
             LINK=LNKR.(LST)                                                    001510
             W'R LNKL.(LST) .NE. LINK .OR. LINK .G. LIMIT, T'O NO               001520
             HEAD=CONT.(LINK)                                                   001530
             W'R ID.(HEAD) .NE. 2 .OR. LNKL.(HEAD) .G. LIMIT, T'O NO            001540
             W'R LNKR.(CONT.(LNKL.(HEAD))) .NE. LINK, T'O NO                    001550
             F'N 0                                                              001560
NO           F'N 1                                                              001570
             E'N                                                                001580
 */
machine_word namtst(machine_word lst)
{
    machine_word link = lnkr(lst);
    if (lnkl(lst) != link or link > total_words)
        return 1;
    machine_word head = cont(link);
    if (id(head) != id_list_header)
        return 1;
    if (lnkl(head) > total_words)
        return 1;
    if (lnkr(cont(lnkl(head))) != link) {
        machine_word a = lnkl(head);
        machine_word b = cont(a);
        machine_word c = lnkr(b);
        bool d = c != link;
        return 1;
    }
    return 0;
}

machine_word namlst(machine_word w)
{
    return namtst(w);
}

/*
        FUNCTION LOCT(K)
            IF NAMTST(K)1,2,1
      2     LOCT = k
            RETURN
      1     PRINT 901
            STOP
    901 FORMAT (1H1,94HA LIST WAS REQUIRED AS AN OPERAND BUT WAS NOT FOUND
       1- THE PROGRAM WAS REGRETFULLY TERMINATED .  )
        END
 */
machine_word loct(machine_word k)
{
    if (namtst(k) == 0)
        return k;
    throw std::runtime_error("loct(): LIST WAS REQUIRED AS AN OPERAND BUT WAS NOT FOUND - THE PROGRAM WAS REGRETFULLY TERMINATED .");
}

machine_word newcommon(machine_word obj, machine_word addr)
{
    const machine_word newcell = nucell();
    const machine_word ll = lnkl(cont(addr));
    setind(unchanged, unchanged, newcell, ll);
    setind(unchanged, newcell, unchanged, addr);
    setind(id_datum, ll, addr, newcell);
    if (obj != 0 && namtst(obj) == 0) {
        setind(id_list_name, unchanged, unchanged, newcell);
        setind(unchanged, unchanged, usecount(obj + 1) + 1, obj + 1);
    }
    strind(obj, newcell + 1);
    return newcell;
}
machine_word newtop(machine_word obj, machine_word lst)
{
    return newcommon(obj, lnkr(cont(lst)));
}
machine_word newbot(machine_word obj, machine_word lst)
{
    return newcommon(obj, lst);
}
machine_word lnkbot(machine_word obj, machine_word lst)
{
    if (listmt(lst) != 0)
        mrkneg(lnkl(cont(lst)));
    return newbot(obj, lst);
}
machine_word many(machine_word lst, machine_word obj1, machine_word obj2)
{
    newbot(obj1, lst);
    newbot(obj2, lst);
    return lst;
}


/*
             EXTERNAL FUNCTION (LST)                                            002140
             NORMAL MODE IS INTEGER                                             002150
             ENTRY TO POPBOT.                                                   002160
             IT=REMOVE.(LNKL.(CONT.(LST)))                                      002170
             T'O DONE                                                           002180
             ENTRY TO POPTOP.                                                   002190
             IT=REMOVE.(LNKR.(CONT.(LST)))                                      002200
DONE         FUNCTION RETURN IT                                                 002210
             END OF FUNCTION                                                    002220
 */
machine_word popbot(machine_word lst)
{
    return remove(lnkl(cont(lst)));
}
machine_word poptop(machine_word lst)
{
    return remove(lnkr(cont(lst)));
}

/*
    FUNCTION MTLIST(P)
    COMMON AVSL
       M = LOCT(P)
       IF (LISTMT(P))3,4,3
  3    LR = LNKR(CONT(M))
       LL = LNKL(CONT(M))
       CALL SETIND(-1,M,M,M)
       CALL SETIND(-1,-1,LR,LNKL(AVSL))
       CALL SETDIR(-1,LL,-1,AVSL)
       CALL SETIND(-1,-1,0,LNKL(AVSL))
  4    MTLIST = M
    RETURN
    END
 */
machine_word mtlist(machine_word p)
{
    machine_word m = loct(p);
    if (listmt(p) == 0)
        return m;
    machine_word lr = lnkr(cont(m));
    machine_word ll = lnkl(cont(m));
    setind(unchanged, m, m, m);
    setind(unchanged, unchanged, lr, lnkl(lavs));
    setdir(unchanged, ll, unchanged, lavs);
    setind(unchanged, unchanged, 0, lnkl(lavs));
    return m;
}


/*
            EXTERNAL FUNCTION (LST)                                            000850
            NORMAL MODE IS INTEGER                                             000860
            ENTRY TO IRALST.                                                   000870
            EXECUTE SETIND.(-1,-1,LCNTR.(LST)-1,LST+1)                         000880
            W'R LCNTR.(LST) .NE. 0,TRANSFER TO DONE                            000890
            EXECUTE MTLIST.(LST)                                               000900
            MAYBE = LNKL.(CONT.(LST+1))                                        000910
            W'R MAYBE .E. 0, TRANSFER TO RETHED                                000920
            EXECUTE SETIND.(1,-1,-1,LST)                                       000930
            EXECUTE SETIND.(0,-1,MAYBE,LST+1)                                  000940
RETHED      EXECUTE RCELL.(LST)                                                000950
DONE        FUNCTION RETURN LST                                                000960
            END OF FUNCTION                                                    000970
 */
machine_word iralst(machine_word lst)
{
    setind(unchanged, unchanged, lcntr(lst) - 1, lst + 1);
    if (lcntr(lst) != 0)
        return lst;
    mtlist(lst);
    machine_word maybe = lnkl(cont(lst + 1));
    if (maybe != 0) {
        setind(id_list_name, unchanged, unchanged, lst);
        setind(id_datum, unchanged, maybe, lst + 1);
    }
    rcell(lst);
    return lst;
}

// return a "sequence reader" for the given lst
machine_word seqrdr(machine_word lst)
{
    return cont(lst);
}

// return the datum in the next cell or 0
machine_word seqlr(machine_word & reader, machine_word & flag)
{
    const machine_word addr = lnkr(reader);
    reader = cont(addr);
    switch (id(reader)) {
        case id_datum:          flag = 1; set_sign(flag, 1); return cont(addr + 1);
        case id_list_name:      flag = 0;                    return cont(addr + 1);
        case id_list_header:    flag = 1;                    return 0;
        case id_list_reader:    throw std::runtime_error("seqlr(): unexpected reader in list");
    }
    throw std::runtime_error("seqlr(): unknown id");
}
machine_word seqll(machine_word & reader, machine_word & flag)
{
    const machine_word addr = lnkl(reader);
    reader = cont(addr);
    switch (id(reader)) {
        case id_datum:          flag = 1; set_sign(flag, 1); return cont(addr + 1);
        case id_list_name:      flag = 0;                    return cont(addr + 1);
        case id_list_header:    flag = 1;                    return 0;
        case id_list_reader:    throw std::runtime_error("seqll(): unexpected reader in list");
    }
    throw std::runtime_error("seqll(): unknown id");
}

machine_word top(machine_word lst)  { return cont(lnkr(cont(lst)) + 1); }
machine_word bot(machine_word lst)  { return cont(lnkl(cont(lst)) + 1); }

machine_word makedl(machine_word dlst, machine_word lst)
{
    const machine_word k = lnkl(cont(lst + 1));
    if (k)
        iralst(k);
    set_lnkl(cont(lst+1), dlst);
    set_usecount(cont(dlst+1), usecount(cont(dlst+1)) + 1);
    return lst;
}

/*
      MADATR   MAD
           EXTERNAL FUNCTION(AT,LST)
           NORMAL MODE IS INTEGER
           ENTRY TO MADATR.
           ADDR = LNKL.(CONT.(LST+1))
           W'R ADDR .E. 0, T'O FAIL
START      ADDR=LNKR.(CONT.(ADDR))
           W'R CONT.(ADDR + 1) .E. AT, T'O SUCCES
           ADDR= LNKR.(CONT.(ADDR))
           W'R ID.(CONT.(ADDR)) .E. 2, T'O FAIL
           T'O START
SUCCES     FUNCTION RETURN ADDR
FAIL       ADDR=-1
           T'O SUCCES
           END OF FUNCTION
*/
machine_word madatr(machine_word at, machine_word lst)
{
                                                        //       MADATR   MAD
                                                        //            EXTERNAL FUNCTION(AT,LST)
                                                        //            NORMAL MODE IS INTEGER
                                                        //            ENTRY TO MADATR.
    machine_word addr = lnkl(cont(lst + 1));            //            ADDR = LNKL.(CONT.(LST+1))
    if (addr == 0) goto fail;                           //            W'R ADDR .E. 0, T'O FAIL
start: addr = lnkr(cont(addr));                         // START      ADDR=LNKR.(CONT.(ADDR))
    if (id(cont(addr)) == id_list_header) goto fail;    //            W'R ID.(CONT.(ADDR)) .E. 2, T'O FAIL
    if (cont(addr + 1) == at) goto succes;              //            W'R CONT.(ADDR + 1) .E. AT, T'O SUCCES
    addr = lnkr(cont(addr));                            //            ADDR= LNKR.(CONT.(ADDR))
    if (id(cont(addr)) == id_list_header) goto fail;    //            W'R ID.(CONT.(ADDR)) .E. 2, T'O FAIL
    goto start;                                         //            T'O START
succes: return addr;                                    // SUCCES     FUNCTION RETURN ADDR
fail: addr = 1; set_sign(addr, 1);                      // FAIL       ADDR=-1
    goto succes;                                        //            T'O SUCCES
}                                                       //            END OF FUNCTION


/*
      ITSVAL   MAD
           EXTERNAL FUNCTION(ATRBT,LST)
           NORMAL MODE IS INTEGER
           ENTRY TO ITSVAL.
           W'R LNKL.(CONT.(LST+1)) .E. 0, T'O FAIL
           ADDR = MADATR.(ATRBT,LST)
           W'R ADDR .E. -1, T'O FAIL
           FUNCTION RETURN CONT.(LNKR.(CONT.(ADDR))+1)
FAIL       FUNCTION RETURN 0
           END OF FUNCTION
*/
machine_word itsval(machine_word atrbt, machine_word lst)
{
    machine_word addr;
                                                        //       ITSVAL   MAD
                                                        //            EXTERNAL FUNCTION(ATRBT,LST)
                                                        //            NORMAL MODE IS INTEGER
                                                        //            ENTRY TO ITSVAL.
    if (lnkl(cont(lst + 1)) == 0) goto fail;            //            W'R LNKL.(CONT.(LST+1)) .E. 0, T'O FAIL
    addr = madatr(atrbt, lst);                          //            ADDR = MADATR.(ATRBT,LST)
    if (negative(addr)) goto fail;                      //            W'R ADDR .E. -1, T'O FAIL
    return cont(lnkr(cont(addr)) + 1);                  //            FUNCTION RETURN CONT.(LNKR.(CONT.(ADDR))+1)
fail: return 0;                                         // FAIL       FUNCTION RETURN 0
}                                                       //            END OF FUNCTION


/*
      SUBST    MAD
           EXTERNAL FUNCTION(DATUM,ADDR)
           NORMAL MODE IS INTEGER
           ENTRY TO SUBST.
           PRESNT = CONT.(ADDR+1)
           W'R NAMTST.(PRESNT) .NE. 0, T'O NONAME
           N=NUCELL.(N)
           EXECUTE SETIND.(1,0,0,N)
           EXECUTE STRIND.(PRESNT,N+1)
           EXECUTE RCELL. (N)
           EXECUTE STRIND.(DATUM,ADDR + 1)
NONAME     W'R NAMTST.(DATUM) .E. 0
           EXECUTE SETIND.(1,-1,-1,ADDR)
           COUNT=LNKL.(DATUM)
           EXECUTE SETIND.(-1,-1,LNKR.(CONT.(COUNT+1))+1,COUNT+1)
           EXECUTE STRIND.(DATUM,ADDR+1)
           OTHERWISE
           EXECUTE STRIND.(DATUM,ADDR+1)
           END OF CONDITIONAL
           FUNCTION RETURN PRESNT
           END OF FUNCTION
*/
machine_word subst(machine_word datum, machine_word addr)
{
    machine_word n;
                                                        //       SUBST    MAD
                                                        //            EXTERNAL FUNCTION(DATUM,ADDR)
                                                        //            NORMAL MODE IS INTEGER
                                                        //            ENTRY TO SUBST.
    machine_word presnt = cont(addr + 1);               //            PRESNT = CONT.(ADDR+1)
    if (namtst(presnt) != 0) goto noname;               //            W'R NAMTST.(PRESNT) .NE. 0, T'O NONAME
    n = nucell();                                       //            N=NUCELL.(N)
    setind(1,0,0,n);                                    //            EXECUTE SETIND.(1,0,0,N)
    strind(presnt, n + 1);                              //            EXECUTE STRIND.(PRESNT,N+1)
    rcell(n);                                           //            EXECUTE RCELL. (N)
    strind(datum, addr + 1);                            //            EXECUTE STRIND.(DATUM,ADDR + 1)
noname: if (namtst(datum) == 0) {                       // NONAME     W'R NAMTST.(DATUM) .E. 0
        setind(1, unchanged, unchanged, addr);          //            EXECUTE SETIND.(1,-1,-1,ADDR)
        machine_word count = lnkl(datum);               //            COUNT=LNKL.(DATUM)
        setind(unchanged, unchanged, lnkr(cont(count + 1)) + 1, count + 1);
                                                        //            EXECUTE SETIND.(-1,-1,LNKR.(CONT.(COUNT+1))+1,COUNT+1)
        strind(datum, addr + 1);                        //            EXECUTE STRIND.(DATUM,ADDR+1)
    } else {                                            //            OTHERWISE
        strind(datum, addr + 1);                        //            EXECUTE STRIND.(DATUM,ADDR+1)
    }                                                   //            END OF CONDITIONAL
    return presnt;                                      //            FUNCTION RETURN PRESNT
}                                                       //            END OF FUNCTION


/*
      NEWVAL   MAD
           EXTERNAL FUNCTION(AT,VAL,LST)
           NORMAL MODE IS INTEGER
           ENTRY TO NEWVAL.
           W'R LNKL.(CONT.(LST+1)) .E. 0, T'O NEWDL
           ADDR = MADATR.(AT,LST)
           W'R ADDR .E. -1, T'O NEWAT
           IT = SUBST.(VAL,LNKR.(CONT.(ADDR)))
DONE       FUNCTION RETURN IT
NEWDL      EXECUTE SETIND.(-1,LIST.(IT),-1,LST+1)
NEWAT      IT=LSTNAM.(LST)
           EXECUTE MANY.(IT,AT,VAL)
           IT = VAL
           T'O DONE
           END OF FUNCTION
*/
machine_word newval(machine_word at, machine_word val, machine_word lst)
{
    machine_word addr, it;
                                                        //       NEWVAL   MAD
                                                        //            EXTERNAL FUNCTION(ATRBT,LST)
                                                        //            NORMAL MODE IS INTEGER
    if (lnkl(cont(lst + 1)) == 0) goto newdl;           //            W'R LNKL.(CONT.(LST+1)) .E. 0, T'O NEWDL
    addr = madatr(at, lst);                             //            ADDR = MADATR.(AT,LST)
    if (negative(addr)) goto newat;                     //            W'R ADDR .E. -1, T'O NEWAT
    it = subst(val, lnkr(cont(addr)));                  //            IT = SUBST.(VAL,LNKR.(CONT.(ADDR)))
done: return it;                                        // DONE       FUNCTION RETURN IT
newdl: setind(unchanged, list(it),unchanged, lst + 1);  // NEWDL      EXECUTE SETIND.(-1,LIST.(IT),-1,LST+1)
newat: it = lstnam(lst);                                // NEWAT      IT=LSTNAM.(LST)
    many(it, at, val);                                  //            EXECUTE MANY.(IT,AT,VAL)
    it = val;                                           //            IT = VAL
    goto done;                                          //            T'O DONE
}                                                       //            END OF FUNCTION


/*
      LSSCPY   MAD
           EXTERNAL FUNCTION ( ORGNL,COPY)
           NORMAL MODE IS INTEGER
           ENTRY TO LSSCPY.
           NEWBOT.(ORGNL,LIST.(STACK))
           NEWBOT.(COPY,STACK)
           NEWVAL.(ORGNL,COPY,STACK)
START      W'R LISTMT.(STACK) .E. 0, T'O DONE
           OLD=POPTOP.(STACK)
           LST=POPTOP.(STACK)
           DLIST=LISTNAM.(OLD)
           W'R DLIST .E. 0,T'O GO
           SEE = ITSVAL.(DLIST,STACK)
           W'R SEE .E. 0
           NEXT=LIST.(9)
           MAKEDL.(NEXT,LST)
           NEWBOT.(DLIST,STACK)
           NEWBOT.(NEXT,STACK)
           NEWVAL.(DLIST,NEXT,STACK)
           T'O GO
           O'E
           MAKEDL.(SEE,LST)
           E'L
GO         READER=SEQRDR.(OLD)
READ       DATUM=SEQLR.(READER,FLAG)
           W'R FLAG .L. 0
           W'R READER .L. 0
           MRKNEG.(NEWBOT.(DATUM,LST))
           O'E
           NEWBOT.(DATUM,LST)
           E'L
           OR W'R FLAG .E. 0
           SEE = ITSVAL.(DATUM,STACK)
           W'R SEE .E. 0, T'O NEW
           NEWBOT.(SEE,LST)
           T'O READ
NEW        NEWBOT.(DATUM,STACK)
           NEWBOT.(LIST.(9),STACK)
           NEWBOT.(BOT.(STACK),LST)
           NEWVAL.(DATUM,BOT.(STACK),STACK)
           OTHERWISE
           T'O START
           END OF CONDITIONAL
           T'O READ
DONE       IRALST.(STACK)
           FUNCTION RETURN COPY
           END OF FUNCTION
*/
machine_word lsscpy(machine_word orgnl, machine_word copy)
{
    machine_word stack, old, lst, dlist, see, next, reader, flag, datum;
                                                        //       LSSCPY   MAD
                                                        //            EXTERNAL FUNCTION ( ORGNL,COPY)
                                                        //            NORMAL MODE IS INTEGER
                                                        //            ENTRY TO LSSCPY.
    newbot(orgnl, list(stack));                         //            NEWBOT.(ORGNL,LIST.(STACK))
    newbot(copy, stack);                                //            NEWBOT.(COPY,STACK)
    newval(orgnl, copy, stack);                         //            NEWVAL.(ORGNL,COPY,STACK)
start: if (listmt(stack) == 0) goto done;               // START      W'R LISTMT.(STACK) .E. 0, T'O DONE
    old = poptop(stack);                                //            OLD=POPTOP.(STACK)
    lst = poptop(stack);                                //            LST=POPTOP.(STACK)
    dlist = lstnam(old);                                //            DLIST=LISTNAM.(OLD)
    if (dlist == 0) goto go;                            //            W'R DLIST .E. 0,T'O GO
    see = itsval(dlist, stack);                         //            SEE = ITSVAL.(DLIST,STACK)
    if (see == 0) {                                     //            W'R SEE .E. 0
        next = list9();                                 //            NEXT=LIST.(9)
        makedl(next, lst);                              //            MAKEDL.(NEXT,LST)
        newbot(dlist, stack);                           //            NEWBOT.(DLIST,STACK)
        newbot(next, stack);                            //            NEWBOT.(NEXT,STACK)
        newval(dlist,next, stack);                      //            NEWVAL.(DLIST,NEXT,STACK)
        goto go;                                        //            T'O GO
    } else {                                            //            O'E
        makedl(see, lst);                               //            MAKEDL.(SEE,LST)
    }                                                   //            E'L
go: reader = seqrdr(old);                               // GO         READER=SEQRDR.(OLD)
read: datum = seqlr(reader, flag);                      // READ       DATUM=SEQLR.(READER,FLAG)
    if (negative(flag)) {                               //            W'R FLAG .L. 0
        if (negative(reader)) {                         //            W'R READER .L. 0
            mrkneg(newbot(datum, lst));                 //            MRKNEG.(NEWBOT.(DATUM,LST))
        } else {                                        //            O'E
            newbot(datum, lst);                         //            NEWBOT.(DATUM,LST)
        }                                               //            E'L
    } else if (flag == 0) {                             //            OR W'R FLAG .E. 0
        see = itsval(datum, stack);                     //            SEE = ITSVAL.(DATUM,STACK)
        if (see == 0) goto newlabel;                    //            W'R SEE .E. 0, T'O NEW
        newbot(see, lst);                               //            NEWBOT.(SEE,LST)
        goto read;                                      //            T'O READ
newlabel: newbot(datum, stack);                         // NEW        NEWBOT.(DATUM,STACK)
        newbot(list9(), stack);                         //            NEWBOT.(LIST.(9),STACK)
        newbot(bot(stack), lst);                        //            NEWBOT.(BOT.(STACK),LST)
        newval(datum, bot(stack), stack);               //            NEWVAL.(DATUM,BOT.(STACK),STACK)
    } else {                                            //            OTHERWISE
        goto start;                                     //            T'O START
    }                                                   //            END OF CONDITIONAL
    goto read;                                          //            T'O READ
done: iralst(stack);                                    // DONE       IRALST.(STACK)
    return copy;                                        //            FUNCTION RETURN COPY
}                                                       //            END OF FUNCTION


/*
        LSTEQL  MAD
           EXTERNAL FUNCTION (ONE,OTHER)
           NORMAL MODE IS INTEGER
           ENTRY TO LSTEQL.
           MANY.(LIST.(STACK),ONE,OTHER)
START      W'R LISTMT.(STACK) .E. 0, T'O DONE
           FIRST = POPTOP.(STACK)
           SECOND = POPTOP.(STACK)
           W'R FIRST .E. SECOND, T'O START
           SA=SEQRDR.(FIRST)
           SB=SEQRDR.(SECOND)
READ       DATUMA=SEQLR.(SA,FLAGA)
           DATUMB=SEQLR.(SB,FLAGB)
           W'R FLAGA .NE. FLAGB, T'O FAIL
           W'R FLAGA .L. 0
           W'R DATUMA .NE. DATUMB, T'O FAIL
           OR W'R FLAGA .E. 0
           MANY.(STACK,DATUMA,DATUMB)
           OTHERWISE
           T'O START
           E'L
           T'O READ
FAIL       TEST=-1
           T'O END
DONE       TEST=0
END        IRALST.(STACK)
           FUNCTION RETURN TEST
           END OF FUNCTION
*/
machine_word lsteql(machine_word one, machine_word other)
{
    machine_word stack, first, second, sa, sb, datuma, datumb, flaga, flagb, test;
                                                        //       LSTEQL   MAD
                                                        //            EXTERNAL FUNCTION ( ORGNL,COPY)
                                                        //            NORMAL MODE IS INTEGER
                                                        //            ENTRY TO LSTEQL.
    many(list(stack), one, other);                      //            MANY.(LIST.(STACK),ONE,OTHER)
start: if (listmt(stack) == 0) goto done;               // START      W'R LISTMT.(STACK) .E. 0, T'O DONE
    first = poptop(stack);                              //            FIRST = POPTOP.(STACK)
    second = poptop(stack);                             //            SECOND = POPTOP.(STACK)
    if (first == second) goto start;                    //            W'R FIRST .E. SECOND, T'O START
    sa = seqrdr(first);                                 //            SA=SEQRDR.(FIRST)
    sb = seqrdr(second);                                //            SB=SEQRDR.(SECOND)
read: datuma = seqlr(sa, flaga);                        // READ       DATUMA=SEQLR.(SA,FLAGA)
    datumb = seqlr(sb, flagb);                          //            DATUMB=SEQLR.(SB,FLAGB)
    if (flaga != flagb) goto fail;                      //            W'R FLAGA .NE. FLAGB, T'O FAIL
    if (negative(flaga)) {                              //            W'R FLAGA .L. 0
        if (datuma != datumb) goto fail;                //            W'R DATUMA .NE. DATUMB, T'O FAIL
    } else if (flaga == 0) {                            //            OR W'R FLAGA .E. 0
        many(stack, datuma, datumb);                    //            MANY.(STACK,DATUMA,DATUMB)
    } else {                                            //            OTHERWISE
        goto start;                                     //            T'O START
    }                                                   //            E'L
    goto read;                                          //            T'O READ
fail:test = 1; set_sign(test, 1);                       //            FAIL       TEST=-1
    goto end;                                           //            T'O END
done:test = 0;                                          // DONE       TEST=0
end:iralst(stack);                                      // END        IRALST.(STACK)
    return test;                                        //            FUNCTION RETURN TEST
}                                                       //            END OF FUNCTION


/*
    "INLSTL(M, A)
    M must be the ALIAS of a list and A the machine address of a list cell.
    INLSTL takes the set of linked cells constituting the body of the list M,
    i.e., all but the HEADER of that list, and inserts it to the left of the
    cell A. It thus lengthens the list of which A is a member by whatever the
    length of M was. M is made into an empty list and its NAME delivered as
    the value of this function.

    INLSTR(M, A)
    Functions just as does INLSTL except that the list M is inserted to the
    right of the list cell whose address is specified by A."

    -- From Weizenbaum's 1963 CACM paper on SLIP

    FUNCTION INLSTL(M,N)
        L = LOCT(M)
        ITOP = LNKR(CONT(L))
        IBOT = LNKL(CONT(L))
    INLSTL = L
    CALL SETIND(-1,L,L,L)
        IPRE = LNKR(CONT(N))   [bug? should be LNKL?]
    CALL SETIND(-1,IBOT,-1,N)
    CALL SETIND(-1,-1,ITOP,IPRE)
    CALL SETIND(-1,IPRE,-1,ITOP)
    CALL SETIND(-1,-1,N,IBOT)
    RETURN
    END

    FUNCTION INLSTR(M,N)
        L = LOCT(M)
        ITOP = LNKR(CONT(L))
        IBOT = LNKL(CONT(L))
    INLSTR = L
    CALL SETIND(-1,L,L,L)
        ISUC = LNKR(CONT(N))
    CALL SETIND(-1,-1,ITOP,N)
    CALL SETIND(-1,IBOT,-1,ISUC)
    CALL SETIND(-1,N,-1,ITOP)
    CALL SETIND(-1,-1,ISUC,IBOT)
    RETURN
    END
*/
machine_word inlstl(machine_word m, machine_word n)
{
                                                        // FUNCTION INLSTL(M,N)
    machine_word l = loct(m);                           //     L = LOCT(M)
    machine_word itop = lnkr(cont(l));                  //     ITOP = LNKR(CONT(L))
    machine_word ibot = lnkl(cont(l));                  //     IBOT = LNKL(CONT(L))
    machine_word inlstl_value = l;                      // INLSTL = L
    setind(unchanged, l, l, l);                         // CALL SETIND(-1,L,L,L)
    machine_word ipre = lnkl(cont(n));                  //     IPRE = LNKR(CONT(N))   [bug? should be LNKL?]
    setind(unchanged, ibot, unchanged, n);              // CALL SETIND(-1,IBOT,-1,N)
    setind(unchanged, unchanged, itop, ipre);           // CALL SETIND(-1,-1,ITOP,IPRE)
    setind(unchanged, ipre, unchanged, itop);           // CALL SETIND(-1,IPRE,-1,ITOP)
    setind(unchanged, unchanged, n, ibot);              // CALL SETIND(-1,-1,N,IBOT)
    return inlstl_value;                                // RETURN
}                                                       // END




template <typename output_device>
machine_word not_slip_lprint(machine_word lst, output_device & tape)
{
    static constexpr unsigned char bcd[64] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 0, '=', '\'', 0, 0, 0, // 00-17
        '+', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 0, '.', ')',  0, 0, 0, // 20-37
        '-', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 0, '$', '*',  0, 0, 0, // 40-57
        ' ', '/', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 0, ',', '(',  0, 0, 0  // 60-77
    };

    auto place = [&](machine_word w){
        if ((w & 0770000000000ULL) == 0)
            tape << w; // if top 6 bits are zero assume it's a numeric value
        else {
            char str[7] = {};
            for (int i = 5; i > -1; --i)
                str[5-i] = bcd[(w >> 6 * i) & 077];
            for (int i = 5; i > 0 && str[i] == ' '; --i)
                str[i] = '\0';
            tape << str;
        }
        };

    tape << "( ";
    const machine_word dlist = lstnam(lst);
    if (dlist != 0) {
        tape << "DLIST ";
        not_slip_lprint(dlist, tape);
    }
    machine_word s = seqrdr(lst);
    for (;;) {
        machine_word flag;
        machine_word word = seqlr(s, flag);
        if (flag == 1)  // header
            break;
        if (negative(flag)) { // datum
            place(word);
            if (positive(s))
                tape << ' ';
        } else {        // sub-list
            not_slip_lprint(word, tape);
        }
    }
    tape << ") ";
    return lst;
}

std::string not_slip_lprintstr(machine_word lst)
{
    std::stringstream s;
    not_slip_lprint(lst, s);
    return s.str(); 
};




/*
        PARTN   MAD
           EXTERNAL FUNCTION(SLST,PART,SIGNAL)
           NORMAL MODE IS INTEGER
           ENTRY TO PARTN.
           TAG=SIGNAL
           COUNT=0
           READER=SEQRDR.(SLST)
READ       COUNT=COUNT+1
           DATUM=SEQLR.(READER,FLAG)
           W'R FLAG .G. 0, T'O DONE
           W'R LNKL.(DATUM) .E. 0
           PART(COUNT) = DATUM
           T'O READ
           O'E
           W'R NAMLST.(DATUM) .NE. 0, T'O PLAIN
           W'R TOP.(DATUM) .NE. TAG, T'O PLAIN
           COUNT=COUNT-1
           LSSCPY.(DATUM,LIST.(IT))
           POPTOP.(IT)
           MAKEDL.(IT,PART(COUNT))
           IRALST.(IT)
           T'O READ
PLAIN      NEWBOT.(DATUM,LIST.(PART(COUNT)))
ATTCH      W'R READER .GE. 0, T'O READ
           LNKBOT.(SEQLR.(READER,FLAG),PART(COUNT))
           T'O ATTCH
           E'L
DONE       COUNT=COUNT-1
           PART(0)=COUNT
           F'N COUNT
           E'N
*/
machine_word partn(machine_word slst, std::vector<machine_word> & part, machine_word signal)
{
    ///
    std::cout << "partn{ " << not_slip_lprintstr(slst) << "} ->\n";
    ///
    machine_word flag, it;
                                                        //       PARTN     MAD
                                                        //            EXTERNAL FUNCTION(SLST,PART,SIGNAL)
                                                        //            NORMAL MODE IS INTEGER
                                                        //            ENTRY TO PARTN.
    const machine_word tag = signal;                    //            TAG=SIGNAL
    machine_word count = 0;                             //            COUNT=0
    machine_word reader = seqrdr(slst);                 //            READER=SEQRDR.(SLST)
read: ++count;                                          // READ       COUNT=COUNT+1
    machine_word datum = seqlr(reader, flag);           //            DATUM=SEQLR.(READER,FLAG)
    if (flag == 1) goto done;                           //            W'R FLAG .G. 0, T'O DONE
    if (lnkl(datum) == 0) {                             //            W'R LNKL.(DATUM) .E. 0
        part[count] = datum;                            //            PART(COUNT) = DATUM
        goto read;                                      //            T'O READ
    } else {                                            //            O'E
        if (namlst(datum) != 0) goto plain;             //            W'R NAMLST.(DATUM) .NE. 0, T'O PLAIN
        ///
        std::cout << "  datum = " << not_slip_lprintstr(datum) << "\n";
        std::cout << "  top(datum) = " << std::hex << top(datum) << std::dec << "\n";
        std::cout << "  tag = " << tag << "\n";
        ///
        if (top(datum) != tag) goto plain;              //            W'R TOP.(DATUM) .NE. TAG, T'O PLAIN
        --count;                                        //            COUNT=COUNT-1
        lsscpy(datum, list(it));                        //            LSSCPY.(DATUM,LIST.(IT))
        poptop(it);                                     //            POPTOP.(IT)
        makedl(it, part[count]);                        //            MAKEDL.(IT,PART(COUNT))
        iralst(it);                                     //            IRALST.(IT)
        goto read;                                      //            T'O READ
plain: newbot(datum, list(part[count]));                // PLAIN      NEWBOT.(DATUM,LIST.(PART(COUNT)))
attch: if (positive(reader)) goto read;                 // ATTCH      W'R READER .GE. 0, T'O READ
        lnkbot(seqlr(reader, flag), part[count]);       //            LNKBOT.(SEQLR.(READER,FLAG),PART(COUNT))
        goto attch;                                     //            T'O ATTCH
    }                                                   //            E'L
done: --count;                                          // DONE       COUNT=COUNT-1
    part[0] = count;                                    //            PART(0)=COUNT
    ///
    for (machine_word i = 0; i < count; ++i) {
        std::cout << "  " << i+1 << " ";
        if (lnkl(part[i+1]) == 0)
            std::cout << part[i+1] << "\n";
        else
            std::cout << not_slip_lprintstr(part[i+1]) << "\n";
    }
    ///
    return count;                                       //            F'N COUNT
}                                                       //            E'N


/*
      XLOOK    MAD
           EXTERNAL FUNCTION(VALUE,LST)
           NORMAL MODE IS INTEGER
           ENTRY TO XLOOK.
           DL=LSTNAM.(LST)
           W'R DL .E. 0, T'O FAIL
           S=SEQRDR.(DL)
READ       WORD=SEQLR.(S,F)
           W'R F .G. 0, T'O FAIL
           W'R WORD .E. VALUE,T'O SUCCES
           T'O READ
FAIL       F'N 1
SUCCES     F'N 0
           E'N
*/
machine_word xlook(machine_word value, machine_word lst)
{
    machine_word s, word, f;
                                                        //       XLOOK    MAD
                                                        //            EXTERNAL FUNCTION(VALUE,LST)
                                                        //            NORMAL MODE IS INTEGER
                                                        //            ENTRY TO XLOOK.
    machine_word dl = lstnam(lst);                      //            DL=LSTNAM.(LST)
    if (dl == 0) goto fail;                             //            W'R DL .E. 0, T'O FAIL
    s = seqrdr(dl);                                     //            S=SEQRDR.(DL)
read: word = seqlr(s, f);                               // READ       WORD=SEQLR.(S,F)
    if (f == 1) goto fail;                              //            W'R F .G. 0, T'O FAIL
    if (word == value) goto succes;                     //            W'R WORD .E. VALUE,T'O SUCCES
    goto read;                                          //            T'O READ
fail: return 1;                                         // FAIL       F'N 1
succes: return 0;                                       // SUCCES     F'N 0
}                                                       //            E'N


/*
      GOODY    MAD
           EXTERNAL FUNCTION (LST,B)
           NORMAL MODE IS INTEGER
           ENTRY TO GOODY.
           W'R TOP.(LST) .E. $/$
           S=SEQRDR.(LST)
READ       WORD=SEQLL.(S,F)
           W'R WORD .E. $/$,T'O FAIL
           W'R XLOOK.(WORD,B) .E. 0,T'O SUCCES
           T'O READ
           O'E
           W'R TOP.(LST) .NE. $*$, T'O FAIL
           S=SEQRDR.(LST)
           SEQLR.(S,F)
           LIST.(TEMP)
RDA        WORD=SEQLR.(S,F)
           W'R F .G. 0, T'O FAILA
PUT        NEWBOT.(WORD,TEMP)
TST        W'R S .GE. 0
           W'R LSTEQL.(TEMP,B) .E. 0, T'O GOOD
           MTLIST.(TEMP)
           T'O RDA
           O'E
RDB        WORD=SEQLR.(S,F)
           T'O PUT
           E'L
           E'L
GOOD       IRALST.(TEMP)
SUCCES     F'N 0
FAILA      IRALST.(TEMP)
FAIL       F'N 1
           E'N
*/
machine_word goody(machine_word lst, machine_word b)
{
    constexpr machine_word solidus = 0616060606060ULL;
    constexpr machine_word asterisk = 0546060606060ULL;
    machine_word s, f, word, temp;
                                                        //       GOODY    MAD
                                                        //            EXTERNAL FUNCTION (LST,B)
                                                        //            NORMAL MODE IS INTEGER
                                                        //            ENTRY TO GOODY.
    if (top(lst) == solidus) {                          //            W'R TOP.(LST) .E. $/$
        s = seqrdr(lst);                                //            S=SEQRDR.(LST)
read:   word = seqll(s, f);                             // READ       WORD=SEQLL.(S,F)
        if (word == solidus) goto fail;                 //            W'R WORD .E. $/$,T'O FAIL
        if (xlook(word, b) == 0) goto succes;           //            W'R XLOOK.(WORD,B) .E. 0,T'O SUCCES
        goto read;                                      //            T'O READ
    } else {                                            //            O'E
        if (top(lst) != asterisk) goto fail;            //            W'R TOP.(LST) .NE. $*$, T'O FAIL
        s = seqrdr(lst);                                //            S=SEQRDR.(LST)
        seqlr(s, f);                                    //            SEQLR.(S,F)
        list(temp);                                     //            LIST.(TEMP)
rda:    word = seqlr(s, f);                             // RDA        WORD=SEQLR.(S,F)
        if (f == 1) goto faila;                         //            W'R F .G. 0, T'O FAILA
put:    newbot(word, temp);                             // PUT        NEWBOT.(WORD,TEMP)
        if (positive(s)) {                              // TST        W'R S .GE. 0
            if (lsteql(temp, b) == 0) goto good;        //            W'R LSTEQL.(TEMP,B) .E. 0, T'O GOOD
            mtlist(temp);                               //            MTLIST.(TEMP)
            goto rda;                                   //            T'O RDA
        } else {                                        //            O'E
            word = seqlr(s, f);                         // RDB        WORD=SEQLR.(S,F)
            goto put;                                   //            T'O PUT
        }                                               //            E'L
    }                                                   //            E'L
good: iralst(temp);                                     // GOOD       IRALST.(TEMP)
succes: return 0;                                       // SUCCES     F'N 0
faila: iralst(temp);                                    // FAILA      IRALST.(TEMP)
fail: return 1;                                         // FAIL       F'N 1
}                                                       //            E'N


/*
      XMATCH   MAD
           EXTERNAL FUNCTION(A,B,AA,AB,AC,BA)
           NORMAL MODE IS INTEGER
           ENTRY TO XMATCH.
           BLAST=B(0)
           LIST.(NUMBER)
           W'R LNKL.(A(AB)) .NE. 0, T'O NORMAL
           W'R BA .E. 1 .AND. A(AA) .NE. 0
           BB=1
           T'H SUMB, FOR I=1,1, I.G. AB
SUMB       BB=BB + A(I)
           BMARK=BB
           O'E
           BB = BLAST + 1
           E'L
           AB=AC
           T'O ENDSTR
NORMAL     AMARK=AB
           BMARK=BA
           T'H INIT, FOR I=AA,1, I .E. AB
INIT       BMARK=BMARK+A(I)
START      OBJ=A(AB)
           T'H LOCATE, FOR I=BMARK,1, I .G. BLAST
           W'R LSTEQL.(OBJ,B(I)) .E. 0, T'O GOOD
           W'R NAMTST.(TOP.(OBJ)) .E. 0
           LST=TOP.(OBJ)
           W'R GOODY.(LST,B(I)) .NE. 0, T'O LOCATE
           T'O GOOD
           O'E
           T'O LOCATE
           E'L
LOCATE     CONTINUE
           T'O FAIL
GOOD       BMARK=1
           BB=1
           OBJ=1
           T'O FOUND
GO         AMARK=AMARK+1
           W'R AMARK .E. AC, T'O ENDSTR
           W'R BMARK .G. BLAST, T'O FAIL
           OBJ=A(AMARK)
           W'R LNKL.(OBJ) .E. 0
FOUND      NEWBOT.(SETDIR.(0,OBJ,BMARK,IT),NUMBER)
           BMARK=BMARK+OBJ
           T'O GO
           O'E
           W'R LSTEQL.(OBJ,B(BMARK)) .E. 0
           OBJ=1
           T'O FOUND
           O'E
           W'R NAMTST.(TOP.(OBJ)) .NE. 0, T'O FEHLER
           LST=TOP.(OBJ)
           W'R GOODY.(LST,B(BMARK)) .NE. 0, T'O FEHLER
           OBJ=1
           T'O FOUND
FEHLER     MTLIST.(NUMBER)
           BMARK=BB+1
           AMARK=AB
           T'O START
           E'L
           E'L
ENDSTR     T'H MORE , FOR I=AB-1,-1, I .L. AA
           OBJ=A(I)
           LIST.(A(I))
           W'R OBJ .E. 0, OBJ = BB-BA
           BB=BB - OBJ
           W'R BB .L. BA, T'O FAIL
           T'H CONC, FOR J=0,1, J .E. OBJ
CONC       INLSTL.(B(BB+J),A(I))
MORE       CONTINUE
           I=AB-1
FORWRD     W'R LISTMT.(NUMBER) .E. 0
           IRALST.(NUMBER)
           BA=BMARK
           F'N 0
           O'E
           I=I+1
           IT=POPTOP.(NUMBER)
           OBJ=LNKL.(IT)
           J=LNKR.(IT)
           W'R NAMTST.(A(I)) .E. 0, IRALST.(A(I))
           LIST.(A(I))
           T'H PLACE, FOR K=0,1, K .E. OBJ
PLACE      INLSTL.(B(J+K),A(I))
           E'L
           T'O FORWRD
FAIL       IRALST.(NUMBER)
           F'N 1
           E'N
*/
#if 1
machine_word xmatch(
    std::vector<machine_word> & a,  // the pattern rules, one element per slot
    std::vector<machine_word> & b,  // the text to match, one word per slot
    const machine_word aa,
    machine_word & ab,
    const machine_word ac,          // index into a where matching should stop
    machine_word & ba)              // index into b just after pattern matching ended
{
    ///
    std::cout
        << "xmatch{\n  aa = " << aa
        << "\n  ab = " << ab
        << "\n  ac = " << ac
        << "\n  ba = " << ba
        << "}\n";
    ///
    machine_word number, bb, amark, obj, lst, ii, jj, it;
    machine_word bmark = 0;//TBD bug? uninitialised in original
                                                        //       XMATCH   MAD
                                                        //            EXTERNAL FUNCTION(A,B,AA,AB,AC,BA)
                                                        //            NORMAL MODE IS INTEGER
                                                        //            ENTRY TO XMATCH.
    const machine_word blast = b[0];                    //            BLAST=B(0)
    list(number);                                       //            LIST.(NUMBER)
    if (lnkl(a[ab]) != 0) goto normal;                  //            W'R LNKL.(A(AB)) .NE. 0, T'O NORMAL
    if (ba == 1 && a[aa] != 0) {                        //            W'R BA .E. 1 .AND. A(AA) .NE. 0
        bb = 1;                                         //            BB=1
        for (machine_word i = 1; i <= ab; ++i)          //            T'H SUMB, FOR I=1,1, I.G. AB
            bb += a[i];                                 // SUMB       BB=BB + A(I)
        bmark = bb;                                     //            BMARK=BB
    } else {                                            //            O'E
        bb = blast + 1;                                 //            BB = BLAST + 1
    }                                                   //            E'L
    ab = ac;                                            //            AB=AC
    goto endstr;                                        //            T'O ENDSTR
normal: amark = ab;                                     // NORMAL     AMARK=AB
    bmark = ba;                                         //            BMARK=BA
    for (machine_word i = aa; i != ab; ++i)             //            T'H INIT, FOR I=AA,1, I .E. AB
        bmark += a[i];                                  // INIT       BMARK=BMARK+A(I)
start: obj = a[ab];                                     // START      OBJ=A(AB)
    for (machine_word i = bmark; i <= blast; ++i) {     //            T'H LOCATE, FOR I=BMARK,1, I .G. BLAST
        if (lsteql(obj, b[i]) == 0) goto good;          //            W'R LSTEQL.(OBJ,B(I)) .E. 0, T'O GOOD
        if (namtst(top(obj)) == 0) {                    //            W'R NAMTST.(TOP.(OBJ)) .E. 0
            lst = top(obj);                             //            LST=TOP.(OBJ)
            if(goody(lst, b[i]) != 0) continue;         //            W'R GOODY.(LST,B(I)) .NE. 0, T'O LOCATE
            goto good;                                  //            T'O GOOD
        } else {                                        //            O'E
            continue;                                   //            T'O LOCATE
        }                                               //            E'L
    }                                                   // LOCATE     CONTINUE
    goto fail;                                          //            T'O FAIL
good: bmark = 1;                                        // GOOD       BMARK=1
    bb = 1;                                             //            BB=1
    obj = 1;                                            //            OBJ=1
    goto found;                                         //            T'O FOUND
go: ++amark;                                            // GO         AMARK=AMARK+1
    if (amark == ac) goto endstr;                       //            W'R AMARK .E. AC, T'O ENDSTR
    if (bmark > blast) goto fail;                       //            W'R BMARK .G. BLAST, T'O FAIL
    obj = a[amark];                                     //            OBJ=A(AMARK)
    if (lnkl(obj) == 0) {                               //            W'R LNKL.(OBJ) .E. 0
found:  newbot(setdir(0, obj, bmark, it), number);      // FOUND      NEWBOT.(SETDIR.(0,OBJ,BMARK,IT),NUMBER)
        bmark += obj;                                   //            BMARK=BMARK+OBJ
        goto go;                                        //            T'O GO
    } else {                                            //            O'E
        if (lsteql(obj, b[bmark]) == 0) {               //            W'R LSTEQL.(OBJ,B(BMARK)) .E. 0
            obj = 1;                                    //            OBJ=1
            goto found;                                 //            T'O FOUND
        } else {                                        //            O'E
            if (namtst(top(obj)) != 0) goto fehler;     //            W'R NAMTST.(TOP.(OBJ)) .NE. 0, T'O FEHLER
            lst = top(obj);                             //            LST=TOP.(OBJ)
            if (goody(lst, b[bmark]) != 0) goto fehler; //            W'R GOODY.(LST,B(BMARK)) .NE. 0, T'O FEHLER
            obj = 1;                                    //            OBJ=1
            goto found;                                 //            T'O FOUND
fehler:     mtlist(number);                             // FEHLER     MTLIST.(NUMBER)
            bmark = bb + 1;                             //            BMARK=BB+1
            amark = ab;                                 //            AMARK=AB
            goto start;                                 //            T'O START
        }                                               //            E'L
    }                                                   //            E'L
endstr: for (machine_word i = ab - 1; i >= aa; --i) {   // ENDSTR     T'H MORE , FOR I=AB-1,-1, I .L. AA
        obj = a[i];                                     //            OBJ=A(I)
        list(a[i]);                                     //            LIST.(A(I))
        if (obj == 0) obj = bb - ba;                    //            W'R OBJ .E. 0, OBJ = BB-BA
        bb -= obj;                                      //            BB=BB - OBJ
        if (bb < ba) goto fail;                         //            W'R BB .L. BA, T'O FAIL
        for (machine_word j = 0; j != obj; ++j)         //            T'H CONC, FOR J=0,1, J .E. OBJ
            inlstl(b[bb + j], a[i]);                    // CONC       INLSTL.(B(BB+J),A(I))
    }                                                   // MORE       CONTINUE
    ii = ab - 1;                                        //            I=AB-1
forwrd: if (listmt(number) == 0) {                      // FORWRD     W'R LISTMT.(NUMBER) .E. 0
        iralst(number);                                 //            IRALST.(NUMBER)
        ba = bmark;                                     //            BA=BMARK
        ///
        std::cout << "xmatch SUCCEEDED\n";
        std::cout
            << "xmatch exit{\n  aa = " << aa
            << "\n  ab = " << ab
            << "\n  ac = " << ac
            << "\n  ba = " << ba
            << "}\n";
        ///
        return 0;                                       //            F'N 0
    } else {                                            //            O'E
        ++ii;                                           //            I=I+1
        it = poptop(number);                            //            IT=POPTOP.(NUMBER)
        obj = lnkl(it);                                 //            OBJ=LNKL.(IT)
        jj = lnkr(it);                                  //            J=LNKR.(IT)
        if (namtst(a[ii]) == 0) iralst(a[ii]);          //            W'R NAMTST.(A(I)) .E. 0, IRALST.(A(I))
        list(a[ii]);                                    //            LIST.(A(I))
        for (machine_word k = 0; k != obj; ++k)         //            T'H PLACE, FOR K=0,1, K .E. OBJ
            inlstl(b[jj + k], a[ii]);                   // PLACE      INLSTL.(B(J+K),A(I))
    }                                                   //            E'L
    goto forwrd;                                        //            T'O FORWRD
fail: iralst(number);                                   // FAIL       IRALST.(NUMBER)
    ///
    std::cout << "xmatch FAILED\n";
    ///
    return 1;                                           //            F'N 1
}                                                       //            E'N
#endif


/*
      YMATCH   MAD
           EXTERNAL FUNCTION (SLST,DLST,OUTLST)
           NORMAL MODE IS INTEGER
           DIMENSION A(100),B(100)
           ENTRY TO YMATCH.
           PARTN.(SLST,A,$ NONE$)
           PARTN.(DLST,B,$/$)
           BA=1
           LIMIT=A(0)
           MARKC=1
MORE       MARKA=MARKC
           MKA=MARKA
           W'R A(MKA) .NE. 0, T'O FINDB
           W'R MKA .E. LIMIT, T'O AMARK
           MKA=MKA+1
FINDB      T'H FINDB, FOR I = MKA,1 , I .E. LIMIT
          1.OR. LNKL.(A(I)) .NE. 0 .OR. A(I) .E. 0
           W'R A(I) .NE. 0, T'O BMARK
           MARKB=I-1
           MARKC=I
           T'O MATCH
AMARK      I=LIMIT
BMARK      MARKB=I
FINDC      T'H FINDC, FOR J=I+1,1, J.G. LIMIT
          1.OR. A(J) .E. 0
           MARKC=J
MATCH      W'R XMATCH.(A,B,MARKA,MARKB,MARKC,BA) .E. 0
           W'R MARKC .G. LIMIT, T'O SUCCES
           T'O MORE
           O'E
           SWITCH=1
           T'O FAIL
           E'L
SUCCES     SWITCH=2
FAIL       T'H MTB, FOR I=1,1, I .G. B(O)
MTB        IRALST.(B(I))
           T'H MTA, FOR I=1,1, I .G. LIMIT
           W'R LNKL.(A(I)) .NE. 0
           NEWBOT.(A(I),OUTLST)
           IRALST.(A(I))
           O'E
           E'L
MTA        CONTINUE
           T'O END(SWITCH)
END(1)     MTLIST.(OUTLST)
           F'N 0
END(2)     F'N OUTLST
           E'N
*/
machine_word ymatch(
    machine_word slst,      // rule (pattern to be matched)
    machine_word dlst,      // text
    machine_word & outlst)  // result
{
    // a        an array rule elements copied from slst
    // b        an array of words copied from dlst
    // limit    the index of the last rule element in rule array a
    // markc    the index into a where matching should continue
    //          on the next time round the more loop 

    ///
    std::cout
        << "\nymatch{\n  rule = " << not_slip_lprintstr(slst)
        << "\n  text = " << not_slip_lprintstr(dlst) << "}\n";
    ///

    machine_word ba, marka, markb, markc, mka, i, j, switch_flag;
                                                        //       YMATCH   MAD
                                                        //            EXTERNAL FUNCTION (SLST,DLST,OUTLST)
                                                        //            NORMAL MODE IS INTEGER
    std::vector<machine_word> a(100), b(100);           //            DIMENSION A(100),B(100)
                                                        //            ENTRY TO YMATCH.
    partn(slst, a, last_chunk_as_bcd(" NONE"));         //            PARTN.(SLST,A,$ NONE$)
    partn(dlst, b, last_chunk_as_bcd("/"));             //            PARTN.(DLST,B,$/$)
    ba = 1;                                             //            BA=1
    const machine_word limit = a[0];                    //            LIMIT=A(0)
    markc = 1;                                          //            MARKC=1
more: marka = markc;                                    // MORE       MARKA=MARKC
    mka = marka;                                        //            MKA=MARKA
    if (a[mka] != 0) goto findb;                        //            W'R A(MKA) .NE. 0, T'O FINDB
    if (mka == limit) goto amark;                       //            W'R MKA .E. LIMIT, T'O AMARK
    ++mka;                                              //            MKA=MKA+1
findb: for (i = mka; i != limit && lnkl(a[i]) == 0 && a[i] != 0; ++i)// FINDB      T'H FINDB, FOR I = MKA,1 , I .E. LIMIT
        ;                                               //           1.OR. LNKL.(A(I)) .NE. 0 .OR. A(I) .E. 0
    if (a[i] != 0) goto bmark;                          //            W'R A(I) .NE. 0, T'O BMARK
    markb = i - 1;                                      //            MARKB=I-1
    markc = i;                                          //            MARKC=I
    goto match;                                         //            T'O MATCH
amark: i = limit;                                       // AMARK      I=LIMIT
bmark: markb = i;                                       // BMARK      MARKB=I
    for(j = i + 1; j <= limit && a[j] != 0; ++j)        // FINDC      T'H FINDC, FOR J=I+1,1, J.G. LIMIT
        ;                                               //           1.OR. A(J) .E. 0
    markc = j;                                          //            MARKC=J
match: if (xmatch(a, b, marka, markb, markc, ba) == 0) {// MATCH      W'R XMATCH.(A,B,MARKA,MARKB,MARKC,BA) .E. 0
        if (markc > limit) goto succes;                 //            W'R MARKC .G. LIMIT, T'O SUCCES
        goto more;                                      //            T'O MORE
    } else {                                            //            O'E
        switch_flag = 1;                                //            SWITCH=1
        goto fail;                                      //            T'O FAIL
    }                                                   //            E'L
succes: switch_flag = 2;                                // SUCCES     SWITCH=2
fail: for (i = 1; i <= b[0]; ++i)                       // FAIL       T'H MTB, FOR I=1,1, I .G. B(O)
        iralst(b[i]);                                   // MTB        IRALST.(B(I))
    for (i = 1; i <= limit; ++i) {                      //            T'H MTA, FOR I=1,1, I .G. LIMIT
        if (lnkl(a[i]) != 0) {                          //            W'R LNKL.(A(I)) .NE. 0
            newbot(a[i], outlst);                       //            NEWBOT.(A(I),OUTLST)
            iralst(a[i]);                               //            IRALST.(A(I))
                                                        //            O'E
        }                                               //            E'L
    }                                                   // MTA        CONTINUE
    if (switch_flag == 1) {                             //            T'O END(SWITCH)
        mtlist(outlst);                                 // END(1)     MTLIST.(OUTLST)
        return 0;                                       //            F'N 0
    }
    return outlst;                                      // END(2)     F'N OUTLST
}                                                       //            E'N








DEF_TEST_FUNC(slip_test)
{
    auto length = [](machine_word lst) {
        TEST_EQUAL(namlst(lst), 0);
        unsigned count = 0;
        machine_word reader = seqrdr(lst);
        for (;;) {
            machine_word flag;
            seqlr(reader, flag);
            if (flag == 1)
                break;
            ++count;
        }
        return count;
    };

    auto namtst = [](machine_word w) {
        TEST_EQUAL(lnkl(w), lnkr(w));
        TEST_EQUAL(id(cont(w)), id_list_header);
        TEST_EQUAL(iqual(cont(lnkr(cont(lnkl(cont(w))))), cont(w)), 0);
    };

    {
        unsigned assumed_free_cells = initas();

        machine_word lst;
        list(lst);
        TEST_EQUAL(length(lst), 0);
        TEST_EQUAL(listmt(lst), 0);
        machine_word reader = seqrdr(lst);
        machine_word flag;
        TEST_EQUAL(seqlr(reader, flag), 0);
        TEST_EQUAL(flag, 1);
        TEST_EQUAL(seqll(reader, flag), 0);
        TEST_EQUAL(flag, 1);

        newtop(123, lst);
        TEST_EQUAL(length(lst), 1);
        TEST_EQUAL(listmt(lst), 1);
        TEST_EQUAL(top(lst), 123);
        TEST_EQUAL(bot(lst), 123);
        TEST_EQUAL(namlst(top(lst)), 1);
        reader = seqrdr(lst);
        TEST_EQUAL(seqlr(reader, flag), 123);
        TEST_EQUAL(negative(flag), true);
        TEST_EQUAL(seqlr(reader, flag), 0);
        TEST_EQUAL(flag, 1);
        TEST_EQUAL(seqll(reader, flag), 123);
        TEST_EQUAL(negative(flag), true);
        TEST_EQUAL(seqll(reader, flag), 0);
        TEST_EQUAL(flag, 1);

        newtop(456, lst);
        TEST_EQUAL(length(lst), 2);
        TEST_EQUAL(listmt(lst), 1);
        TEST_EQUAL(top(lst), 456);
        TEST_EQUAL(bot(lst), 123);
        reader = seqrdr(lst);
        TEST_EQUAL(seqlr(reader, flag), 456);
        TEST_EQUAL(negative(flag), true);
        TEST_EQUAL(seqlr(reader, flag), 123);
        TEST_EQUAL(negative(flag), true);
        TEST_EQUAL(seqlr(reader, flag), 0);
        TEST_EQUAL(flag, 1);
        TEST_EQUAL(seqll(reader, flag), 123);
        TEST_EQUAL(negative(flag), true);
        TEST_EQUAL(seqll(reader, flag), 456);
        TEST_EQUAL(negative(flag), true);
        TEST_EQUAL(seqll(reader, flag), 0);
        TEST_EQUAL(flag, 1);

        newbot(789, lst);
        TEST_EQUAL(length(lst), 3);
        TEST_EQUAL(listmt(lst), 1);
        TEST_EQUAL(top(lst), 456);
        TEST_EQUAL(bot(lst), 789);
        reader = seqrdr(lst);
        TEST_EQUAL(seqlr(reader, flag), 456);
        TEST_EQUAL(negative(flag), true);
        TEST_EQUAL(seqlr(reader, flag), 123);
        TEST_EQUAL(negative(flag), true);
        TEST_EQUAL(seqlr(reader, flag), 789);
        TEST_EQUAL(negative(flag), true);
        TEST_EQUAL(seqlr(reader, flag), 0);
        TEST_EQUAL(flag, 1);
        TEST_EQUAL(seqll(reader, flag), 789);
        TEST_EQUAL(negative(flag), true);
        TEST_EQUAL(seqll(reader, flag), 123);
        TEST_EQUAL(negative(flag), true);
        TEST_EQUAL(seqll(reader, flag), 456);
        TEST_EQUAL(negative(flag), true);
        TEST_EQUAL(seqll(reader, flag), 0);
        TEST_EQUAL(flag, 1);

        TEST_EQUAL(popbot(lst), 789);
        TEST_EQUAL(length(lst), 2);
        TEST_EQUAL(listmt(lst), 1);
        TEST_EQUAL(top(lst), 456);
        TEST_EQUAL(bot(lst), 123);
        reader = seqrdr(lst);
        TEST_EQUAL(seqlr(reader, flag), 456);
        TEST_EQUAL(negative(flag), true);
        TEST_EQUAL(seqlr(reader, flag), 123);
        TEST_EQUAL(negative(flag), true);
        TEST_EQUAL(seqlr(reader, flag), 0);
        TEST_EQUAL(flag, 1);
        TEST_EQUAL(seqll(reader, flag), 123);
        TEST_EQUAL(negative(flag), true);
        TEST_EQUAL(seqll(reader, flag), 456);
        TEST_EQUAL(negative(flag), true);
        TEST_EQUAL(seqll(reader, flag), 0);
        TEST_EQUAL(flag, 1);

        TEST_EQUAL(poptop(lst), 456);
        TEST_EQUAL(length(lst), 1);
        TEST_EQUAL(listmt(lst), 1);
        TEST_EQUAL(top(lst), 123);
        TEST_EQUAL(bot(lst), 123);
        reader = seqrdr(lst);
        TEST_EQUAL(seqlr(reader, flag), 123);
        TEST_EQUAL(negative(flag), true);
        TEST_EQUAL(seqlr(reader, flag), 0);
        TEST_EQUAL(flag, 1);
        TEST_EQUAL(seqll(reader, flag), 123);
        TEST_EQUAL(negative(flag), true);
        TEST_EQUAL(seqll(reader, flag), 0);
        TEST_EQUAL(flag, 1);

        TEST_EQUAL(popbot(lst), 123);
        TEST_EQUAL(length(lst), 0);
        TEST_EQUAL(listmt(lst), 0);
        reader = seqrdr(lst);
        TEST_EQUAL(seqlr(reader, flag), 0);
        TEST_EQUAL(flag, 1);
        TEST_EQUAL(seqll(reader, flag), 0);
        TEST_EQUAL(flag, 1);

        TEST_EQUAL(mtlist(lst), lst);
        TEST_EQUAL(listmt(lst), 0);
        newtop(123, lst);
        newtop(123, lst);
        newtop(123, lst);
        newtop(123, lst);
        TEST_EQUAL(listmt(lst), 1);
        TEST_EQUAL(mtlist(lst), lst);
        TEST_EQUAL(listmt(lst), 0);

        newtop(123, lst);
        newtop(123, lst);
        newtop(123, lst);
        newtop(123, lst);
        TEST_EQUAL(length(lst), 4);
        //TEST_EQUAL(iralst(lst), 0);
        iralst(lst);

        //TEST_EQUAL(iralst(list(lst)), 0);


        // machine_word sublst;
        // newtop(list(sublst), lst);
        // assumed_free_cells -= 2;
        // TEST_EQUAL(length(lst), 4);
        // TEST_EQUAL(length(lavs), assumed_free_cells);
        // TEST_EQUAL(namlst(top(lst)), 0);
        // TEST_EQUAL(length(top(lst)), 0);
        // TEST_EQUAL(bot(lst), 789);

    }
}


/*
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
*/

template <typename output_device>
machine_word lprint(machine_word lst, output_device & tape)
{
    static constexpr unsigned char bcd[64] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 0, '=', '\'', 0, 0, 0,
        '+', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 0, '.', ')',  0, 0, 0,
        '-', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 0, '$', '*',  0, 0, 0,
        ' ', '/', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 0, ',', '(',  0, 0, 0
    };

    auto place = [&](machine_word w){
        for (int i = 5; i > -1; --i)
            tape << bcd[(w >> 6 * i) & 077];
    };

    // Note: this implementation does not apear to print description lists.

                                                //         LPRINT  MAD
                                                //             EXTERNAL FUNCTION (LST,TAPE)
                                                //             NORMAL MODE IS INTEGER
                                                //             ENTRY TO LPRINT.
    const machine_word blank = 0606060606060;   //             BLANK = $      $
    const machine_word leftp = 0606074606060;   //             LEFTP = 606074606060K
    const machine_word rightp= 0606034606060;   //             RIGHTP= 606034606060K
    const machine_word both  = 0607460603460;   //             BOTH  = 607460603460K
    machine_word stack;
    newtop(seqrdr(lst), list(stack));           //             EXECUTE NEWTOP.(SEQRDR.(LST),LIST.(STACK))
    machine_word s = poptop(stack);             //             S=POPTOP.(STACK)
begin: place(leftp);                            // BEGIN       EXECUTE PLACE.(LEFTP,1)
    machine_word flag;
next: machine_word word = seqlr(s, flag);       // NEXT        WORD=SEQLR.(S,FLAG)
    if (negative(flag)) {                       //             W'R FLAG .L. 0
        place(word);                            //             EXECUTE PLACE.(WORD,1)
        if (positive(s)) place(blank);          //             W'R S .G. 0, PLACE.(BLANK,1)
        goto next;                              //             T'O NEXT
    } else if (flag == 1) {                     //             OR W'R FLAG .G. 0
        place(rightp);                          //             EXECUTE PLACE.(RIGHTP,1)
        if (listmt(stack) == 0) goto done;      //             W'R LISTMT.(STACK) .E. 0, T'O DONE
        s = poptop(stack);                      //             S=POPTOP.(STACK)
        goto next;                              //             T'O NEXT
    } else {                                    //             OTHERWISE
        if (listmt(word) == 0) {                //             W'R LISTMT.(WORD) .E. 0
            place(both);                        //             EXECUTE PLACE.(BOTH,1)
            goto next;                          //             T'O NEXT
        } else {                                //             OTHERWISE
            newtop(s, stack);                   //             EXECUTE NEWTOP.(S,STACK)
            s = seqrdr(word);                   //             S=SEQRDR.(WORD)
            goto begin;                         //             T'O BEGIN
        }                                       //             E'L
    }                                           //             E'L
done:                                           // DONE        EXECUTE PLACE.(0,-1)
    iralst(stack);                              //             EXECUTE IRALST.(STACK)
    return lst;                                 //             FUNCTION RETURN LST
}                                               //             END OF FUNCTION

DEF_TEST_FUNC(slip_lprint_test)
{
    auto lprintstr = [](machine_word lst) {
        std::stringstream s;
        lprint(lst, s);
        return s.str(); 
    };

    machine_word lst;
    list(lst);
    TEST_EQUAL(lprintstr(lst), "  (     )   ");
    newtop(0212223606060ULL, lst);
    TEST_EQUAL(lprintstr(lst), "  (   ABC           )   ");
    newtop(0010203040506ULL, lst);
    TEST_EQUAL(lprintstr(lst), "  (   123456      ABC           )   ");
    newbot(0010203040506ULL, lst);
    TEST_EQUAL(lprintstr(lst), "  (   123456      ABC         123456        )   ");
    lnkbot(0071011606060ULL, lst);
    TEST_EQUAL(lprintstr(lst), "  (   123456      ABC         123456789           )   ");

    machine_word lst2;
    list(lst2);
    newbot(lst2, lst);
    TEST_EQUAL(lprintstr(lst), "  (   123456      ABC         123456789          (  )   )   ");
    newtop(0302543434660ULL, lst2);
    TEST_EQUAL(lprintstr(lst), "  (   123456      ABC         123456789           (   HELLO         )     )   ");

    poptop(lst);
    TEST_EQUAL(lprintstr(lst), "  (   ABC         123456789           (   HELLO         )     )   ");
    popbot(lst);
    TEST_EQUAL(lprintstr(lst), "  (   ABC         123456789           )   ");
    popbot(lst);
    TEST_EQUAL(lprintstr(lst), "  (   ABC         123456  )   ");
    popbot(lst);
    TEST_EQUAL(lprintstr(lst), "  (   ABC           )   ");
    popbot(lst);
    TEST_EQUAL(lprintstr(lst), "  (     )   ");

    // Why doesn't LPRINT print the discription list?
    makedl(lst2, lst);
    TEST_EQUAL(lprintstr(lst), "  (     )   ");
}


DEF_TEST_FUNC(test_basic_slip_list_functions)
{
    const unsigned free_cells = initas();

    auto lprintstr = [](machine_word lst) {
        std::stringstream s;
        not_slip_lprint(lst, s);
        return s.str(); 
    };


    machine_word lst;
    list(lst);
    //TEST_EQUAL(number_of_free_cells(), free_cells - 1);
    TEST_EQUAL(lprintstr(lst), "( ) ");
    iralst(lst);
    //TEST_EQUAL(number_of_free_cells(), free_cells);

    list(lst);
    newbot(last_chunk_as_bcd("HELLO"), lst);
    //TEST_EQUAL(number_of_free_cells(), free_cells - 2);
    TEST_EQUAL(lprintstr(lst), "( HELLO ) ");
    iralst(lst);
    //TEST_EQUAL(number_of_free_cells(), free_cells);

    list(lst);
    newbot(last_chunk_as_bcd("OVERFL"), lst);
    lnkbot(last_chunk_as_bcd("OWING"), lst);
    //TEST_EQUAL(number_of_free_cells(), free_cells - 3);
    TEST_EQUAL(lprintstr(lst), "( OVERFLOWING ) ");
    iralst(lst);
    //TEST_EQUAL(number_of_free_cells(), free_cells);

    machine_word sublst;
    list(sublst);
    list(lst);
    newbot(sublst, lst);
    //TEST_EQUAL(number_of_free_cells(), free_cells - 3);
    TEST_EQUAL(lprintstr(lst), "( ( ) ) ");
    iralst(lst);
    //TEST_EQUAL(number_of_free_cells(), free_cells - 1);
    iralst(sublst);
    //TEST_EQUAL(number_of_free_cells(), free_cells);
    //TBD

}







DEF_TEST_FUNC(not_slip_lprint_test)
{
    //const unsigned free_cells = number_of_free_cells();

    auto lprintstr = [](machine_word lst) {
        std::stringstream s;
        not_slip_lprint(lst, s);
        return s.str(); 
    };

    // ((BOYS CATS DLIST (PTSPCH NOUN)) LIKE GIRLS)

    machine_word dlst;
    list(dlst);
    newbot(last_chunk_as_bcd("OVERFL"), dlst);
    lnkbot(last_chunk_as_bcd("OWING"), dlst);
    newbot(last_chunk_as_bcd("NOUN"), dlst);

    machine_word boys;
    list(boys);
    newbot(last_chunk_as_bcd("BOYS"), boys);
    newbot(last_chunk_as_bcd("CATS"), boys);
    makedl(dlst, boys);

    machine_word lst;
    list(lst);
    TEST_EQUAL(lprintstr(lst), "( ) ");
    newbot(boys, lst);
    newbot(last_chunk_as_bcd("LIKE"), lst);
    newbot(last_chunk_as_bcd("GIRLS"), lst);
    //std::cout << lprintstr(lst) << '\n';

    iralst(lst);
    //std::cout << lprintstr(boys) << '\n';
    //std::cout << lprintstr(dlst) << '\n';
    iralst(boys);
    iralst(dlst);

    //TEST_EQUAL(number_of_free_cells(), free_cells);
    //TBD
}



DEF_TEST_FUNC(test_slip_stuff)
{
    initas();

    auto lprintstr = [](machine_word lst) {
        std::stringstream s;
        not_slip_lprint(lst, s);
        return s.str(); 
    };


    machine_word a, b;
    newtop(last_chunk_as_bcd("5"), list(a));
    newbot(last_chunk_as_bcd("13"), a);
    newtop(list(b), a);
    newtop(last_chunk_as_bcd("10"), b);
    TEST_EQUAL(lprintstr(a), "( ( 10 ) 5 13 ) ");
    iralst(a);

    machine_word car, dlist, d, e;
    newbot(last_chunk_as_bcd("EONE"), list(e));
    newbot(last_chunk_as_bcd("ETWO"), e);
    newbot(last_chunk_as_bcd("DONE"), list(d));
    newbot(last_chunk_as_bcd("DTWO"), d);
    newbot(e, d);
    newbot(last_chunk_as_bcd("COLOR"), list(dlist));
    newbot(last_chunk_as_bcd("RED"), dlist);
    newbot(last_chunk_as_bcd("SIZE"), dlist);
    newbot(last_chunk_as_bcd("GIGANT"), dlist);
    lnkbot(last_chunk_as_bcd("IC"), dlist);
    newbot(last_chunk_as_bcd("A"), list(car));
    newbot(last_chunk_as_bcd("B"), car);
    makedl(dlist, car);
    newbot(last_chunk_as_bcd("C"), car);
    newbot(d, car);
    TEST_EQUAL(lprintstr(car), "( DLIST ( COLOR RED SIZE GIGANTIC ) A B C ( DONE DTWO ( EONE ETWO ) ) ) ");
    iralst(car);

}


DEF_TEST_FUNC(test_slip_madatr_itsval_newval)
{
    initas();

    auto lprintstr = [](machine_word lst) {
        std::stringstream s;
        not_slip_lprint(lst, s);
        return s.str(); 
    };

    machine_word car, dlist, d, e;
    newbot(last_chunk_as_bcd("EONE"), list(e));
    newbot(last_chunk_as_bcd("ETWO"), e);
    newbot(last_chunk_as_bcd("DONE"), list(d));
    newbot(last_chunk_as_bcd("DTWO"), d);
    newbot(e, d);
    newbot(last_chunk_as_bcd("COLOR"), list(dlist));
    newbot(last_chunk_as_bcd("RED"), dlist);
    newbot(last_chunk_as_bcd("SIZE"), dlist);
    newbot(last_chunk_as_bcd("GIGANT"), dlist);
    lnkbot(last_chunk_as_bcd("IC"), dlist);
    newbot(last_chunk_as_bcd("A"), list(car));
    newbot(last_chunk_as_bcd("B"), car);
    makedl(dlist, car);
    newbot(last_chunk_as_bcd("C"), car);
    newbot(d, car);
    TEST_EQUAL(lprintstr(car), "( DLIST ( COLOR RED SIZE GIGANTIC ) A B C ( DONE DTWO ( EONE ETWO ) ) ) ");

    machine_word addr = madatr(last_chunk_as_bcd("COLOR"), car);
    TEST_EQUAL(negative(addr), false);
    TEST_EQUAL(cont(lnkr(cont(addr)) + 1), last_chunk_as_bcd("RED"));
    addr = madatr(last_chunk_as_bcd("SIZE"), car);
    TEST_EQUAL(negative(addr), false);
    TEST_EQUAL(cont(lnkr(cont(addr)) + 1), last_chunk_as_bcd("GIGANT"));
    TEST_EQUAL(negative(cont(lnkr(cont(addr)))), true);
    TEST_EQUAL(cont(lnkr(cont(lnkr(cont(addr)))) + 1), last_chunk_as_bcd("IC"));
    TEST_EQUAL(negative(cont(lnkr(cont(lnkr(cont(addr)))))), false);
    addr = madatr(last_chunk_as_bcd("LOST"), car);
    TEST_EQUAL(negative(addr), true);

    machine_word value = itsval(last_chunk_as_bcd("COLOR"), car);
    TEST_EQUAL(value, last_chunk_as_bcd("RED"));
    value = itsval(last_chunk_as_bcd("SIZE"), car);
    // SLIP bug or programmer caveat? ("GIGANTIC" is the value associated with "SIZE")
    TEST_EQUAL(value, last_chunk_as_bcd("GIGANT"));
    value = itsval(last_chunk_as_bcd("LOST"), car);
    TEST_EQUAL(value, 0);

    newval(last_chunk_as_bcd("COLOR"), last_chunk_as_bcd("GREEN"), car);
    TEST_EQUAL(lprintstr(car), "( DLIST ( COLOR GREEN SIZE GIGANTIC ) A B C ( DONE DTWO ( EONE ETWO ) ) ) ");
    newval(last_chunk_as_bcd("LOST"), last_chunk_as_bcd("FOUND"), car);
    TEST_EQUAL(lprintstr(car), "( DLIST ( COLOR GREEN SIZE GIGANTIC LOST FOUND ) A B C ( DONE DTWO ( EONE ETWO ) ) ) ");

    machine_word alist;
    list(alist);
    TEST_EQUAL(lprintstr(alist), "( ) ");
    newval(last_chunk_as_bcd("NAME"), last_chunk_as_bcd("VALUE"), alist);
    TEST_EQUAL(lprintstr(alist), "( DLIST ( NAME VALUE ) ) ");
    newval(last_chunk_as_bcd("NAME"), last_chunk_as_bcd("NEWAGE"), alist);
    TEST_EQUAL(lprintstr(alist), "( DLIST ( NAME NEWAGE ) ) ");
    newval(last_chunk_as_bcd("AGE"), last_chunk_as_bcd("OLD"), alist);
    TEST_EQUAL(lprintstr(alist), "( DLIST ( NAME NEWAGE AGE OLD ) ) ");

    // SLIP bug or programmer caveat? (after newval() the value associated with "SIZE" will be "SMALLIC")
    newval(last_chunk_as_bcd("SIZE"), last_chunk_as_bcd("SMALL"), car);
    TEST_EQUAL(itsval(last_chunk_as_bcd("SIZE"), car), last_chunk_as_bcd("SMALL")); // looks good, but...
    TEST_EQUAL(lprintstr(car), "( DLIST ( COLOR GREEN SIZE SMALLIC LOST FOUND ) A B C ( DONE DTWO ( EONE ETWO ) ) ) ");
}


DEF_TEST_FUNC(test_slip_lstcpy)
{
    initas();

    auto lprintstr = [](machine_word lst) {
        std::stringstream s;
        not_slip_lprint(lst, s);
        return s.str(); 
    };

    machine_word lst, cpy;
    list(lst);
    TEST_EQUAL(lprintstr(lst), "( ) ");
    lsscpy(lst, list(cpy));
    TEST_EQUAL(lprintstr(cpy), "( ) ");

    newbot(last_chunk_as_bcd("HELLO"), lst);
    TEST_EQUAL(lprintstr(lst), "( HELLO ) ");
    lsscpy(lst, list(cpy));
    TEST_EQUAL(lprintstr(cpy), "( HELLO ) ");
    iralst(cpy);

    newbot(last_chunk_as_bcd("OVERFL"), lst);
    lnkbot(last_chunk_as_bcd("OWING"), lst);
    TEST_EQUAL(lprintstr(lst), "( HELLO OVERFLOWING ) ");
    lsscpy(lst, list(cpy));
    TEST_EQUAL(lprintstr(cpy), "( HELLO OVERFLOWING ) ");
    iralst(cpy);

    machine_word sublst;
    list(sublst);
    newbot(sublst, lst);
    TEST_EQUAL(lprintstr(lst), "( HELLO OVERFLOWING ( ) ) ");
    lsscpy(lst, list(cpy));
    TEST_EQUAL(lprintstr(cpy), "( HELLO OVERFLOWING ( ) ) ");
    iralst(cpy);

    machine_word car, dlist, d, e;
    newbot(last_chunk_as_bcd("EONE"), list(e));
    newbot(last_chunk_as_bcd("ETWO"), e);
    newbot(last_chunk_as_bcd("DONE"), list(d));
    newbot(last_chunk_as_bcd("DTWO"), d);
    newbot(e, d);
    newbot(last_chunk_as_bcd("COLOR"), list(dlist));
    newbot(last_chunk_as_bcd("RED"), dlist);
    newbot(last_chunk_as_bcd("SIZE"), dlist);
    newbot(last_chunk_as_bcd("GIGANT"), dlist);
    lnkbot(last_chunk_as_bcd("IC"), dlist);
    newbot(last_chunk_as_bcd("A"), list(car));
    newbot(last_chunk_as_bcd("B"), car);
    makedl(dlist, car);
    newbot(last_chunk_as_bcd("C"), car);
    newbot(d, car);
    TEST_EQUAL(lprintstr(car), "( DLIST ( COLOR RED SIZE GIGANTIC ) A B C ( DONE DTWO ( EONE ETWO ) ) ) ");
    lsscpy(car, list(cpy));
    TEST_EQUAL(lprintstr(cpy), "( DLIST ( COLOR RED SIZE GIGANTIC ) A B C ( DONE DTWO ( EONE ETWO ) ) ) ");
    newval(last_chunk_as_bcd("COLOR"), last_chunk_as_bcd("GREEN"), car);
    TEST_EQUAL(lprintstr(car), "( DLIST ( COLOR GREEN SIZE GIGANTIC ) A B C ( DONE DTWO ( EONE ETWO ) ) ) ");
    TEST_EQUAL(lprintstr(cpy), "( DLIST ( COLOR RED SIZE GIGANTIC ) A B C ( DONE DTWO ( EONE ETWO ) ) ) ");
    iralst(cpy);

}


DEF_TEST_FUNC(test_slip_lsteql)
{
    initas();

    auto lprintstr = [](machine_word lst) {
        std::stringstream s;
        not_slip_lprint(lst, s);
        return s.str(); 
    };

    machine_word l1, l2, dl, sub1, sub2;
    list(l1);
    list(l2);
    TEST_EQUAL(lsteql(l1, l2), 0);
    newtop(last_chunk_as_bcd("JOSEPH"), l1);
    TEST_EQUAL(negative(lsteql(l1, l2)), true);
    newtop(last_chunk_as_bcd("JOSEPH"), l2);
    TEST_EQUAL(lsteql(l1, l2), 0);
    list(dl);
    newbot(last_chunk_as_bcd("COLOUR"), list(dl));
    newbot(last_chunk_as_bcd("GREEN"), dl);
    makedl(dl, l1);
    // Note: lsteql() returns that two lists are equal even if they have
    // different description lists, or one has a dl and the other doesn't.
    TEST_EQUAL(lsteql(l1, l2), 0);
    list(sub2);
    newbot(sub2, l2);
    TEST_EQUAL(negative(lsteql(l1, l2)), true);
    list(sub1);
    newbot(sub1, l1);
    TEST_EQUAL(lsteql(l1, l2), 0);
    newbot(last_chunk_as_bcd("WEIZEN"), sub1);
    lnkbot(last_chunk_as_bcd("BAUM"), sub1);
    TEST_EQUAL(negative(lsteql(l1, l2)), true);
    newbot(last_chunk_as_bcd("WEIZEN"), sub2);
    lnkbot(last_chunk_as_bcd("BAUM"), sub2);
    TEST_EQUAL(lsteql(l1, l2), 0);
    TEST_EQUAL(lprintstr(l1), "( DLIST ( COLOUR GREEN ) JOSEPH ( WEIZENBAUM ) ) ");
    TEST_EQUAL(lprintstr(l2), "( JOSEPH ( WEIZENBAUM ) ) ");

    machine_word car, dlist, d, e;
    newbot(last_chunk_as_bcd("EONE"), list(e));
    newbot(last_chunk_as_bcd("ETWO"), e);
    newbot(last_chunk_as_bcd("DONE"), list(d));
    newbot(last_chunk_as_bcd("DTWO"), d);
    newbot(e, d);
    newbot(last_chunk_as_bcd("COLOR"), list(dlist));
    newbot(last_chunk_as_bcd("RED"), dlist);
    newbot(last_chunk_as_bcd("SIZE"), dlist);
    newbot(last_chunk_as_bcd("GIGANT"), dlist);
    lnkbot(last_chunk_as_bcd("IC"), dlist);
    newbot(last_chunk_as_bcd("A"), list(car));
    newbot(last_chunk_as_bcd("B"), car);
    makedl(dlist, car);
    newbot(last_chunk_as_bcd("C"), car);
    newbot(d, car);
    TEST_EQUAL(lprintstr(car), "( DLIST ( COLOR RED SIZE GIGANTIC ) A B C ( DONE DTWO ( EONE ETWO ) ) ) ");
    machine_word cpy;
    lsscpy(car, list(cpy));
    TEST_EQUAL(lprintstr(cpy), "( DLIST ( COLOR RED SIZE GIGANTIC ) A B C ( DONE DTWO ( EONE ETWO ) ) ) ");
    TEST_EQUAL(lsteql(car, cpy), 0);
}


DEF_TEST_FUNC(test_slip_inlstl)
{
    initas();

    auto lprintstr = [](machine_word lst) {
        std::stringstream s;
        not_slip_lprint(lst, s);
        return s.str(); 
    };

/*
    "INLSTL(M, A)
    M must be the ALIAS of a list and A the machine address of a list cell.
    INLSTL takes the set of linked cells constituting the body of the list M,
    i.e., all but the HEADER of that list, and inserts it to the left of the
    cell A. It thus lengthens the list of which A is a member by whatever the
    length of M was. M is made into an empty list and its NAME delivered as
    the value of this function." -- From Weizenbaum's 1963 CACM paper on SLIP
*/

    machine_word l1, l2;
    list(l1);
    newbot(last_chunk_as_bcd("ONE"), l1);
    newbot(last_chunk_as_bcd("TWO"), l1);
    newbot(last_chunk_as_bcd("THREE"), l1);
    TEST_EQUAL(lprintstr(l1), "( ONE TWO THREE ) ");
    list(l2);
    newbot(last_chunk_as_bcd("FOUR"), l2);
    newbot(last_chunk_as_bcd("FIVE"), l2);
    newbot(last_chunk_as_bcd("SIX"), l2);
    TEST_EQUAL(lprintstr(l2), "( FOUR FIVE SIX ) ");
    machine_word result = inlstl(l1, lnkr(cont(l2)));
    TEST_EQUAL(lprintstr(l1), "( ) ");
    TEST_EQUAL(lprintstr(l2), "( ONE TWO THREE FOUR FIVE SIX ) ");
    TEST_EQUAL(result, l1);

    newbot(last_chunk_as_bcd("A"), l1);
    newbot(last_chunk_as_bcd("B"), l1);
    newbot(last_chunk_as_bcd("C"), l1);
    inlstl(l1, lnkl(cont(l2)));
    TEST_EQUAL(lprintstr(l1), "( ) ");
    TEST_EQUAL(lprintstr(l2), "( ONE TWO THREE FOUR FIVE A B C SIX ) ");

    machine_word l3;
    list(l3);
    newbot(last_chunk_as_bcd("TEST"), l3);
    newbot(l3, l1);
    inlstl(l2, lnkr(cont(l3)));
    TEST_EQUAL(lprintstr(l2), "( ) ");
    TEST_EQUAL(lprintstr(l1), "( ( ONE TWO THREE FOUR FIVE A B C SIX TEST ) ) ");
}


DEF_TEST_FUNC(test_slip_partn)
{
    initas();

    auto lprintstr = [](machine_word lst) {
        std::stringstream s;
        not_slip_lprint(lst, s);
        return s.str(); 
    };

    machine_word rule;
    list(rule);
    newbot(0, rule);

    machine_word text;
    newbot(99, list(text));
    newbot(42, text);

    std::vector<machine_word> a(100);
    machine_word count = partn(rule, a, last_chunk_as_bcd(" NONE"));

    std::vector<machine_word> b(100);
    count = partn(text, b, last_chunk_as_bcd("/"));

    // I dunno
}


DEF_TEST_FUNC(test_slip_xlook)
{
    initas();

    auto lprintstr = [](machine_word lst) {
        std::stringstream s;
        not_slip_lprint(lst, s);
        return s.str(); 
    };

    machine_word car, dlist, d, e;
    newbot(last_chunk_as_bcd("EONE"), list(e));
    newbot(last_chunk_as_bcd("ETWO"), e);
    newbot(last_chunk_as_bcd("DONE"), list(d));
    newbot(last_chunk_as_bcd("DTWO"), d);
    newbot(e, d);
    newbot(last_chunk_as_bcd("COLOR"), list(dlist));
    newbot(last_chunk_as_bcd("RED"), dlist);
    newbot(last_chunk_as_bcd("SIZE"), dlist);
    newbot(last_chunk_as_bcd("GIGANT"), dlist);
    lnkbot(last_chunk_as_bcd("IC"), dlist);
    newbot(last_chunk_as_bcd("A"), list(car));
    newbot(last_chunk_as_bcd("B"), car);
    makedl(dlist, car);
    newbot(last_chunk_as_bcd("C"), car);
    newbot(d, car);
    TEST_EQUAL(lprintstr(car), "( DLIST ( COLOR RED SIZE GIGANTIC ) A B C ( DONE DTWO ( EONE ETWO ) ) ) ");

    TEST_EQUAL(xlook(last_chunk_as_bcd("COLOR"), car), 0)
    TEST_EQUAL(xlook(last_chunk_as_bcd("RED"), car), 0)
    TEST_EQUAL(xlook(last_chunk_as_bcd("SIZE"), car), 0)
    TEST_EQUAL(xlook(last_chunk_as_bcd("GIGANT"), car), 0)
    TEST_EQUAL(xlook(last_chunk_as_bcd("DONE"), car), 1)
    TEST_EQUAL(xlook(last_chunk_as_bcd("EONE"), car), 1)

    /*  Note: The following is why in the conversations in the Quarton study
        from 1965 the user input "WONDER" incorrectly matches (*HAPPY ELATED
        EXCITED GOOD WONDERFUL) (because WONDERFUL is stored as "WONDER",
        "FUL   ") and user input "S" incorrectly matches (*VICIOUS HOSTILE
        MEAN ANGRY ENVIOUS FURIOUS BITTER) (because VICIOUS is stored as
        "VICIOU", "S     "). */
    TEST_EQUAL(xlook(last_chunk_as_bcd("IC"), car), 0)
    machine_word wonder;
    newbot(last_chunk_as_bcd("HAPPY"), list(wonder));
    newbot(last_chunk_as_bcd("ELATED"), wonder);
    newbot(last_chunk_as_bcd("EXCITE"), wonder);
    lnkbot(last_chunk_as_bcd("D"), wonder);
    newbot(last_chunk_as_bcd("GOOD"), wonder);
    newbot(last_chunk_as_bcd("WONDER"), wonder);
    lnkbot(last_chunk_as_bcd("FUL"), wonder);
    machine_word wonder_test;
    makedl(wonder, list(wonder_test));
    TEST_EQUAL(lprintstr(wonder_test), "( DLIST ( HAPPY ELATED EXCITED GOOD WONDERFUL ) ) ");
    TEST_EQUAL(xlook(last_chunk_as_bcd("HAPPY"), wonder_test), 0)
    TEST_EQUAL(xlook(last_chunk_as_bcd("ELATED"), wonder_test), 0)
    TEST_EQUAL(xlook(last_chunk_as_bcd("EXCITE"), wonder_test), 0)
    TEST_EQUAL(xlook(last_chunk_as_bcd("D"), wonder_test), 0)
    TEST_EQUAL(xlook(last_chunk_as_bcd("GOOD"), wonder_test), 0)
    TEST_EQUAL(xlook(last_chunk_as_bcd("WONDER"), wonder_test), 0)
    TEST_EQUAL(xlook(last_chunk_as_bcd("FUL"), wonder_test), 0)
    TEST_EQUAL(xlook(last_chunk_as_bcd("SAD"), wonder_test), 1)
}


DEF_TEST_FUNC(test_slip_goody)
{
    initas();

    auto lprintstr = [](machine_word lst) {
        std::stringstream s;
        not_slip_lprint(lst, s);
        return s.str(); 
    };

    machine_word wonder;
    newbot(last_chunk_as_bcd("*"),      list(wonder));
    newbot(last_chunk_as_bcd("HAPPY"),  wonder);
    newbot(last_chunk_as_bcd("ELATED"), wonder);
    newbot(last_chunk_as_bcd("EXCITE"), wonder);
    lnkbot(last_chunk_as_bcd("D"),      wonder);
    newbot(last_chunk_as_bcd("GOOD"),   wonder);
    newbot(last_chunk_as_bcd("WONDER"), wonder);
    lnkbot(last_chunk_as_bcd("FUL"),    wonder);
    TEST_EQUAL(lprintstr(wonder), "( * HAPPY ELATED EXCITED GOOD WONDERFUL ) ");

    machine_word word;
    list(word);
    newbot(last_chunk_as_bcd("HAPPY"), word);
    TEST_EQUAL(goody(wonder, word), 0);
    mtlist(word);
    newbot(last_chunk_as_bcd("ELATED"), word);
    TEST_EQUAL(goody(wonder, word), 0);
    mtlist(word);
    newbot(last_chunk_as_bcd("EXCITE"), word);
    TEST_EQUAL(goody(wonder, word), 1);
    lnkbot(last_chunk_as_bcd("D"), word);
    TEST_EQUAL(goody(wonder, word), 0);
    poptop(word);
    TEST_EQUAL(goody(wonder, word), 1);
    mtlist(word);
    newbot(last_chunk_as_bcd("GOOD"), word);
    TEST_EQUAL(goody(wonder, word), 0);
    mtlist(word);
    newbot(last_chunk_as_bcd("WONDER"), word);
    TEST_EQUAL(goody(wonder, word), 1);
    lnkbot(last_chunk_as_bcd("FUL"), word);
    TEST_EQUAL(goody(wonder, word), 0);
    poptop(word);
    TEST_EQUAL(goody(wonder, word), 1);

    machine_word dlist;
    mtlist(word);
    newbot(last_chunk_as_bcd("MOTHER"), word);

    machine_word pattern;
    list(pattern);
    newbot(last_chunk_as_bcd("/"), pattern);
    newbot(last_chunk_as_bcd("FRIEND"), pattern);
    TEST_EQUAL(goody(pattern, word), 1);
    newbot(last_chunk_as_bcd("FAMILY"), pattern);
    TEST_EQUAL(goody(pattern, word), 1);

    list(dlist);
    newbot(last_chunk_as_bcd("/"), dlist);
    newbot(last_chunk_as_bcd("NOUN"), dlist);
    newbot(last_chunk_as_bcd("FAMILY"), dlist);
    makedl(dlist, word);
    TEST_EQUAL(lprintstr(word), "( DLIST ( / NOUN FAMILY ) MOTHER ) ");

    mtlist(pattern);
    newbot(last_chunk_as_bcd("/"), pattern);
    newbot(last_chunk_as_bcd("FRIEND"), pattern);
    TEST_EQUAL(goody(pattern, word), 1);
    newbot(last_chunk_as_bcd("FAMILY"), pattern);
    TEST_EQUAL(goody(pattern, word), 0);
    popbot(pattern);
    TEST_EQUAL(goody(pattern, word), 1);
    newbot(last_chunk_as_bcd("NOUN"), pattern);
    TEST_EQUAL(goody(pattern, word), 0);

    /*  The following demonstrates that if description list terms are
        longer than six characters there is the potential for them
        to be incorrectly matched. */
    newbot(last_chunk_as_bcd("HIGHLY"), dlist);
    lnkbot(last_chunk_as_bcd("STRUNG"), dlist);
    TEST_EQUAL(lprintstr(word), "( DLIST ( / NOUN FAMILY HIGHLYSTRUNG ) MOTHER ) ");
    mtlist(pattern);
    newbot(last_chunk_as_bcd("/"), pattern);
    newbot(last_chunk_as_bcd("STRUNG"), pattern);
    TEST_EQUAL(lprintstr(pattern), "( / STRUNG ) ");
    TEST_EQUAL(goody(pattern, word), 0); // STRUNG incorrectly matches HIGHLYSTRUNG
    popbot(pattern);
    newbot(last_chunk_as_bcd("HIGHLY"), pattern);
    TEST_EQUAL(lprintstr(pattern), "( / HIGHLY ) ");
    TEST_EQUAL(goody(pattern, word), 0); // HIGHLY incorrectly matches HIGHLYSTRUNG
    lnkbot(last_chunk_as_bcd("STRUNG"), pattern);
    TEST_EQUAL(lprintstr(pattern), "( / HIGHLYSTRUNG ) ");
    TEST_EQUAL(goody(pattern, word), 0); // HIGHLYSTRUNG "correctly" matches HIGHLYSTRUNG
    popbot(pattern);
    lnkbot(last_chunk_as_bcd("LIKELY"), pattern);
    TEST_EQUAL(lprintstr(pattern), "( / HIGHLYLIKELY ) ");
    TEST_EQUAL(goody(pattern, word), 0); // HIGHLYLIKELY incorrectly matches HIGHLYSTRUNG
    popbot(pattern);
    popbot(pattern);
    newbot(last_chunk_as_bcd("FAMILY"), pattern);
    lnkbot(last_chunk_as_bcd("FRIEND"), pattern);
    TEST_EQUAL(lprintstr(pattern), "( / FAMILYFRIEND ) ");
    TEST_EQUAL(goody(pattern, word), 0); // FAMILYFRIEND incorrectly matches FAMILY
}


DEF_TEST_FUNC(test_slip_ymatch)
{
    initas();

    auto lprintstr = [](machine_word lst) {
        std::stringstream s;
        not_slip_lprint(lst, s);
        return s.str(); 
    };

    machine_word text;
    list(text);
    machine_word rule;
    list(rule);
    machine_word results;
    list(results);
    machine_word dlist;
    list(dlist);
    machine_word dlist2;
    list(dlist2);
    machine_word star;
    list(star);



    // decomposition rule (EGG), (EASTER) and (EASTEREGG)
    mtlist(text);
    newbot(last_chunk_as_bcd("EASTER"), text);
    TEST_EQUAL(lprintstr(text), "( EASTER ) ");
    mtlist(rule);
    newbot(last_chunk_as_bcd("EASTER"), rule);
    TEST_EQUAL(lprintstr(rule), "( EASTER ) ");
    mtlist(results);
    ymatch(rule, text, results);
    TEST_EQUAL(lprintstr(results), "( ( EASTER ) ) ");
    lnkbot(last_chunk_as_bcd("EGG"), text);
    TEST_EQUAL(lprintstr(text), "( EASTEREGG ) ");
    mtlist(results);
    TEST_EQUAL(ymatch(rule, text, results), 0);
    TEST_EQUAL(lprintstr(results), "( ) ");
    lnkbot(last_chunk_as_bcd("EGG"), rule);
    TEST_EQUAL(lprintstr(rule), "( EASTEREGG ) ");
    mtlist(results);
    ymatch(rule, text, results);
    TEST_EQUAL(lprintstr(results), "( ( EASTEREGG ) ) ");
    mtlist(rule);
    newbot(last_chunk_as_bcd("EASTER"), rule);
    TEST_EQUAL(lprintstr(rule), "( EASTER ) ");
    mtlist(results);
    TEST_EQUAL(ymatch(rule, text, results), 0);
    TEST_EQUAL(lprintstr(results), "( ) ");
    mtlist(rule);
    newbot(last_chunk_as_bcd("EGG"), rule);
    TEST_EQUAL(lprintstr(rule), "( EGG ) ");
    mtlist(results);
    TEST_EQUAL(ymatch(rule, text, results), 0);
    TEST_EQUAL(lprintstr(results), "( ) ");

    // decomposition rule (0 (/FAMILY) 0)
    mtlist(rule);
    newbot(0, rule);
    mtlist(dlist);
    newbot(last_chunk_as_bcd("/"), dlist);
    newbot(last_chunk_as_bcd("FAMILY"), dlist);
    newbot(dlist, rule);
    newbot(0, rule);
    TEST_EQUAL(lprintstr(rule), "( 0 ( / FAMILY ) 0 ) ");
    mtlist(text);
    newbot(last_chunk_as_bcd("SISTER"), text);
    newbot(dlist, text);
    TEST_EQUAL(lprintstr(text), "( SISTER ( / FAMILY ) ) ");
    mtlist(results);
    ymatch(rule, text, results);
    TEST_EQUAL(lprintstr(results), "( ( ) ( SISTER ) ( ) ) ");

    // decomposition rule (0 (/FAMILYX) 0)
    mtlist(rule);
    newbot(0, rule);
    mtlist(dlist);
    newbot(last_chunk_as_bcd("/"), dlist);
    newbot(last_chunk_as_bcd("FAMILY"), dlist);
    lnkbot(last_chunk_as_bcd("X"), dlist);
    newbot(dlist, rule);
    newbot(0, rule);
    TEST_EQUAL(lprintstr(rule), "( 0 ( / FAMILYX ) 0 ) ");
    mtlist(text);
    newbot(last_chunk_as_bcd("SISTER"), text);
    newbot(dlist, text);
    TEST_EQUAL(lprintstr(text), "( SISTER ( / FAMILYX ) ) ");
    mtlist(results);
    ymatch(rule, text, results);
    TEST_EQUAL(lprintstr(results), "( ( ) ( SISTER ) ( ) ) ");

    // decomposition rule (0 (/EGG) 0)
    mtlist(rule);
    newbot(0, rule);
    mtlist(dlist);
    newbot(last_chunk_as_bcd("/"), dlist);
    newbot(last_chunk_as_bcd("EGG"), dlist);
    newbot(dlist, rule);
    newbot(0, rule);
    TEST_EQUAL(lprintstr(rule), "( 0 ( / EGG ) 0 ) ");
    mtlist(text);
    newbot(last_chunk_as_bcd("CHOCO"), text);
    mtlist(dlist2);
    newbot(last_chunk_as_bcd("/"), dlist2);
    newbot(last_chunk_as_bcd("EASTER"), dlist2);
    lnkbot(last_chunk_as_bcd("EGG"), dlist2);
    newbot(dlist2, text);
    TEST_EQUAL(lprintstr(text), "( CHOCO ( / EASTEREGG ) ) ");
    mtlist(results);
    ymatch(rule, text, results);
    TEST_EQUAL(lprintstr(results), "( ) "); // fails! matches producing ( ( ) ( CHOCO ) ( ) ),
                                            // but /EGG should not match /EASTEREGG
#if 1
    // decomposition rule (1)
    mtlist(text);
    newbot(last_chunk_as_bcd("EASTER"), text);
    TEST_EQUAL(lprintstr(text), "( EASTER ) ");
    mtlist(rule);
    newbot(1, rule);
    TEST_EQUAL(lprintstr(rule), "( 1 ) ");
    mtlist(results);
    ymatch(rule, text, results);
    TEST_EQUAL(lprintstr(results), "( ( EASTER ) ) ");
    lnkbot(last_chunk_as_bcd("EGG"), text);
    TEST_EQUAL(lprintstr(text), "( EASTEREGG ) ");
    mtlist(results);
    ymatch(rule, text, results);
    TEST_EQUAL(lprintstr(results), "( ( EASTEREGG ) ) ");
    newbot(last_chunk_as_bcd("YUM"), text);
    TEST_EQUAL(lprintstr(text), "( EASTEREGG YUM ) ");
    mtlist(results);
    ymatch(rule, text, results);
    TEST_EQUAL(lprintstr(results), "( ) "); // fails! (1) matches (EASTEREGG YUM) producing (EASTEREGG)
    mtlist(rule);
    newbot(2, rule);
    TEST_EQUAL(lprintstr(rule), "( 2 ) ");
    mtlist(results);
    ymatch(rule, text, results);
    TEST_EQUAL(lprintstr(results), "( ( EASTEREGG YUM ) ) ");
    mtlist(rule);
    newbot(3, rule);
    TEST_EQUAL(lprintstr(rule), "( 3 ) ");
    mtlist(results);
//    ymatch(rule, text, results);
    TEST_EQUAL(lprintstr(results), "( ) "); // fails!

    // decomposition rule (0) and (0 0)
    mtlist(text);
    newbot(last_chunk_as_bcd("EASTER"), text);
    TEST_EQUAL(lprintstr(text), "( EASTER ) ");
    mtlist(rule);
    newbot(0, rule);
    TEST_EQUAL(lprintstr(rule), "( 0 ) ");
    mtlist(results);
    ymatch(rule, text, results);
    TEST_EQUAL(lprintstr(results), "( ( EASTER ) ) ");
    lnkbot(last_chunk_as_bcd("EGG"), text);
    TEST_EQUAL(lprintstr(text), "( EASTEREGG ) ");
    mtlist(results);
    ymatch(rule, text, results);
    TEST_EQUAL(lprintstr(results), "( ( EASTEREGG ) ) ");
    newbot(last_chunk_as_bcd("YUM"), text);
    TEST_EQUAL(lprintstr(text), "( EASTEREGG YUM ) ");
    mtlist(results);
    ymatch(rule, text, results);
    TEST_EQUAL(lprintstr(results), "( ( EASTEREGG YUM ) ) ");
    newtop(1, rule);
    TEST_EQUAL(lprintstr(rule), "( 1 0 ) ");
    mtlist(results);
    ymatch(rule, text, results);
    TEST_EQUAL(lprintstr(results), "( ( EASTEREGG ) ( YUM ) ) ");
    newbot(1, rule);
    TEST_EQUAL(lprintstr(rule), "( 1 0 1 ) ");
    mtlist(results);
    ymatch(rule, text, results);
    TEST_EQUAL(lprintstr(results), "( ( EASTEREGG ) ( ) ( YUM ) ) ");
    newtop(last_chunk_as_bcd("MY"), text);
    TEST_EQUAL(lprintstr(text), "( MY EASTEREGG YUM ) ");
    mtlist(results);
    ymatch(rule, text, results);
    TEST_EQUAL(lprintstr(results), "( ( MY ) ( EASTEREGG ) ( YUM ) ) ");
    newtop(1, rule);
    TEST_EQUAL(lprintstr(rule), "( 1 1 0 1 ) ");
    mtlist(results);
    ymatch(rule, text, results);
    TEST_EQUAL(lprintstr(results), "( ( MY ) ( EASTEREGG ) ( ) ( YUM ) ) ");
    newtop(last_chunk_as_bcd("IT'S"), text);
    TEST_EQUAL(lprintstr(text), "( IT'S MY EASTEREGG YUM ) ");
    mtlist(results);
    ymatch(rule, text, results);
    TEST_EQUAL(lprintstr(results), "( ( IT'S ) ( MY ) ( EASTEREGG ) ( YUM ) ) ");
    newtop(1, rule);
    TEST_EQUAL(lprintstr(rule), "( 1 1 1 0 1 ) ");
    mtlist(results);
    ymatch(rule, text, results);
    TEST_EQUAL(lprintstr(results), "( ( IT'S ) ( MY ) ( EASTEREGG ) ( ) ( YUM ) ) ");
    poptop(rule);
    newtop(last_chunk_as_bcd("IT'S"), rule);
    TEST_EQUAL(lprintstr(rule), "( IT'S 1 1 0 1 ) ");
    mtlist(results);
    ymatch(rule, text, results);
    TEST_EQUAL(lprintstr(results), "( ( IT'S ) ( MY ) ( EASTEREGG ) ( ) ( YUM ) ) ");
    poptop(rule);
    poptop(rule);
    newtop(last_chunk_as_bcd("MY"), rule);
    newtop(1, rule);
    TEST_EQUAL(lprintstr(rule), "( 1 MY 1 0 1 ) ");
    mtlist(results);
    ymatch(rule, text, results);
    TEST_EQUAL(lprintstr(results), "( ( IT'S ) ( MY ) ( EASTEREGG ) ( ) ( YUM ) ) ");
    mtlist(rule);
    newbot(1, rule);
    newbot(1, rule);
    newbot(last_chunk_as_bcd("EASTER"), rule);
    lnkbot(last_chunk_as_bcd("EGG"), rule);
    newbot(0, rule);
    newbot(1, rule);
    TEST_EQUAL(lprintstr(rule), "( 1 1 EASTEREGG 0 1 ) ");
    mtlist(results);
    ymatch(rule, text, results);
    TEST_EQUAL(lprintstr(results), "( ( IT'S ) ( MY ) ( EASTEREGG ) ( ) ( YUM ) ) ");
    poptop(rule);
    newtop(last_chunk_as_bcd("IT'S"), rule);
    TEST_EQUAL(lprintstr(rule), "( IT'S 1 EASTEREGG 0 1 ) ");
    mtlist(results);
    ymatch(rule, text, results);
    TEST_EQUAL(lprintstr(results), "( ( IT'S ) ( MY ) ( EASTEREGG ) ( ) ( YUM ) ) ");
    poptop(rule);
    poptop(rule);
    newtop(last_chunk_as_bcd("MY"), rule);
    newtop(1, rule);
    TEST_EQUAL(lprintstr(rule), "( 1 MY EASTEREGG 0 1 ) ");
    mtlist(results);
    //ymatch(rule, text, results);
    //TEST_EQUAL(lprintstr(results), "( ( IT'S ) ( MY ) ( EASTEREGG ) ( ) ( YUM ) ) ");
    poptop(rule);
    newtop(last_chunk_as_bcd("IT'S"), rule);
    TEST_EQUAL(lprintstr(rule), "( IT'S MY EASTEREGG 0 1 ) ");
    mtlist(results);
    ymatch(rule, text, results);
    TEST_EQUAL(lprintstr(results), "( ( IT'S ) ( MY ) ( EASTEREGG ) ( ) ( YUM ) ) ");
    mtlist(rule);
    lsscpy(text, rule);
    TEST_EQUAL(lprintstr(rule), "( IT'S MY EASTEREGG YUM ) ");
    mtlist(results);
    ymatch(rule, text, results);
    TEST_EQUAL(lprintstr(results), "( ( IT'S ) ( MY ) ( EASTEREGG ) ( YUM ) ) ");
    /*newbot(0, rule);
    TEST_EQUAL(lprintstr(rule), "( 1 0 1 0 ) ");
    mtlist(results);
    ymatch(rule, text, results);
    TEST_EQUAL(lprintstr(results), "( ( EASTEREGG ) ( ) ( YUM ) ( ) ) ");
    */
#endif
    
    // decomposition rule (2)
    mtlist(text);
    newbot(last_chunk_as_bcd("EASTER"), text);
    TEST_EQUAL(lprintstr(text), "( EASTER ) ");
    mtlist(rule);
    newbot(2, rule);
    TEST_EQUAL(lprintstr(rule), "( 2 ) ");
    mtlist(results);
//    ymatch(rule, text, results);
//    TEST_EQUAL(lprintstr(results), "( ) ");
    newbot(last_chunk_as_bcd("EGG"), text);
    TEST_EQUAL(lprintstr(text), "( EASTER EGG ) ");
    mtlist(results);
    ymatch(rule, text, results);
    TEST_EQUAL(lprintstr(results), "( ( EASTER EGG ) ) ");

    // decomposition rule (ORANGE 0 ORANGE 0 ORANGE)
    mtlist(rule);
    newbot(last_chunk_as_bcd("ORANGE"), rule);
    newbot(0, rule);
    newbot(last_chunk_as_bcd("ORANGE"), rule);
    newbot(0, rule);
    newbot(last_chunk_as_bcd("ORANGE"), rule);
    TEST_EQUAL(lprintstr(rule), "( ORANGE 0 ORANGE 0 ORANGE ) ");
    mtlist(text);
    newbot(last_chunk_as_bcd("ORANGE"), text);
    newbot(last_chunk_as_bcd("THE"), text);
    newbot(last_chunk_as_bcd("RAIN"), text);
    newbot(last_chunk_as_bcd("ORANGE"), text);
    newbot(last_chunk_as_bcd("IN"), text);
    newbot(last_chunk_as_bcd("SPAIN"), text);
    newbot(last_chunk_as_bcd("ORANGE"), text);
    TEST_EQUAL(lprintstr(text), "( ORANGE THE RAIN ORANGE IN SPAIN ORANGE ) ");
    mtlist(results);
//    ymatch(rule, text, results);
    TEST_EQUAL(lprintstr(results), "( ( ORANGE ) ( THE RAIN ) ( ORANGE ) ( IN SPAIN ) ( ORANGE ) ) ");

    // decomposition rule (MARY 2 2 ITS 1 0)
    mtlist(text);
    newbot(last_chunk_as_bcd("MARY"), text);
    newbot(last_chunk_as_bcd("HAD"), text);
    newbot(last_chunk_as_bcd("A"), text);
    newbot(last_chunk_as_bcd("LITTLE"), text);
    newbot(last_chunk_as_bcd("LAMB"), text);
    newbot(last_chunk_as_bcd("ITS"), text);
    newbot(last_chunk_as_bcd("PROBAB"), text);
    lnkbot(last_chunk_as_bcd("ILITY"), text);
    newbot(last_chunk_as_bcd("WAS"), text);
    newbot(last_chunk_as_bcd("ZERO"), text);
    TEST_EQUAL(lprintstr(text), "( MARY HAD A LITTLE LAMB ITS PROBABILITY WAS ZERO ) ");
    mtlist(rule);
    newbot(last_chunk_as_bcd("MARY"), rule);
    newbot(2, rule);
    newbot(2, rule);
    newbot(last_chunk_as_bcd("ITS"), rule);
    newbot(1, rule);
    newbot(0, rule);
    TEST_EQUAL(lprintstr(rule), "( MARY 2 2 ITS 1 0 ) ");
    mtlist(results);
    TEST_EQUAL(ymatch(rule, text, results), results);
    TEST_EQUAL(lprintstr(results), "( ( MARY ) ( HAD A ) ( LITTLE LAMB ) ( ITS ) ( PROBABILITY ) ( WAS ZERO ) ) ");
    mtlist(rule);
    newbot(0, rule);
    newbot(1, rule);
    newbot(0, rule);
    newbot(1, rule);
    newbot(0, rule);
    newbot(1, rule);
    newbot(0, rule);
    TEST_EQUAL(lprintstr(rule), "( 0 1 0 1 0 1 ) ");
    mtlist(results);
//    ymatch(rule, text, results);
    TEST_EQUAL(lprintstr(results), "( ( MARY ) ( HAD A ) ( LITTLE LAMB ) ( ITS ) ( PROBABILITY ) ( WAS ZERO ) ) ");

    // decomposition rule (0 A)
    mtlist(rule);
    newbot(0, rule);
    newbot(last_chunk_as_bcd("A"), rule);
    TEST_EQUAL(lprintstr(rule), "( 0 A ) ");
    mtlist(text);
    newbot(last_chunk_as_bcd("A"), text);
    mtlist(results);
    TEST_EQUAL(ymatch(rule, text, results), results);
    TEST_EQUAL(lprintstr(results), "( ( ) ( A ) ) ");
    newtop(last_chunk_as_bcd("B"), text);
    TEST_EQUAL(lprintstr(text), "( B A ) ");
    mtlist(results);
    TEST_EQUAL(ymatch(rule, text, results), results);
    TEST_EQUAL(lprintstr(results), "( ( B ) ( A ) ) ");

    // decomposition rule (0 A 0)
    mtlist(rule);
    newbot(0, rule);
    newbot(last_chunk_as_bcd("A"), rule);
    newbot(0, rule);
    TEST_EQUAL(lprintstr(rule), "( 0 A 0 ) ");
    mtlist(text);
    newbot(last_chunk_as_bcd("A"), text);
    mtlist(results);
    TEST_EQUAL(ymatch(rule, text, results), results);
    TEST_EQUAL(lprintstr(results), "( ( ) ( A ) ( ) ) ");
    newbot(last_chunk_as_bcd("T"), text);
    TEST_EQUAL(lprintstr(text), "( A T ) ");
    mtlist(results);
    TEST_EQUAL(ymatch(rule, text, results), results);
    TEST_EQUAL(lprintstr(results), "( ( ) ( A ) ( T ) ) ");
    newtop(last_chunk_as_bcd("C"), text);
    TEST_EQUAL(lprintstr(text), "( C A T ) ");
    mtlist(results);
    TEST_EQUAL(ymatch(rule, text, results), results);
    TEST_EQUAL(lprintstr(results), "( ( C ) ( A ) ( T ) ) ");

    // decomposition rule (0 (* SHORT PORCUPINE LONG) 0)
    mtlist(rule);
    newbot(0, rule);
    mtlist(star);
    newbot(last_chunk_as_bcd("*"), star);
    newbot(last_chunk_as_bcd("SHORT"), star);
    newbot(last_chunk_as_bcd("PORCUP"), star);
    lnkbot(last_chunk_as_bcd("INE"), star);
    newbot(last_chunk_as_bcd("LONG"), star);
    newbot(star, rule);
    newbot(0, rule);
    TEST_EQUAL(lprintstr(rule), "( 0 ( * SHORT PORCUPINE LONG ) 0 ) ");
    mtlist(text);
    newbot(last_chunk_as_bcd("SHORT"), text);
    mtlist(results);
    TEST_EQUAL(ymatch(rule, text, results), results);
    TEST_EQUAL(lprintstr(results), "( ( ) ( SHORT ) ( ) ) ");
    mtlist(text);
    newbot(last_chunk_as_bcd("PORCUP"), text);
    lnkbot(last_chunk_as_bcd("INE"), text);
    mtlist(results);
    TEST_EQUAL(ymatch(rule, text, results), results);
    TEST_EQUAL(lprintstr(results), "( ( ) ( PORCUPINE ) ( ) ) ");
    mtlist(text);
    newbot(last_chunk_as_bcd("INE"), text);
    mtlist(results);
    TEST_EQUAL(ymatch(rule, text, results), 0); // The "S IS A STRONG WORD" bug is fixed
    TEST_EQUAL(lprintstr(results), "( ) ");
    mtlist(text);
    newbot(last_chunk_as_bcd("LONG"), text);
    mtlist(results);
    TEST_EQUAL(ymatch(rule, text, results), results);
    TEST_EQUAL(lprintstr(results), "( ( ) ( LONG ) ( ) ) ");
    newbot(last_chunk_as_bcd("B"), text);
    TEST_EQUAL(lprintstr(text), "( LONG B ) ");
    mtlist(results);
    TEST_EQUAL(ymatch(rule, text, results), results);
    TEST_EQUAL(lprintstr(results), "( ( ) ( LONG ) ( B ) ) ");
    newtop(last_chunk_as_bcd("A"), text);
    TEST_EQUAL(lprintstr(text), "( A LONG B ) ");
    mtlist(results);
    ymatch(rule, text, results);
    TEST_EQUAL(lprintstr(results), "( ( A ) ( LONG ) ( B ) ) ");
}



}//namespace slip


int main()
{
    try {
        RUN_TESTS();
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
