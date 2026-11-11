// The people on the street you can talk to, and Moheeb and Luis on the Chupacabra patio.
// tools/script/street_compile.mjs turns this into src/gen/street_data.c and source/street_lines.json;
// tools/bake_tts.py voices it. src/street.c runs it. None of it is needed to finish the day: a talk is
// something you can have in a roam, never something a roam waits on.
//
// ADD A STRANGER: an entry in STRANGERS.
//   id     lower case, unique: their memory and their voice key (st_<id>)
//   name   the name on the dialogue box ("Barista on a break"), 18 characters at most; strangers do not give names
//   body   one or more background bodies they can wear: H2 I2 J2 K2 L2 M2 N2 O2 P2 (adults) and E (a child).
//          See actor.h for what each looks like; the first one cast keeps them for the day.
//   look   0 or 1: which of the body's two looks (story.c's extras region holds both palettes)
//   where  'street' (Main and its sidewalks), 'inside' (a room you walk into), 'lab' (members at the benches)
//   hours  [from, to] on the lab clock, when they are out
//   voice, speed  a Kokoro voice (tools/script/cast.mjs lists the ones the cast uses; pick by the body)
//   color  the name's colour on the dialogue box
//   talks  TALK(when, beats) list, see below
//
// ADD A CONVERSATION: a TALK(when, beats) in a stranger's talks (or in CAMEO.talks).
//   when   { hour: [from, to], met: true (only once you have talked before) or false (only a first meeting),
//            flag: 'story_flag' or '!story_flag' (FLAG_NAMES in src/gen/script.c, set by day.mjs),
//            mark: 'name' or '!name' (a street memory a MARK set, any stranger's),
//            stat: ['vibe', 8] (at least), low: ['energy', 3] (at most), venue: 'proof' (the room you are in),
//            again: true (it can be had again; one of these is what they say once the rest are used) }
//          The first talk in the list that fits and has not been had today is the one they have.
//   beats  L(who, text): who is 'them' (the stranger), 'me' (you), or in the cameo 'moheeb' and 'luis'.
//            text is a string, or { vee, dex, ruth, any } by who is playing; {me} is your name.
//          CHOICE(prompt, [O(text, beats, { say }), A(text, beats)]): O is said aloud (say: other words than
//            the menu's), A is an action, never said.
//          ADD('vibe', 1): a small reward (strangers give at most a few a day). MARK('name'): remembered.
//          NOTE('text'): a line on screen, no voice.
// Then: node tools/script/street_compile.mjs, and bake the voices (HANDOFF.md).
//
// Writing: full sentences, each person their own voice, about downtown Mesa, the lab and what is on.
// Fictional people only. HeatSync facts: open to the public, membership is a donation, joining needs no
// vouch, only 24/7 card access needs a nomination. Arduino Night is the first Wednesday, seven to ten.
// No emdashes, no double dashes, no emoji.

export const TALK = (when, beats) => ({ when, beats });
export const L = (who, text, o = {}) => ({ op: 'say', who, text, ...o });
export const CHOICE = (prompt, options) => ({ op: 'choice', prompt, options });
export const O = (text, beats = [], o = {}) => ({ text, beats, ...o });
export const A = (text, beats = [], o = {}) => ({ text, beats, ...o, act: true });
export const ADD = (stat, n = 1) => ({ op: 'add', stat, n });
export const MARK = (name) => ({ op: 'mark', name });
export const NOTE = (text) => ({ op: 'note', text });
const CHAT = (when, lines) => ({ when, lines });

const LUNCH = [11, 15], EVENING = [16.5, 21], NIGHT = [21, 25], DAY = [9, 19];

export const STRANGERS = [
  // ---------------------------------------------------------------- Main Street
  {
    id: 'commuter', name: 'Rail commuter', body: ['H2'], look: 0, where: 'street', hours: [9, 25],
    voice: 'bm_lewis', speed: 1.02, color: 0x8fb8d8,
    talks: [
      TALK({ hour: LUNCH }, [
        L('them', "The sign at the station says four minutes. It has said four minutes for ten minutes."),
        L('me', "Listen for the bell at Macdonald. Then you have a minute."),
        L('them', "A bell. That's better than the sign. The bell doesn't lie to me."),
      ]),
      TALK({ hour: [13.2, 16] }, [
        L('them', "Were you the one out here with the sword? Over a bicycle? With a bag of bread?"),
        CHOICE('He saw the lunch fight.', [
          O("It was the lab's bike. We got it back.", [L('them', "Good. People should leave bikes alone. I say that as a man who has lost three.")],
            { say: "It was the lab's bike. We got it back." }),
          A("Shrug. Change the subject.", [L('them', "Fair. I didn't see anything. I was reading my phone. Like a citizen.")]),
        ]),
      ]),
      TALK({ hour: EVENING }, [
        L('them', "You're still in your lanyard. Long shift?"),
        L('me', { vee: "Nine to midnight. I'm about halfway, I think.", dex: "Nine to midnight. I've stopped checking the clock.", ruth: "Nine until midnight. The clock and I aren't speaking." }),
        L('them', "I do eight hours at a desk and I feel like a raisin. Go easy on yourself."),
      ]),
      TALK({ hour: NIGHT }, [
        L('them', "Last eastbound's soon. I always end up running for it, and I always make it, and I never learn."),
        L('them', "Your lab still has the lights on. Does anybody over there ever go home?"),
        L('me', "Eventually. Somebody has to lock up, and tonight that's me."),
      ]),
      TALK({ again: true }, [L('them', "Still waiting on the train. Me and the pigeons. Have a good one.")]),
    ],
  },
  {
    id: 'retiree', name: 'Retired bus driver', body: ['L2'], look: 0, where: 'street', hours: [9, 20],
    voice: 'am_echo', speed: 0.92, color: 0xc8b890,
    talks: [
      TALK({ met: false }, [
        L('them', "I drove a bus down this street for twenty two years. Before the train. Before the trees, even."),
        L('them', "Now there's a place that lets people use a laser on purpose. What exactly goes on in there?"),
        CHOICE('He nods at the lab.', [
          O("Come in and see. It's open to the public.", [
            L('them', "Just walk in? Nobody checks if I belong?"),
            L('me', "If the lab's open, you belong. We'll show you around."),
            L('them', "Huh. I might. My grandson thinks I can't work a printer."),
            ADD('trust', 1), MARK('invited_retiree'),
          ], { say: "Come in and see. It's open to the public." }),
          O("People build things. Furniture, robots, costumes.", [
            L('them', "Costumes. Well. There's a man over there in a cape every Wednesday. That explains it."),
          ], { say: "People build things. Furniture, robots, costumes." }),
        ]),
      ]),
      TALK({ met: true, mark: 'invited_retiree' }, [
        L('them', "I walked past your door twice today. Third time, I'm going in."),
        L('me', "Ask for the lathe. Somebody will light up."),
      ]),
      TALK({ hour: [13.2, 20] }, [
        L('them', "Saw some commotion out front at lunch. In my day, we settled things with a strongly worded letter."),
        L('me', "We don't settle anything by sword. That was about a bike."),
        L('them', "That's what they all say."),
      ]),
      TALK({ again: true }, [L('them', "Don't let the heat get you. Drink some water. A bus driver's advice, free.")]),
    ],
  },
  {
    id: 'student', name: 'College student', body: ['K2'], look: 0, where: 'street', hours: [11, 23],
    voice: 'bf_lily', speed: 1.06, color: 0xf0a8c8,
    talks: [
      TALK({ met: false }, [
        L('them', "Sorry, is that a HeatSync lanyard? I have a studio project due Friday and I need a laser cutter."),
        L('them', "Do I need to know somebody? Like, does someone have to vouch for me?"),
        CHOICE('She looks like she has not slept since Monday.', [
          O("No vouch. Come in during open hours.", [
            L('me', "Joining is a donation and one form. Nobody approves it."),
            L('them', "Wait. That's it? Our school shop has a three page waiver and a guy named Doug."),
            L('me', "We have a waiver too. And a laser checkout first."),
            ADD('trust', 1), MARK('helped_student'),
          ], { say: "No vouch needed. Come in during open hours." }),
          O("Come tonight. Arduino Night is at seven.", [
            L('them', "I don't know anything about Arduino."),
            L('me', "Perfect. No experience needed. That's the whole point of it."),
            ADD('vibe', 1), MARK('invited_student'),
          ], { say: "Come tonight. Arduino Night is at seven." }),
        ]),
      ]),
      TALK({ met: true, hour: [18.5, 25], mark: 'invited_student' }, [
        L('them', "I came by! Well. I stood outside and watched someone blink an LED for ten minutes."),
        L('them', "It was weirdly beautiful. I'm going in next time."),
      ]),
      TALK({ met: true }, [
        L('them', "I asked my professor about the laser. She said she's a member too. Small world. Small, laser cut world."),
      ]),
      TALK({ hour: [21.5, 25] }, [
        L('them', "Is it true there's a twenty four hour card? I'm a night person. Like, aggressively."),
        L('me', "That one needs a nomination. Come around. People will know you."),
        L('them', "So I have to be a regular first. Okay. I can be a regular. I'm a regular at three cafes."),
      ]),
      TALK({ again: true }, [L('them', "Friday. The project is due Friday. Wish me luck.")]),
    ],
  },
  {
    id: 'barista', name: 'Barista on a break', body: ['I2'], look: 0, where: 'street', hours: [9, 18],
    voice: 'af_river', speed: 0.98, color: 0xd8a070,
    talks: [
      TALK({ hour: [9, 13.2] }, [
        L('them', "You're from the lab. I can tell. You all order cold brew like it's a structural material."),
        L('me', "Sometimes it is. A cup once shimmed a print bed."),
        L('them', "I'm going to pretend you didn't tell me that."),
      ]),
      TALK({ hour: [13.2, 18] }, [
        L('them', "Was that you at lunch? Swords, a bike, and a sack of sandwiches? The whole patio stopped talking."),
        CHOICE('She is grinning.', [
          O("Just a normal Wednesday.", [L('them', "If that's normal, I want to see a weird one.")], { say: "Just a normal Wednesday." }),
          O("We don't usually do that. Truly.", [L('them', "Sure. And I don't usually give free refills. Here we are."), ADD('vibe', 1)],
            { say: "We don't usually do that. Truly." }),
        ]),
      ]),
      TALK({ met: true }, [
        L('them', "One of your people comes in every morning and draws circuits on the napkins. We keep the good ones."),
      ]),
      TALK({ again: true }, [L('them', "Break's over in five. The milk steamer waits for no one.")]),
    ],
  },
  {
    id: 'crew', name: 'City crew', body: ['N2'], look: 0, where: 'street', hours: [9, 17],
    voice: 'am_fenrir', speed: 0.95, color: 0xe0c040,
    talks: [
      TALK({}, [
        L('them', "This lamp post has been blinking since Tuesday. I've been up and down it four times."),
        L('them', "It's a hundred and four degrees and I'm in long sleeves. Don't go into public works, kid."),
        CHOICE('He mops his forehead.', [
          O("Want a cold soda from the lab fridge?", [
            L('them', "You'd do that? Yeah. That's nice of you. Just one. I'm on the clock."),
            ADD('vibe', 1), MARK('soda_crew'),
          ], { say: "Want a cold soda from the lab fridge?" }),
          O("Sounds like a loose neutral.", [
            L('them', "That's what I said. My boss said it's the photocell. We have a bet."),
          ], { say: "Sounds like a loose neutral." }),
        ]),
      ]),
      TALK({ met: true, mark: 'soda_crew' }, [
        L('them', "That soda saved my afternoon. And it was the neutral. I won twenty bucks."),
      ]),
      TALK({ met: true }, [
        L('them', "My kid's got a remote control car with a busted motor. You people fix stuff like that?"),
        L('me', "We'll show you how. Bring the car and the kid."),
        L('them', "Huh. Teach a man to fix a car. I like that."),
      ]),
      TALK({ again: true }, [L('them', "Back up the pole. Wish me luck.")]),
    ],
  },
  {
    id: 'usher', name: 'Arts Center usher', body: ['M2'], look: 0, where: 'street', hours: [16, 23],
    voice: 'af_sarah', speed: 0.95, color: 0xd07080,
    talks: [
      TALK({ met: false }, [
        L('them', "I usher at the Arts Center on show nights. I've walked past your window for years and never asked."),
        L('them', "What is a hackerspace, exactly? My nephew says it's a gym for nerds."),
        CHOICE('She has her usher badge on already.', [
          O("It's a shared workshop people run together.", [
            L('them', "Like a co-op. With saws."),
            L('me', "With saws, and people who want to show you how."),
            ADD('trust', 1),
          ], { say: "A shared workshop the members run together." }),
          O("Your nephew isn't wrong.", [
            L('them', "He usually isn't. Don't tell him I said so."),
          ], { say: "Your nephew isn't wrong." }),
        ]),
      ]),
      TALK({ hour: [19, 23] }, [
        L('them', "Intermission. I came out for air. The show is a musical about a lighthouse. Everyone cried."),
        L('them', "Including the lighthouse, I think. It had a very good monologue."),
      ]),
      TALK({ again: true }, [L('them', "If you ever want to see the show, find me at the side door. I'll find you a seat.")]),
    ],
  },
  {
    id: 'tourist', name: 'Out of towner', body: ['P2'], look: 0, where: 'street', hours: [11, 23],
    voice: 'am_liam', speed: 1.0, color: 0x70c070,
    talks: [
      TALK({ hour: [11, 16.5] }, [
        L('them', "Hi. I'm visiting my sister, and she's at work, so I'm doing what she called a downtown walk."),
        L('them', "Which so far has been a bakery line and a train that rang its bell at me. Is there more?"),
        CHOICE('He has a paper map.', [
          O("Come see the lab. We do tours.", [
            L('them', "A tour! Of what?"),
            L('me', "Lasers, printers, a woodshop, and somebody's robot that only drives left."),
            L('them', "Sold. I love a robot with a flaw."),
            ADD('vibe', 1),
          ], { say: "Come see the lab. We do tours." }),
          O("Walk to the Arts Center. It's worth it.", [
            L('them', "East? That way? Okay. I trust a person with a clipboard."),
          ], { say: "Walk east to the Arts Center. It's worth it." }),
        ]),
      ]),
      TALK({ hour: [16.5, 25] }, [
        L('them', "Hey, you're from the lab, right? Two guys on the Chupacabra patio talked my ear off about it."),
        L('them', "One had a laptop. One had a beer. Both had opinions about solder."),
        L('them', "They said if I wanted the real downtown, I should find the hackerspace. So, here I am."),
        MARK('heard_patio'),
        NOTE("The Chupacabra Taproom is on Robson, just north of Main."),
      ]),
      TALK({ again: true }, [L('them', "My sister is not going to believe the robot. I'm going to tell her anyway.")]),
    ],
  },
  {
    id: 'busker', name: 'Guitar player', body: ['J2'], look: 0, where: 'street', hours: [16, 25],
    voice: 'am_eric', speed: 0.96, color: 0xb090e0,
    talks: [
      TALK({}, [
        L('them', "I play outside the Nile on show nights. My guitar's in the car. Tonight I'm scouting the good corner."),
        L('them', "The good corner has shade, foot traffic, and nobody's speaker next door playing the same song."),
        CHOICE('He is very serious about the corner.', [
          O("We've got a busted amp you could fix up.", [
            L('them', "You'd let me? I'd need to know a soldering iron from a curling iron."),
            L('me', "Somebody will show you. Bring it to Arduino Night, seven tonight."),
            ADD('vibe', 1), MARK('busker_amp'),
          ], { say: "We've got a busted amp you could fix up." }),
          A("Point at the corner by the lab.", [
            L('them', "In front of the hackerspace? Do you get foot traffic, or just people carrying plywood?"),
          ]),
        ]),
      ]),
      TALK({ met: true, mark: 'busker_amp' }, [
        L('them', "I keep thinking about that amp. If I build my own pedal, am I a musician or an engineer?"),
        L('me', "Both. That's the good news."),
      ]),
      TALK({ again: true }, [L('them', "Corner's still free. I'm going to stand on it a while, so it knows it's mine.")]),
    ],
  },
  {
    id: 'neighbor', name: 'Upstairs neighbor', body: ['O2'], look: 0, where: 'street', hours: [11, 24],
    voice: 'af_aoede', speed: 0.98, color: 0x90b0d0,
    talks: [
      TALK({ met: false }, [
        L('them', "I live behind you, on the third floor. I work from home, so I hear your shop vac at ten every morning."),
        L('them', "I'm not complaining. It's like a rooster. A rooster with a filter bag."),
        CHOICE('She means it kindly.', [
          O("You could work from our coworking tables.", [
            L('them', "Is that allowed? I'm not a member."),
            L('me', "Anyone can come in. If you stay, membership is a donation."),
            L('them', "A change of scenery. With a rooster. I'll think about it."),
            ADD('trust', 1),
          ], { say: "You could work from our coworking tables." }),
          O("Sorry about the shop vac.", [L('them', "Don't be. It means somebody cleaned up. That's rare in a building.")],
            { say: "Sorry about the shop vac." }),
        ]),
      ]),
      TALK({ flag: 'gloria_quiet' }, [
        L('them', "Gloria from my floor says you're posting quiet hours. She told the whole elevator."),
        L('them', "She's been rehearsing that speech for a month. You took it from her. She seems happy anyway."),
      ]),
      TALK({ again: true }, [L('them', "Ten o'clock tomorrow. The rooster. I'll be listening.")]),
    ],
  },
  {
    id: 'courier', name: 'Bike courier', body: ['P2'], look: 1, where: 'street', hours: [11, 20],
    voice: 'bm_daniel', speed: 1.04, color: 0xe08040,
    talks: [
      TALK({ hour: [13.2, 20] }, [
        L('them', "That orange loaner by your door is yours, right? I saw two guys try to walk it off at lunch."),
        L('them', "I was going to step in. Then you stepped in. With a sword. So I stepped back out."),
        L('me', "It's a shared bike. It goes on the rack, not in a van."),
        L('them', "Respect. Put a better lock on it, though. I can recommend one. I've lost four bikes on this street."),
      ]),
      TALK({}, [
        L('them', "I do forty miles a day on Main and Broadway. You know what I want? A bike repair stand that's in the shade."),
        L('me', "We have one inside. It's in the shade, because it's indoors."),
        L('them', "Don't tempt me. I'll move in."),
      ]),
      TALK({ again: true }, [L('them', "Got a drop at Center and Main. Later.")]),
    ],
  },
  {
    id: 'teen', name: 'Skateboarder', body: ['K2'], look: 1, where: 'street', hours: [12, 21],
    voice: 'af_nicole', speed: 1.12, color: 0x60d0e0,
    talks: [
      TALK({ flag: 'beat_champ' }, [
        L('them', "Wait. Wait wait wait. Are you the one who beat High Score at Level 1 today?"),
        L('them', "Everybody at the arcade is losing their minds. She's never lost on that cabinet."),
        CHOICE('The teen is starstruck.', [
          O("It was a close game.", [L('them', "That is the coolest thing a grown up has ever said to me.")], { say: "It was a close game." }),
          O("Come learn to build a cabinet at the lab.", [
            L('them', "You can build an arcade cabinet? Like, from wood? Like, legally?"),
            L('me', "From wood, with help, legally. Bring a parent the first time."),
            ADD('vibe', 1),
          ], { say: "Come learn to build a cabinet at the lab." }),
        ]),
      ]),
      TALK({}, [
        L('them', "My board's got a bad wheel. It sounds like a shopping cart. Everybody can hear me coming."),
        L('me', "Swap the bearings. We have a press for it at the lab."),
        L('them', "A bearing press. That sounds so official. Okay. Maybe."),
      ]),
      TALK({ again: true }, [L('them', "Gotta go. Arcade closes at ten and I'm saving up for a high score of my own.")]),
    ],
  },
  {
    id: 'cook', name: 'Line cook', body: ['N2'], look: 1, where: 'street', hours: [17, 25],
    voice: 'am_onyx', speed: 1.02, color: 0xe06050,
    talks: [
      TALK({}, [
        L('them', "Ten minute break. The kitchen is a hundred and twenty degrees, so out here feels like a fridge."),
        L('them', "My knife needs sharpening and our sharpening guy retired. You people have a grinder?"),
        CHOICE('He is holding a soda like it is the last one on earth.', [
          O("Bring it in. Someone will show you how.", [
            L('them', "Show me how. Not do it for me. I like that. I'm going to be insufferable about it."),
            ADD('trust', 1),
          ], { say: "Bring it in. Someone will show you how." }),
          O("Don't learn on your good knife.", [L('them', "Wise words. I'll bring the bad knife. I have a lot of bad knives.")],
            { say: "Don't learn on your good knife." }),
        ]),
      ]),
      TALK({ hour: NIGHT }, [
        L('them', "Kitchen's closing. I'm going to eat a whole sleeve of crackers on the train and nobody can stop me."),
      ]),
      TALK({ again: true }, [L('them', "Back to the line. Order up, as they say. They do say it. Constantly.")]),
    ],
  },
  {
    id: 'owl', name: 'Night walker', body: ['H2'], look: 1, where: 'street', hours: [20.5, 25],
    voice: 'am_puck', speed: 0.98, color: 0x8080d0,
    talks: [
      TALK({}, [
        L('them', "I like Main this late. The train's quieter, the lights hum. It feels like the street is thinking."),
        L('them', "Your lab is the only window still lit on this block. Somebody inside is working on something."),
        CHOICE('He looks up at the lab sign.', [
          O("You're welcome to look in. We're open a bit longer.", [
            L('them', "Not tonight. But it's nice to know the light is on. Somebody's always building something in there."),
            ADD('vibe', 1),
          ], { say: "You're welcome to look in. We're open a bit longer." }),
          O("Mostly it's me, turning things off.", [L('them', "Somebody has to be the last one. That's its own kind of work.")],
            { say: "Mostly it's me, turning things off." }),
        ]),
      ]),
      TALK({ flag: 'sam_joins' }, [
        L('them', "Saw you walking someone to the train. She was talking about cardboard the whole way. Very happy about it."),
      ]),
      TALK({ again: true }, [L('them', "Good night, keyholder. Lock the door twice. Everybody forgets once.")]),
    ],
  },

  // ---------------------------------------------------------------- inside: the rooms you walk into
  {
    id: 'counter', name: 'Behind the counter', body: ['I2', 'O2'], look: 1, where: 'inside', hours: [0, 99],
    voice: 'af_jessica', speed: 0.98, color: 0xe0b060,
    talks: [
      TALK({ venue: 'proof', hour: [11, 14] }, [
        L('them', "You're here for the lab order? Fourteen sandwiches. One of them just says surprise."),
        L('them', "We don't know who ordered surprise. The baker made something. It's the baker's secret."),
        L('me', "Somebody at the lab is going to be very happy, or very confused."),
      ]),
      TALK({ venue: 'proof' }, [
        L('them', "We get a lot of your people in here. You can tell them by the sawdust on their shoes."),
        L('them', "And the questions about how the ovens work. One of them asked for a schematic."),
      ]),
      TALK({ venue: 'nile' }, [
        L('them', "There's a show tonight, so it'll be packed down here in an hour. Grab a coffee now, while you can."),
      ]),
      TALK({ venue: 'level1' }, [
        L('them', "Tokens are at the machine. The claw game is fair. I'm required to say that."),
      ]),
      TALK({ met: false }, [
        L('them', "Welcome in. Take your time. You look like you've been on your feet since this morning."),
        L('me', "Nine o'clock, and I'm hosting the hackerspace till midnight."),
        L('them', "Then sit for a minute. Nobody's timing you in here."),
      ]),
      TALK({ again: true }, [L('them', "Anything else? No rush. Seriously, sit down.")]),
    ],
  },
  {
    id: 'patron', name: 'Regular', body: ['L2', 'M2', 'J2'], look: 1, where: 'inside', hours: [0, 99],
    voice: 'bm_george', speed: 0.96, color: 0xa0c0a0,
    talks: [
      TALK({}, [
        L('them', "I've had this same seat every week for six years. They put a little plaque on it. Well, a sticky note."),
        L('them', "You're from the hackerspace, aren't you? Could you people make me a real plaque?"),
        CHOICE('He taps the sticky note.', [
          O("Come in and engrave it yourself.", [
            L('them', "On the laser? Me? I can't program a microwave."),
            L('me', "We'll walk you through it. It's mostly picking a font."),
            ADD('trust', 1),
          ], { say: "Come in and engrave it yourself." }),
          A("Admire the sticky note.", [L('them', "It's a good note. It's been through a lot.")]),
        ]),
      ]),
      TALK({ again: true }, [L('them', "Same seat next week. You'll know where to find me.")]),
    ],
  },

  // ---------------------------------------------------------------- the lab: members at the benches
  {
    id: 'm_cowork', name: 'Coworking member', body: ['H2'], look: 0, where: 'lab', hours: [9, 19],
    voice: 'bm_fable', speed: 1.0, color: 0x9ab0e0,
    talks: [
      TALK({ hour: [9, 12] }, [
        L('them', "Morning, keyholder. I'm writing firmware, which means I'm mostly staring at a wall and sighing."),
        L('me', "Want a coffee for the sighing?"),
        L('them', "No. The sighing is part of the process. The coffee comes after the first bug."),
      ]),
      TALK({}, [
        L('them', "The tables are loud today. I love it. At home it's me, a cat, and a compiler that hates me."),
      ]),
      TALK({ again: true }, [L('them', "Back to it. Sixty three warnings. Sixty two. Progress.")]),
    ],
  },
  {
    id: 'm_jewelry', name: 'Jewelry maker', body: ['I2'], look: 0, where: 'lab', hours: [9, 19],
    voice: 'af_kore', speed: 1.02, color: 0xe0c070,
    talks: [
      TALK({}, [
        L('them', "I'm setting a stone the size of a sesame seed. If you sneeze, we're both looking for it on the floor."),
        CHOICE('She does not look up.', [
          A("Hold very still.", [L('them', "Thank you. Got it. You can breathe now. That was a team effort."), ADD('vibe', 1)]),
          O("Need the loupe from the drawer?", [L('them', "I'm wearing two already. I look like a beetle. A focused beetle.")],
            { say: "Need the loupe from the drawer?" }),
        ]),
      ]),
      TALK({ again: true }, [L('them', "Ring's nearly done. I'll show you when it's set. Don't sneeze.")]),
    ],
  },
  {
    id: 'm_fixer', name: 'Bench fixer', body: ['L2'], look: 0, where: 'lab', hours: [9, 19],
    voice: 'am_santa', speed: 1.0, color: 0xc0a080,
    talks: [
      TALK({}, [
        L('them', "Somebody leveled this bench with a coaster and a dream. I'm fixing it properly. With shims and a grudge."),
        L('me', "Leave a note on it when you're done, so it stays fixed."),
        L('them', "I'll leave three. One will survive."),
      ]),
      TALK({ again: true }, [L('them', "Almost level. Almost is a whole profession, you know.")]),
    ],
  },
  {
    id: 'm_printer', name: 'Printer regular', body: ['O2'], look: 0, where: 'lab', hours: [9, 19],
    voice: 'af_nova', speed: 1.0, color: 0x80d0b0,
    talks: [
      TALK({}, [
        L('them', "Third print of the same bracket. The first one warped, the second one I printed at the wrong scale."),
        L('them', "It was very small. It was adorable. It was useless."),
        CHOICE('She holds up a bracket the size of a fingernail.', [
          O("Keep it. Put it on a keychain.", [L('them', "A souvenir of my shame. Yes. Absolutely.")], { say: "Keep it. Put it on a keychain." }),
          O("Did you level the bed today?", [L('them', "I did not. I'm going to go do that now, and we will never speak of this."), ADD('safety', 1)],
            { say: "Did you level the bed today?" }),
        ]),
      ]),
      TALK({ again: true }, [L('them', "Print four. This is the one. I can feel it.")]),
    ],
  },
  {
    id: 'm_elec', name: 'Electronics member', body: ['J2'], look: 0, where: 'lab', hours: [9, 19],
    voice: 'am_adam', speed: 1.04, color: 0x70b0f0,
    talks: [
      TALK({}, [
        L('them', "The scope says my signal is clean. My circuit says otherwise. One of them is lying."),
        L('me', "Check your ground. It's always the ground."),
        L('them', "It is always the ground. Why is it always the ground? Don't answer. I know why."),
      ]),
      TALK({ again: true }, [L('them', "It was the ground. Don't say anything.")]),
    ],
  },
  {
    id: 'm_lathe', name: 'Woman at the lathe', body: ['M2'], look: 0, where: 'lab', hours: [9, 19],
    voice: 'bf_isabella', speed: 0.96, color: 0xd0a0d0,
    talks: [
      TALK({}, [
        L('them', "I'm turning a handle for my grandmother's old kettle. The original cracked in nineteen ninety one."),
        L('them', "She's been holding it with an oven mitt ever since. Thirty years of oven mitt."),
        L('me', "Safety glasses on, and keep your sleeves clear of the chuck."),
        L('them', "Always. You sound like the woman who checked me out on this. Good. She was right."),
      ]),
      TALK({ again: true }, [L('them', "One more pass. Then it's a handle. Then it's a kettle again.")]),
    ],
  },
  {
    id: 'a_first', name: 'First timer', body: ['K2', 'I2'], look: 1, where: 'lab', hours: [19, 23],
    voice: 'af_sky', speed: 1.06, color: 0xf0c0a0,
    talks: [
      TALK({}, [
        L('them', "This is my first Arduino Night. My LED is blinking. I didn't think it would actually blink."),
        CHOICE('She is holding a breadboard like a baby bird.', [
          O("Now make it blink faster.", [
            L('them', "Change the number? This number? Oh. Oh, it's frantic now. I did that."),
            ADD('vibe', 1),
          ], { say: "Now make it blink faster." }),
          O("Welcome. Come back next month.", [L('them', "Next month I want two LEDs. I'm getting ambitious.")],
            { say: "Welcome. Come back next month." }),
        ]),
      ]),
      TALK({ again: true }, [L('them', "It's still blinking. I can't stop watching it.")]),
    ],
  },
  {
    id: 'a_parent', name: 'Arduino Night dad', body: ['N2', 'P2'], look: 1, where: 'lab', hours: [19, 23],
    voice: 'am_liam', speed: 0.98, color: 0xb0d080,
    talks: [
      TALK({}, [
        L('them', "My daughter dragged me here. Now she's showing me how a servo works. I'm the student tonight."),
        L('me', "That's the best kind of Arduino Night."),
        L('them', "She says I hold the screwdriver wrong. She's nine. She's right."),
      ]),
      TALK({ again: true }, [L('them', "She wants to come back. So I guess I'm coming back too. I'm fine with it.")]),
    ],
  },
  {
    id: 'a_kid', name: 'Kid with a servo', body: ['E'], look: 1, where: 'lab', hours: [19, 22],
    voice: 'af_alloy', speed: 1.12, color: 0xf0e070,
    talks: [
      TALK({}, [
        L('them', "Look! It turns when I turn the knob! I made it a robot arm. It's a very small arm."),
        L('them', "It can hold one pretzel. That's its whole job."),
        CHOICE('The servo waves a pretzel at you.', [
          O("That's a great job for an arm.", [L('them', "I know! Next I'm making a second arm. For a second pretzel."), ADD('vibe', 1)],
            { say: "That's a great job for an arm." }),
          O("Show your dad how the knob works.", [L('them', "I did. Twice. He's slow, but he's trying.")], { say: "Show your dad how the knob works." }),
        ]),
      ]),
      TALK({ again: true }, [L('them', "Pretzel delivery! Beep!")]),
    ],
  },
];

// ---------------------------------------------------------------- the Chupacabra patio
// Moheeb and Luis, at a table outside the taproom from late afternoon: Moheeb on his laptop, Luis with a
// beer. They made a game called Coffee and Kill. Their voices are the Kyutai clones from SMALL PARTS
// (tools/bake_tts.py, KYUTAI); voice and speed below are only the Kokoro stand ins if that is not set up.
// chatter: what they say to each other when you are near (one exchange at a time, each once a day).
// talks: what they say to you (a "Talk" at the table).
export const CAMEO = {
  hours: [16.5, 25],
  people: {
    moheeb: { name: 'Moheeb', color: 0x7ec8e3, voice: 'am_michael', speed: 1.0 },
    luis: { name: 'Luis', color: 0xf0a050, voice: 'am_adam', speed: 1.04 },
  },
  chatter: [
    CHAT({}, [
      ['luis', "You've been typing for an hour. Is it the game, or is it email?"],
      ['moheeb', "It's the game. The emails are in another tab, being ignored with dignity."],
    ]),
    CHAT({}, [
      ['moheeb', "I think the enemies in Coffee and Kill need a coffee break animation."],
      ['luis', "They're made of coffee. That's a little dark."],
      ['moheeb', "Dark roast."],
      ['luis', "I'm leaving."],
    ]),
    CHAT({}, [
      ['luis', "The lab's lights are still on."],
      ['moheeb', "They're always on. Somebody is printing a hinge."],
      ['luis', "Somebody is always printing a hinge. It's the hinge that never ends."],
    ]),
    CHAT({}, [
      ['moheeb', "I ordered a microcontroller that's smaller than the stamp on the box it came in."],
      ['luis', "That's how they get you. The box is the real product."],
    ]),
    CHAT({}, [
      ['luis', "Remember when the scariest thing at the lab was the soldering iron?"],
      ['moheeb', "Then the laser cutter showed up, and everybody had a cardboard phase."],
      ['luis', "Some of us never left the cardboard phase."],
    ]),
    CHAT({}, [
      ['moheeb', "Every time I sit at this table, I end up drawing a circuit board on a napkin."],
      ['luis', "Mine's on the coaster. It's my best work. It has a cup ring through the ground plane."],
    ]),
    CHAT({}, [
      ['luis', "Downtown's busy for a Wednesday."],
      ['moheeb', "It's the train. The train brings everybody."],
      ['luis', "And then it takes them all home again. That's the dream."],
    ]),
    CHAT({ hour: [19, 25] }, [
      ['moheeb', "This is my third coffee."],
      ['luis', "It's eight o'clock at night."],
      ['moheeb', "Then it's a nightcap."],
      ['luis', "That is not what that word means."],
    ]),
    CHAT({}, [
      ['luis', "Somebody at the lab asked me if Coffee and Kill was a cafe."],
      ['moheeb', "We should open one. Espresso and a boss fight."],
      ['luis', "I'm pretty sure the health code says no boss fights."],
    ]),
    CHAT({}, [
      ['moheeb', "The new board boots, but only if I hold it at an angle."],
      ['luis', "That's a cold joint. Or the board has opinions."],
      ['moheeb', "Everything I build has opinions."],
    ]),
    CHAT({ hour: [21, 25] }, [
      ['luis', "We've said one more since six o'clock."],
      ['moheeb', "That's also how every project at the lab works."],
    ]),
    CHAT({ flag: 'beat_champ' }, [
      ['luis', "Did you hear somebody beat High Score at the arcade today?"],
      ['moheeb', "I heard. I'm choosing to believe it was a keyholder."],
    ]),
  ],
  talks: [
    TALK({ met: false, mark: 'heard_patio' }, [
      L('moheeb', "Wait. Did a guy with a paper map send you here?"),
      L('luis', "We told him to find the real downtown. We didn't think he'd send it back to us."),
      L('me', "He said you both had opinions about solder."),
      L('moheeb', "Strong ones. More flux. It's always more flux."),
      L('luis', "Sit for a minute. You look like you've been hosting since nine."),
      ADD('energy', 1),
    ]),
    TALK({ met: false }, [
      L('luis', "Hey, you're hosting today, right? You've got the clipboard face."),
      L('moheeb', "Sit for a minute. Nobody's setting the lab on fire in the next five minutes."),
      L('luis', "Probably."),
      CHOICE('Two people at a patio table, a laptop and a beer between them.', [
        O("How do you two know the lab?", [
          L('moheeb', "We made a game there. Long nights, cold pizza, a whole lot of hot glue."),
          L('luis', "It's called Coffee and Kill. It's exactly what it sounds like."),
          L('moheeb', "Come by sometime when it's slow. We'll show you the build."),
        ], { say: "How do you two know the lab?" }),
        O("I can only stay a minute.", [
          L('luis', "A minute is plenty. That's a whole break, if you do it right."),
          L('moheeb', "Here's the trick. Sit, breathe, then go back and fix whatever's on fire."),
          ADD('energy', 1),
        ], { say: "I can only stay a minute." }),
      ]),
    ]),
    TALK({ flag: 'passed_inspection' }, [
      L('moheeb', "We heard the fire inspector came by. Did the lab pass?"),
      L('me', { vee: "We passed. Every extinguisher tagged. I may have hugged one.", dex: "We passed. She even said the shop looked organized.", ruth: "We passed. I labeled the label maker, just in case." }),
      L('luis', "Of course you did. Cheers to the lab."),
      ADD('vibe', 1),
    ]),
    TALK({ low: ['energy', 3] }, [
      L('luis', "You okay? You look like you've been carrying the whole lab on your back."),
      L('moheeb', "Sit. There's a chair. Hosting isn't a sprint. It's a long walk with a lot of doors."),
      L('me', "Thanks. Just for a minute."),
      ADD('energy', 1),
    ]),
    TALK({ stat: ['vibe', 9] }, [
      L('moheeb', "You look like the day's going well."),
      L('luis', "Or the coffee's going well. Hard to tell from here."),
      L('me', "Both, I think. The lab's buzzing tonight."),
    ]),
    TALK({ met: true }, [
      L('luis', "Back again? Must be quiet at the lab."),
      L('moheeb', "Or loud. Loud is also a good reason to sit out here."),
    ]),
    TALK({ again: true }, [
      L('moheeb', "We'll be here a while. Tell the lab we said hi."),
    ]),
  ],
};
