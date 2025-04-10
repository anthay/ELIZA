# A Simulation in C++ of Joseph Weizenbaum’s 1966 ELIZA

UPDATE: Since writing this the source code for ELIZA has been found. I've written about that [here](https://github.com/anthay/ELIZA/blob/master/doc/Eliza_part_2.md).

I’ve made in C++ what I think is an accurate simulation of the original ELIZA. It is a console application that takes as input the original format script file, which looks like a series of S-expressions, and then waits for the user to type a line of text before responding with a line of text of its own.

I believe it to be an accurate simulation of the original because I followed closely the description Weizenbaum gives in an article on page 36 of the January
1966 edition of Communications of the ACM titled *ELIZA - A Computer Program For the Study of Natural Language Communication Between Man And Machine*. When given the same prompts, this simulation reproduces exactly the conversation shown in the 1966 CACM article. The C++ source is [eliza.cpp](https://github.com/anthay/ELIZA/blob/master/src/eliza.cpp) and a transcription of the original 1966 script file is [here](https://github.com/anthay/ELIZA/blob/master/scripts/ELIZA-script-DOCTOR-original-1966-CACM-appendix.txt).

That’s it. Read on for fluf.

---

I believe I first read about ELIZA in 1978. My favourite teacher at secondary school was Alfred Nussbaum, who taught us physics. He knew of my interest in computers and suggested I read Joseph Weizenbaum’s book *Computer Power and Human Reason*.

![Three students in a classroom at the end of their school days stand in front of a blackboard covered in calculations and diagrams, behind a balding, white haired, seated teacher in a white lab coat](https://github.com/anthay/ELIZA/blob/master/doc/1979AlfredNussbaum.jpg)

Craig, Stuart and me with Alfred Nussbaum in the physics lab in Hengrove Comprehensive School, Bristol, England, 1979.

I recently found my copy of Mr. Weizenbaum’s book and reread it. He seemed to have a strong feelings about people who find computers fascinating.

>How may the compulsive programmer be distinguished from a merely dedicated, hard-working professional programmer? First, by the fact that the ordinary professional programmer addresses himself to the problem to be solved, whereas the compulsive programmer sees the problem mainly as an opportunity to interact with the computer. The ordinary computer programmer will usually discuss both his substantive and his technical programming problem with others. He will generally do lengthy preparatory work, such as writing and flow diagramming, before beginning work with the computer itself. His sessions with the computer may be comparatively short. He may even let others do the actual console work. He develops his program slowly and systematically. When something doesn’t work, he may spend considerable time away from the computer, framing careful hypotheses to account for the malfunction and designing crucial experiments to test them. Again, he may leave the actual running of the computer to others. He is able, while waiting for results from the computer, to attend to other aspects of his work, such as documenting what he has already done. When he has finally composed the program he set out to produce, he is able to complete a sensible description of it and to turn his attention to other things. The professional regards programming as a means toward an end, not as an end in itself. His satisfaction comes from having solved a substantive problem, not from having bent a computer to his will. [Computer Power and Human Reason, page 116]

The book contains many pages in this vein.

Does it matter *how* we accomplish things? Isn’t *what* we accomplish more important? (I’m not suggesting ends can always justify means.) If I do good work sitting at the computer, and you do good work sitting away from the computer, does it really matter? Or perhaps sometimes I do good work sitting at the computer, and sometimes I do good work while walking in the woods. Who cares?

On reflection, I’ve never solved a substantive problem in my life. I do get enjoyment out of bending a computer to my will - otherwise known as programming. It isn’t an academic exercise for me; I like making a machine do something useful or interesting. If there were no computers to execute my algorithms, I wouldn’t write them. I wonder if Weizenbaum would consider whatever problem ELIZA solves to be substantive.

People play and express themselves in many different mediums. Paint, clay, words, sound, theatre... I don’t see the harm in code being another medium in which one might immerse oneself.

Wikipedia says this is what Computer power and Human Reason is about:

>...while Artificial Intelligence may be possible, we should never allow computers to make important decisions because computers will always lack human qualities such as compassion and wisdom. Weizenbaum makes the crucial distinction between deciding and choosing. Deciding is a computational activity, something that can ultimately be programmed. Choice, however, is the product of judgment, not calculation. It is the capacity to choose that ultimately makes us human. Comprehensive human judgment is able to include non-mathematical factors, such as emotions. Judgment can compare apples and oranges, and can do so without quantifying each fruit type and then reductively quantifying each to factors necessary for comparison. \[[Wikipedia (11 Feb 2021)](https://en.wikipedia.org/wiki/Joseph_Weizenbaum)\]

I presume the distinction between deciding and choosing relates to the very old question about freewill and determinism. I don't believe in magic; I think we are subject to the laws of nature, and so we are deterministic. Choice maybe the product of judgment, but I think judgement must be the product of calculation (i.e. the deterministic behaviour of the particles that constitute our world).

I'm not 100% sure, but I think Weizenbaum would agree with this. For example, he says:

>Still, the extreme or hardcore wing of the artificial intelligentsia will insist that the whole man, to again use Simon's expression, is after all an information processor, and that an information-processing theory of man must therefore be adequate to account for his behavior in its entirety. We may agree with the major premise without necessarily drawing the indicated conclusion. We have already observed that a portion of the information the human "processes" is kinesthetic, that it is "stored" in his muscles and joints. It is simply not clear that such information, and the processing associated with it, can be represented in the form of computer programs and data structures at all.
>
>It may, of course, be argued that it is in principle possible for a computer to simulate the entire network of cells that constitutes the human body. But that would introduce a theory of information processing entirely different from any which has so far been advanced. Besides, such a simulation would result in "behavior" on such an incredibly long-time scale that no robot built on such principles could possibly interact with human beings. Finally, there appears to be no prospect whatever that mankind will know enough neurophysiology within the next several hundred years to have the intellectual basis for designing such a machine. We may therefore dismiss such arguments. [Computer Power and Human Reason, page 213]

Cf. the project to simulate a worm at the cellular level \[[Wikipedia](https://en.wikipedia.org/wiki/OpenWorm)\].

At other times I think Weizenbaum is saying it will never happen:

> But, and this is the saving grace of which an insolent and arrogant scientism attempts to rob us, we come to know and understand not only by way of the mechanisms of the conscious. We are capable of listening with the third ear, of sensing living truth that is truth beyond any standards of provability. It is *that* kind of understanding, and the kind of intelligence that is derived from it, which I claim is beyond the abilities of computers to simulate. [Computer Power and Human Reason, page 222]

I think Weizenbaum is saying science can never fully model what it is to be a human being and "there are objectives that are not appropriately assignable to machines” [page 210]. This critic would not disagree. But I would have preferred him to have made this central point more straightforwardly and spent less time waffling and calling people names. (I don't claim to know what "listening with the third ear, of sensing living truth that is truth beyond any standards of provability" means. Love, perhaps?)

Whether we can simulate human intelligence or not, computers are powerful tools that can be used by people to solve difficult problems we almost certainly couldn't solve without them. Some of these solutions will be good for humanity and all life on Earth. Others, maybe not so much... This puts me in mind of the very old quip: *to err is human; to really foul things up requires a computer*.

Cf. James Lovelock's Novacene \[[Wikipedia](https://en.wikipedia.org/wiki/Novacene)\].

---

Rediscovering the book set me off looking for the original 1966 CACM article about ELIZA. Having found and read the article I thought, in the spirit of exploration,  it would be fun to recreate ELIZA for myself. 

The article describing how ELIZA works is mostly fairly clear, except for when it gets mysterious: “...and a certain counting mechanism is in a particular state...” But the description stretches over eight pages, and I found it harder than I expected to translate into code. I think a page of pseudocode would have made much of the text superfluous.

This is my recreation of the core ELIZA algorithm, from my understanding of Weizenbaum’s description. I did the actual console work myself.

```cpp
// produce a response to the given 'input' using the given 'rules'
std::string eliza(rulemap & rules, const std::string & input)
{
    // for simplicity, convert the given input string to a list of uppercase words
    // e.g. "Hello, world!" -> (HELLO , WORLD !)
    stringlist words(split(to_upper(input)));


    // scan for keywords [page 38 (c)]; build the keystack; apply word substitutions
    stringlist keystack;
    int top_rank = 0;
    for (auto word = words.begin(); word != words.end(); ) {
        if (punctuation(*word)) {
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

        const auto r = rules.find(*word);
        if (r != rules.end()) {
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

    auto memory_rule = get_rule<rule_memory>(rules, SPECIAL_RULE_MEMORY);
    if (keystack.empty()) {
        // a text without keywords; can we recall a MEMORY? [page 41 (f)]
        if (memory_rule->is_time_for_recollection())
            return memory_rule->recall_memory();
    }

    // build tag mapping so that e.g. tags[BELIEF] -> (BELIEVE FEEL THINK WISH)
    const tagmap tags(collect_tags(rules));

    // the keystack contains all keywords that occur in the given 'input';
    // apply transformation associated with the top keyword [page 39 (d)]
    while (!keystack.empty()) {
        const std::string top_keyword = pop_front(keystack);

        auto rule = rules.find(top_keyword);
        if (rule == rules.end())
            break; // e.g. could happen if a rule links to a non-existent keyword

        // try to lay down a memory for future use
        memory_rule->create_memory(top_keyword, words, tags);

        // perform the transformation for this rule
        std::string link_keyword;
        auto act = rule->second->apply_transformation(words, tags, link_keyword);

        if (act == rule_base::complete)
            return join(words); // decomposition/reassembly successfully applied

        if (act == rule_base::inapplicable)
            break; // no decomposition rule matched the input words

        if (act == rule_base::linkkey)
            keystack.push_front(link_keyword); // rule links to another; loop

        // (rule_base::newkey -> rule wants to try next highest keyword, if any)
        assert(act == rule_base::linkkey || act == rule_base::newkey);
    }


    // last resort: the NONE rule never fails to produce a response [page 41 (d)]
    auto none_rule = get_rule<rule_vanilla>(rules, SPECIAL_RULE_NONE);
    std::string discard;
    none_rule->apply_transformation(words, tags, discard);
    return join(words);
}
```

From [eliza.cpp](https://github.com/anthay/ELIZA/blob/master/src/eliza.cpp).

Example build under Windows 10 with MS Visual Studio 2019 Community command line:


```text
**********************************************************************
** Visual Studio 2019 Developer Command Prompt v16.8.5
** Copyright (c) 2020 Microsoft Corporation
**********************************************************************
[vcvarsall.bat] Environment initialized for: 'x64'

C:\Program Files (x86)\Microsoft Visual Studio\2019\Community>cd C:\Eliza

C:\Eliza>dir /b
ELIZA-script-DOCTOR-original-1966-CACM-appendix.txt
eliza.cpp

C:\Eliza>cl /EHsc /W4 eliza.cpp
Microsoft (R) C/C++ Optimizing Compiler Version 19.28.29337 for x64
Copyright (C) Microsoft Corporation.  All rights reserved.

eliza.cpp
Microsoft (R) Incremental Linker Version 14.28.29337.0
Copyright (C) Microsoft Corporation.  All rights reserved.

/out:eliza.exe
eliza.obj

C:\Eliza>eliza
Usage: eliza <scriptfile>
  where <scriptfile> is in original 1966 ELIZA script format

C:\Eliza>eliza ELIZA-script-DOCTOR-original-1966-CACM-appendix.txt
HOW DO YOU DO. PLEASE TELL ME YOUR PROBLEM

Men are all alike.
IN WHAT WAY

^C
C:\Eliza>
```


As I mentioned before, my ELIZA simulation reproduces the exact conversation shown in both the 1966 CACM article, and reproduced ten years later in Computer Power and Human Reason. To remind you, this is that conversation. ELIZA output in all caps.

```text
Men are all alike.
IN WHAT WAY
They're always bugging us about something or other.
CAN YOU THINK OF A SPECIFIC EXAMPLE
Well, my boyfriend made me come here.
YOUR BOYFRIEND MADE YOU COME HERE
He says I'm depressed much of the time.
I AM SORRY TO HEAR YOU ARE DEPRESSED
It's true. I am unhappy.
DO YOU THINK COMING HERE WILL HELP YOU NOT TO BE UNHAPPY
I need some help, that much seems certain.
WHAT WOULD IT MEAN TO YOU IF YOU GOT SOME HELP
Perhaps I could learn to get along with my mother.
TELL ME MORE ABOUT YOUR FAMILY
My mother takes care of me.
WHO ELSE IN YOUR FAMILY TAKES CARE OF YOU
My father.
YOUR FATHER
You are like my father in some ways.
WHAT RESEMBLANCE DO YOU SEE
You are not very aggressive, but I think you don't want me to notice that.
WHAT MAKES YOU THINK I AM NOT VERY AGGRESSIVE
You don't argue with me.
WHY DO YOU THINK I DON'T ARGUE WITH YOU
You are afraid of me.
DOES IT PLEASE YOU TO BELIEVE I AM AFRAID OF YOU
My father is afraid of everybody.
WHAT ELSE COMES TO MIND WHEN YOU THINK OF YOUR FATHER
Bullies.
DOES THAT HAVE ANYTHING TO DO WITH THE FACT THAT YOUR BOYFRIEND MADE YOU COME HERE
```

Having fed in the above prompts and received the expected responses, I thought I’d try to play the part of the “young lady” and continue this original 1966 conversation.

```text
My boyfriend loves me, he's not a bully.
WHY DO YOU SAY YOUR BOYFRIEND LOVES YOU
He annoys me sometimes, but I know he cares about me.
YOU SAY YOU KNOW HE CARES ABOUT YOU
That's why he wanted me to come here, because he knows I'm unhappy.
WHY DO YOU ASK
I didn't.
CAN YOU ELABORATE ON THAT
He knows I'm not getting on with my family and thought it would help me if I talked to someone.
DO YOU THINK ITS LIKELY THAT YOU TALKED TO SOMEONE
That's why I'm talking to you.
DOES THAT QUESTION INTEREST YOU
What question?
WHAT IS IT YOU REALLY WANT TO KNOW
How to have a good relationship with my parents.
DOES THAT SUGGEST ANYTHING ELSE WHICH BELONGS TO YOU
A brighter future?
I AM NOT SURE I UNDERSTAND YOU FULLY
If I could get on better with my parents I'd be a lot happier.
DO YOU WISH THAT YOU COULD GET ON BETTER WITH YOUR PARENTS I'D BE A LOT HAPPIER
I do.
DO YOU SAY YOU DO FOR SOME SPECIAL REASON
Not really.
DOES THAT HAVE ANYTHING TO DO WITH THE FACT THAT YOUR MOTHER
I'll never be as good as my sister in my mom's eyes.
TELL ME MORE ABOUT YOUR FAMILY
My sister did better at school than me and she's always got on better with my parents.
WHO ELSE IN YOUR FAMILY DID BETTER AT SCHOOL THAN YOU AND SHE'S ALWAYS GOT ON BETTER WITH YOUR PARENTS
It's just me and my sister.
YOUR SISTER
That's right.
BUT YOUR MOTHER TAKES CARE OF YOU
She does, sure. She's not mean to me. It's just little things she does, probably without even knowing
  she's doing them, that make me feel stupid and not as loved as my sister.
WHAT ELSE COMES TO MIND WHEN YOU THINK OF YOUR SISTER
I don't think she likes me much either.
DON'T YOU REALLY THINK SHE LIKES YOU MUCH EITHER
She's older than me and I was always her silly little annoying sister.
WERE YOU REALLY
She thought so.
BUT YOUR FATHER
He dotes on her too.
PLEASE GO ON
```


See [ELIZA-script-DOCTOR-original-1966-CACM-appendix.txt](https://github.com/anthay/ELIZA/blob/master/scripts/ELIZA-script-DOCTOR-original-1966-CACM-appendix.txt) for a transcription of the complete DOCTOR script from the 1966 CACM paper.

Anthony Hay, February 2021.

There is more information about ELIZA [here](http://elizagen.org/).


