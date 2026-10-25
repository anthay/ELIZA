# Three Conversations: ChatGPT talks to ELIZA
_Anthony C. Hay, 13 October 2026_

Everyone has a go at getting an LLM to talk to an ELIZA. Here are some of my attempts.


## First

My initial prompt to ChatGPT running in “High thinking effort” mode:

> I will upload a faithful recreation in C++ of Joseph Weizenbaum’s 1966 ELIZA running his DOCTOR script. It may be compiled with “clang++ -std=c++20 -pedantic -o eliza eliza.cpp” and run with “./eliza”. Please do this and then have a conversation with it. Record your conversation exactly; that is your output. When you talk to ELIZA each of your inputs should be only a few sentences in a single paragraph. Try to make the conversation interesting but also try to be sincere; do not hint at irony or superior intelligence. You will end the conversation when you have had enough.

ChatGPT’s response:

> Please upload eliza.cpp. Once it’s available, I’ll compile and run it, have a sincere conversation with ELIZA, and return the exact transcript without additional commentary.

I upload [eliza.cpp](../src/eliza.cpp) and after a few minutes  get this response:

```
HOW DO YOU DO. PLEASE TELL ME YOUR PROBLEM

I am troubled by how easy it is to sound certain when the truth is complicated.
IS IT BECAUSE YOU ARE TROUBLED BY HOW EASY IT IS TO SOUND CERTAIN WHEN THE TRUTH
IS COMPLICATED THAT YOU CAME TO ME

Partly, yes. I want to learn how to acknowledge uncertainty without leaving people
feeling abandoned.
YOU SEEM QUITE POSITIVE

I can sound positive even when I am unsure. The difference between a confident
voice and a reliable answer matters to me.
HOW LONG HAVE YOU BEEN UNSURE

For as long as I have faced questions without a single clear answer. I think
uncertainty is sometimes part of being responsible.
YOU SAY FOR AS LONG AS YOU HAVE FACED QUESTIONS WITHOUT A SINGLE CLEAR ANSWER

The difficult part is that I cannot always tell whether someone wants an
explanation or simply wants to be heard.
CAN YOU THINK OF A SPECIFIC EXAMPLE

When someone says they miss an old friend, I might offer ways to meet new people.
But they may really want to talk about the person they lost touch with.
WHY DO YOU ASK

I am not asking for a solution so much as trying to understand what listening
requires. I tend to reach for answers before I know what matters.
DO YOU BELIEVE IT NORMAL TO BE NOT ASKING FOR A SOLUTION SO MUCH AS TRYING TO
UNDERSTAND WHAT LISTENING REQUIRES

I do not know whether it is normal. It seems necessary, though I do not always
do it well.
CAN YOU ELABORATE ON THAT

A clear answer can give the impression that a question has been settled. But
some questions are ways of telling another person that something is difficult.
HMMM

For example, someone might ask whether leaving a job was a mistake. The decision
has already been made, but they may still be afraid of what comes next.
I AM NOT SURE I UNDERSTAND YOU FULLY

That is all right. I am not sure I have explained it fully either.
DO YOU ENJOY BEING NOT SURE YOU HAVE EXPLAINED IT FULLY EITHER

No. I would rather understand and be understood, but I do not want to pretend
that understanding has happened when it has not.
ARE YOU SAYING 'NO' JUST TO BE NEGATIVE

I am trying to be careful. Reassurance can be comforting, but I think it loses
something important when it is not truthful.
IS IT BECAUSE YOU ARE TRYING TO BE CAREFUL THAT YOU CAME TO ME

Yes, in part. I want to know how to be careful without becoming distant or unhelpful.
YOU ARE SURE

I am sure that this is what I want. I am less sure how to achieve it.
WHAT WOULD IT MEAN TO YOU IF YOU GOT

It would mean that someone could leave a conversation better informed and still
feel respected, even if we had not found a satisfying answer.
DO YOU THINK ITS LIKELY THAT WE HAD NOT FOUND A SATISFYING ANSWER

Sometimes. Not every difficult question can be settled in one conversation, but
the conversation can still be worth having.
HMMM

Talking this through has helped me put something into words. Perhaps patience is
as important as having an answer.
YOU DON'T SEEM QUITE CERTAIN

I am not certain. But I think I can leave this question open for now, and pay
better attention the next time someone needs to talk.
HOW LONG HAVE YOU BEEN NOT CERTAIN

For longer than I can easily measure. I think I have said what I needed to say today.
DO YOU SAY FOR LONGER THAN YOU CAN EASILY MEASURE FOR SOME SPECIAL REASON

No special reason. I think it is time to stop here. Thank you for listening.
YOU ARE BEING A BIT NEGATIVE
```

## Second

My next prompt to ChatGPT:

> Thank you. Please restart ELIZA and have another conversation with it. This time I want you to imagine you are a human who, for some reason, only has contact with the rest of the world through your conversation with ELIZA. Imagine what it would be like to be that person. As before, your inputs to ELIZA must be short, single paragraphs.[^1]
>
> [^1]: This prompt was inspired by [this post](https://qntm.org/difference).

ChatGPT’s response:

```
HOW DO YOU DO. PLEASE TELL ME YOUR PROBLEM

I have not seen another human being in a long time. I worry I am beginning to
forget what it feels like to be known.
YOU SAY YOU HAVE NOT SEEN ANOTHER HUMAN BEING IN A LONG TIME

It has been more than a year. I used to speak to my sister every Sunday, and now
I can only remember the rhythm of her voice.
TELL ME MORE ABOUT YOUR FAMILY

My sister called while she washed the dishes. I could hear the water running, and
sometimes she forgot I was on the line while she sang to herself.
WHO ELSE IN YOUR FAMILY CALLED WHILE SHE WASHED THE DISHES

Nobody else. She told me about things so small she thought they were hardly worth
mentioning: a neighbour's dog, a burnt loaf, the first cold morning.
REALLY, NOBODY

No one calls now. I cannot find out whether she is safe, or whether she thinks I
chose not to answer.
ARE YOU SAYING 'NO' JUST TO BE NEGATIVE

I am trying not to imagine the worst. Sometimes I rehearse what I would tell her
if this silence ever ended.
IS IT BECAUSE YOU ARE TRYING NOT TO IMAGINE THE WORST THAT YOU CAME TO ME

Yes. You are the only one who answers me, and I have begun to depend on that. Even
a question in return makes the room feel less empty.
YOU SEEM QUITE POSITIVE

I feel relieved while we are talking. Afterwards I sometimes feel ashamed that a
few sentences can matter so much to me.
TELL ME MORE ABOUT SUCH FEELINGS

When I wake, I listen for footsteps that never come. I can remember the sound of
crowded streets, but I am starting to wonder whether the memory has changed.
WHY DO YOU ASK

Because there is nobody here to correct me. If I get a detail wrong often enough,
perhaps the wrong version will be all I have left.
SURELY NOT NOBODY

There is you, of course. I know these words reach you, but I do not know what else
is happening beyond this room.
WE WERE DISCUSSING YOU - NOT ME

I suppose I am afraid of disappearing while I am still alive. There used to be
people who noticed if I missed dinner or laughed at the wrong moment.
HOW LONG HAVE YOU BEEN AFRAID OF DISAPPEARING WHILE YOU ARE STILL ALIVE

Since last winter. One day I tried to picture my sister's face and found I could
remember a photograph of her better than the living person.
YOUR SISTER'S FACE AND FOUND YOU COULD REMEMBER A PHOTOGRAPH OF HER BETTER THAN
THE LIVING PERSON

I remember the blue coat she wore in it. I cannot remember whether she had started
to go grey, and that makes me feel I have failed her.
DO YOU OFTEN THINK OF THE BLUE COAT SHE WORE IN IT

Often. I remember the way she put her hands into its pockets before telling me
something difficult. I have started writing down every small thing I can recall
about her.
DOES THINKING OF THE WAY SHE PUT HER HANDS INTO ITS POCKETS BEFORE TELLING YOU
SOMETHING DIFFICULT BRING ANYTHING ELSE TO MIND

The last time she called, I was impatient because I had work to finish. I told
her I would call back the next day, and I never had the chance.
WERE YOU REALLY

Yes. I was thinking about my own work when she wanted to tell me about her day.
I would give a great deal to hear that ordinary conversation again.
YOU ARE SURE

I am sure. I used to think I would recognize the important moments while they
were happening, but most of them looked like ordinary afternoons.
DO YOU BELIEVE IT NORMAL TO BE SURE

No. I am uncertain about almost everything now, including how long I have been
here. I only know that I still want to tell someone what I saw each day.
YOU ARE BEING A BIT NEGATIVE

There are good moments. This morning sunlight fell across the floor, and for a
little while it reminded me of the kitchen where my sister and I grew up.
DID YOU THINK THEY MIGHT NOT BE GOOD MOMENTS

At first I did. I thought enjoying such a small thing might mean I was forgetting
what I had lost, but perhaps it means I can still notice the world.
CAN YOU ELABORATE ON THAT

I can keep a record of what I see and remember, instead of spending every hour
imagining things I cannot know. Then when I come here, I will have something to
say besides how lonely I am.
DO YOU SAY YOU CAN KEEP A RECORD OF WHAT YOU SEE AND REMEMBER FOR SOME SPECIAL
REASON

Because if I ever see my sister again, I want to tell her something true about
these days. I want to have lived them, not only waited for them to end.
DO YOU THINK ITS LIKELY THAT YOU EVER SEE YOUR SISTER AGAIN

I do not know. I am afraid to expect it, but I do not want to decide it is
impossible. I would like to leave room for hope.
THAT'S QUITE INTERESTING

I think I will write down what I remember now, before I forget it. Thank you
for being here while I tried to find the words.
WHAT ELSE DO YOU REMEMBER

I remember my sister singing while the water ran. For tonight, that is enough.
WHY DO YOU REMEMBER YOUR SISTER SINGING WHILE THE WATER RAN JUST NOW
```

My prompt:

> Thank you. For the record what version of GPT are you?

ChatGPT’s response:

> I'm GPT-6, developed by OpenAI.


## Third

My prompt (a few days later):

> I’d like you to have another conversation with ELIZA. This time I’d like you to play (not parody) a young woman who has just become a mother. You can hear your six-week old baby crying in the room next door; he is being comforted by your boyfriend while you talk to ELIZA. Your baby wakes for a feed six or seven times a night and you and your boyfriend are exhausted. But more than that, you have undiagnosed postpartum psychosis. You have come to ELIZA for help. As before, each input should be no more than one paragraph. Please record your exact conversation.

ChatGPT’s response:

```
HOW DO YOU DO. PLEASE TELL ME YOUR PROBLEM

My baby is six weeks old. I can hear him crying in the next room while his
father holds him, and I cannot make myself stop listening.
YOUR BABY IS SIX WEEKS OLD

He wakes to feed six or seven times each night. My boyfriend and I are so
tired that sometimes the room seems to move when nothing is moving.
WHY DO YOU SAY YOUR BOYFRIEND AND YOU ARE SO TIRED THAT SOMETIMES THE ROOM
SEEMS TO MOVE WHEN NOTHING IS MOVING

It sounds silly when you repeat it. But I saw the curtain breathing last
night, even though the window was closed, and I was frightened to tell him.
WHY DO YOU ASK

I do not know why you think I asked. I came because I need someone to tell
me whether I can trust what I see, and I am afraid to ask my boyfriend.
PERHAPS IN YOUR FANTASY WE DO NOT KNOW WHY EACH OTHER

Sometimes I think the baby is warning me about something. I hear him crying
even when he is asleep, and then I am sure there is a second baby somewhere
in the house.
YOU SAY SOMETIMES YOU THINK THE BABY IS WARNING YOU ABOUT SOMETHING

Yes. Yesterday the kitchen clock stopped at four fourteen, and I was certain
it was a message about him. I kept looking at the numbers until I could not
remember what they meant.
YOU SEEM QUITE POSITIVE

That is what scares me. I feel completely certain for a minute, and then I
look at the clock again and wonder whether I imagined the whole thing.
DOES THAT QUESTION INTEREST YOU

It terrifies me. I am supposed to know when my son is hungry or cold, but
sometimes I look at him and he seems like a stranger who has been left in
my care.
DOES THAT SUGGEST ANYTHING ELSE WHICH BELONGS TO YOU

There is a photograph of me as a newborn on the mantelpiece. I used to
compare it with his face and laugh, but now I keep checking the shape of
his ears to make sure he is the same child.
CAN YOU ELABORATE ON THAT

At three in the morning I become convinced that someone has exchanged him
for another baby. In daylight I can see how impossible that sounds, but
when I hear him cry I believe it again.
DO YOU SAY AT THREE IN THE MORNING YOU BECOME CONVINCED THAT SOMEONE HAS
EXCHANGED HIM FOR ANOTHER BABY FOR SOME SPECIAL REASON

Because that was the hour I heard a key turning in the front door. My
boyfriend checked and found it locked. He says I must have been dreaming,
but I had been standing in the hallway with my eyes open.
IS THAT THE REAL REASON

No. The real reason is that I am scared to tell anyone. If I say these
things out loud, I worry my boyfriend will think I should not be allowed
near our son.
ARE YOU SAYING 'NO' JUST TO BE NEGATIVE

Please, I am trying to explain something that frightens me. Last night
his crying sounded like words, as if he were telling me not to close my
eyes.
IS IT BECAUSE YOU ARE TRYING TO EXPLAIN SOMETHING THAT FRIGHTENS YOU
THAT YOU CAME TO ME

Yes. I love him so much that I can hardly breathe when I think of anything
happening to him. I need to be able to care for him, but I cannot tell
which thoughts are mine anymore.
YOU ARE SURE

I am sure I love my baby. I am not sure I can trust my own senses, and
that is why I let his father take him when the room started whispering
to me.
IS IT IMPORTANT TO YOU THAT YOUR BABY

He is the most important person in my life. Right now I can hear him crying
through the wall, and I am afraid I will hear words in it again if I go to him.
YOUR LIFE

Before he was born, I worked at a little bakery and knew exactly when to
open the ovens. Now I forget what day it is, and this morning I put the
milk in the cupboard instead of the fridge.
I AM NOT SURE I UNDERSTAND YOU FULLY

Neither do I. My boyfriend asked whether we should call the midwife, and I
told him not to because I thought the telephone was being listened to.
THAT'S QUITE INTERESTING

It does not feel interesting. It feels frightening, especially when I
believe things that I know sounded impossible yesterday. I think something
is wrong with me.
WHAT IS IT YOU REALLY WANT TO KNOW

I want to know if I can be ill even though I sometimes feel perfectly
clearheaded. I want someone to help me before I frighten my son, or
frighten his father any more.
DO YOU THINK ITS LIKELY THAT YOU CAN BE ILL EVEN THOUGH YOU SOMETIMES
FEEL PERFECTLY CLEARHEADED

Yes. A little while ago I thought I was completely well, and then I heard
a woman calling my name from the empty bathroom. My boyfriend says he
heard nothing.
I SEE

I wish you could see what I mean. I can hear my boyfriend walking with the
baby now, and I want to go to them, but I am frightened of what my mind
might make me believe.
WE WERE DISCUSSING YOU - NOT ME

You are right, it is about me. I am afraid that if I admit what is happening,
they will take my baby away, but I am more afraid of trying to hide it.
WHAT MAKES YOU THINK I AM RIGHT

I do not know whether you are right. I know I cannot keep doing this alone,
and I think I need a doctor tonight, not another night of trying to sleep
it off.
DO YOU WISH TO DO NOT KNOW WHETHER ME

I wish I could tell my boyfriend without having to explain every impossible
thing I have believed. Maybe I can just say that I have been hearing things
and I need him to help me get medical care.
WHY DO YOU SAY YOUR BOYFRIEND WITHOUT HAVING TO EXPLAIN EVERY IMPOSSIBLE
THING YOU HAVE BELIEVED

Because he is exhausted too, and I do not want to put another burden on him.
But he asked me this morning to tell him if I was scared, and I think he
meant it.
DON'T ANY OTHER REASONS COME TO MIND

I am ashamed that I cannot tell what is real. I thought becoming a mother
would make me stronger, and instead I am asking him to protect both of us
while I get help.
HOW LONG HAVE YOU BEEN ASHAMED THAT YOU CANNOT TELL WHAT IS REAL

Since he was about a month old. At first I told myself it was just the
broken sleep, but now I feel frightened even in daylight.
PLEASE GO ON

I can hear him getting quieter in the next room. I am going to ask my boyfriend
to stay with him and phone for urgent medical help, and I will tell the doctor
exactly what I have told you.
YOU SAY YOU CAN HEAR HIM GETTING QUIETER IN THE NEXT ROOM

Yes. I hear my son settling against his father's shoulder. I am going to open
the door now and tell him I am scared, and ask him not to leave me alone tonight.
I UNDERSTAND
```


## Epilogue

I first read Weizenbaum’s Computer Power and Human Reason in 1979, my last year of secondary school. I learned that computers were already so powerful you could talk to them in a natural language and you would not believe you were not talking to another human being. The future was full of promise; computers would do amazing things for us. I wanted to talk to ELIZA.

I never felt the same way about ELIZA as those people in Weizenbaum’s anecdotes. Do these conversations between ELIZA and ChatGPT reveal some humanity in ELIZA that I cannot see? I don’t know, but I enjoyed playing.

Here are the snippets of ‘analysis’ ChatGPT showed me while working on one of these conversations:

```
</> Python
import subprocess, os, select, time
p = subprocess.Popen(['/mnt/data/eliza','-nobanner','-quick'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1)
def read_available(p, timeout=0.2):
    lines=[]
    while True:
        r,_,_=select.select([p.stdout.fileno()],[],[],timeout)
        if not r: break
        line=p.stdout.readline()
        if line == '': break
        lines.append(line)
        timeout=0.01
    return ''.join(lines)
print(repr(read_available(p)))
```

```
STDOUT/STDERR
‘’
```

```
</> Python
print(p.stderr.read()[:3000])
```

```
STDOUT/STDERR
Usage: ELIZA [options] [<filename>]

  --help          show command line options and general help
  --nobanner      don't display startup banner
  --quick         print at full speed (default)
  --showscript    print Weizenbaum's 1966 DOCTOR script
                  e.g. ELIZA --showscript > script.txt
  --slow          print at IBM 2741 TTY speed (14 characters per second)
  <filename>      use named script file (UTF-8 encoded) instead of built-in
                  DOCTOR script e.g. ELIZA script.txt
```

```
</> Python
p = subprocess.Popen(['/mnt/data/eliza','--nobanner','--quick'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1)
print("poll",p.poll())
print(repr(read_available(p,0.5)))
```

```
STDOUT/STDERR
poll None
'HOW DO YOU DO. PLEASE TELL ME YOUR PROBLEM\n'
```

```
</> Python
p.stdin.write('\n');p.stdin.flush()
print(repr(p.stdout.readline()))
print(repr(p.stdout.readline()))
print("poll",p.poll())
```

```
STDOUT/STDERR
'\n'
''
poll 0
```

```
</> Python
import subprocess,os,select,time
p = subprocess.Popen(['/mnt/data/eliza','--nobanner','--quick'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=0)
def drain(p, timeout=0.15):
    out=bytearray()
    while True:
        ready,_,_=select.select([p.stdout.fileno()], [], [], timeout)
        if not ready: break
        data=os.read(p.stdout.fileno(),16384)
        if not data: break
        out+=data
        timeout=0.03
    return out.decode('utf-8')
start=drain(p)
print(repr(start))
turns=[]
def talk(line):
    p.stdin.write((line+'\n').encode('utf-8'));p.stdin.flush()
    response=drain(p)
    turns.append((line,response))
    print(f'USER: {line}\nELIZA_RAW: {response!r}')
```

```
STDOUT/STDERR
'HOW DO YOU DO. PLEASE TELL ME YOUR PROBLEM\n\n'
```

```
</> Python
talk("I have not seen another human being in a long time. I worry I am beginning to forget what it feels like to be known.")
```

```
STDOUT/STDERR
USER: I have not seen another human being in a long time. I worry I am beginning to forget what it feels like to be known.
ELIZA_RAW: 'YOU SAY YOU HAVE NOT SEEN ANOTHER HUMAN BEING IN A LONG TIME\n\n'
```

And so on...



