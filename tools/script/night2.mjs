// KEYHOLDER, night two: Hack Your Hackerspace, a month on. The members' meeting: introductions, station reports,
// issues and proposals, votes, then everybody cleans. Any member can start it; quorum is eight voting members;
// a proposal goes to the group nine days ahead and its author bumps it and brings it.
// Shown, never said: proposals as hypotheses from people who did the work; the committee as a way to say no
// without saying it; who decides (the members, by vote, not whoever has the biggest offer); the one person who
// knows how a job is done, and what happens when she stops; a problem raised the right way.
// Carries in: helped_bex, bex_proposal, root_returns, sam_joins, marisol_map/ask/undo, del_followed, gordon_talked,
// stair_ignored, did_bathroom, met_june, invited_morgan. Carries out: june_shadow_me, june_shadow_root, june_docs,
// rotation_passed, drawers_passed, brant_voted_down, advocate_written, gordon_sorry, del_returned.
import { lab, at, stage, CLOCK, P, HIDE, W, WAITW, S, SHOT, WAIT, CLIP, OBJ, ROAM, SIDE, FIGHT, CHOICE, O, A, IF, SET, ADD, CARD, MUSIC, STAGE, DRAW, DOOR, FX, NOTE, TASK, BT, BARK } from './dsl.mjs';

export const FIGHTS = {
  committee: { title: 'THE COMMITTEE', sub: 'A motion to form a committee to study the committee', where: 'stage', radius: 11, twist: 'score', twist_a: 16000, twist_b: 80,
    obj_label: 'GET TO A VOTE BY TEN', tier: 2, foes: [
    { who: 'napper', weapon: 'STAFF', ai: 'CASTER', hp: 60, power: .5, speed: .8, at: [-3, -6] },
    { who: 'regular', weapon: 'TWIN', ai: 'SKIRMISH', hp: 60, power: .5, speed: 1.05, at: [0, -7] },
    { who: 'haunter', weapon: 'SPEAR', ai: 'SKIRMISH', hp: 60, power: .5, speed: 1.0, at: [3, -6] } ],
    barks: [
      { on: 'start', who: 'napper', text: "I move we table this. I also move we nap." },
      { on: 'parry', who: 'regular', text: "Quick one! Can we take that block offline?" },
      { on: 'player_low', who: 'haunter', text: "What if the committee's real decision was the friends we made?" },
      { on: 'foe_low', who: 'regular', text: "Okay, I'm posting that the committee is dissolving." },
      { on: 'assist', who: 'm2', text: "Point of order. It's nine fifty. Vote." },
      { on: 'finish', who: 'napper', text: "Motion to stop forming committees. Seconded by me. Passed by me." } ],
    assists: [{ who: 'm2', flag: null, trust: 12 }] },
};

const BOARD = lab(10.0, 8.0, 180);   /* the moderator at the whiteboard, facing the tables */
const sit = (who, x, z, yaw) => [P(who, lab(x, z, yaw)), CLIP(who, 'Sitting_Idle_Loop', { loop: true })];

// the setup roam: Del's second night only if you promised it
const setup = (del) => ROAM('lab_classroom', 2.4, 'Start the meeting at the whiteboard', { indoors: true, hour: 19.0, until: 19.1, need: del ? 3 : 2, auto: 25, tasks: [
  TASK('signin', 'quorum', { at: at('lab_signin'), label: 'Sign in the members for quorum', where: 'Sign-in tablet', prompt: 'Sign in', reward: { trust: 1 },
    after: [BARK('m2', { ruth: "Seven voting members. We need eight. Somebody wake up Lou.", dex: "Seven. We need eight to vote. Is Lou a member? Lou's a member. Somebody wake Lou." })] }),
  TASK('carry', 'chairs', { at: at('lab_lounge'), to: at('lab_cowork'), n: 2, label: 'Pull chairs up to the tables', where: 'Lounge chairs', where2: 'Coworking tables', prompt: 'Grab a chair', reward: { vibe: 1 } }),
  ...(del ? [TASK('greet', 'del2', { who: 'del', at: at('lab_door', 0, -1.5), label: 'Del came back', prompt: 'Say hi', reward: { trust: 2 },
    after: [
      S('del', "You said Thursday at seven. It's Thursday at seven. I brought the pedal.", { shot: 'mcu', emote: 'joy', amount: .5 }),
      S('me', "Bench is yours. I'm moderating a meeting, but I'll be ten feet away. You hold the iron."),
      S('del', "I hold the iron. Okay.", { emote: 'joy', amount: .6 }),
      SET('del_returned'),
    ] })] : []),
  TASK('answer', 'bex', { who: 'bex', at: lab(13.2, 7.0), optional: true, label: 'Bex has a printout', where: 'Bex', prompt: 'Talk', reward: { trust: 1 },
    before: [
      IF({ flag: 'bex_proposal' }, [
        S('bex', "I posted the kitchen rotation a month ago and bumped it every week. It's been bumped so much it's flat.", { shot: 'mcu', emote: 'fun', amount: .4 }),
        S('bex', "If it fails, I'm moving into the fridge. As a protest. And for the cold."),
      ], [
        S('bex', "No proposal from me. I put a sign-up sheet on the fridge instead. Six names. Six.", { shot: 'mcu', emote: 'joy', amount: .4 }),
      ]),
    ] }),
], banter: [
  BT('m1', "Five bucks says this runs past ten.", { delay: 5 }),
  BT('napper', "I'm here for the cleaning part. I like the cleaning part.", { delay: 15, near: lab(16.3, 4.3) }),
] });

const beats = [
  MUSIC('hosting'), STAGE('town'),
  P('me', lab(9.2, 7.4, 60)), P('m2', lab(10.0, 8.2, -140), 'me'), P('m1', lab(11.6, 6.6, -100)),
  P('napper', lab(16.3, 4.3, -60)), CLIP('napper', 'Sitting_Idle_Loop', { loop: true }),
  FX('fade_in', .8),
  SHOT('two', ['m2', 'me']),
  CLIP('m2', 'Interact'),
  S('m2', { ruth: "Seven sharp. Any member can start the meeting, so I'm starting it. You're moderating.",
    dex: "It's seven. Anybody can start it, so I'm starting it. Sorry. You're moderating." }),
  S('me', "Me? The last thing I moderated was a group chat, and it got muted."),
  S('m2', { ruth: "You hosted a fifteen hour day and an open night. You can hold a marker for three hours.",
    dex: "You hosted a fifteen hour day. This is three hours and a marker. Should be fine." }),
  SHOT('insert', ['m2'], { part: 'rhand' }),
  NOTE('On the whiteboard: Introductions. Station reports. Issues and proposals. Votes. Then everybody cleans.'),
  S('m2', { ruth: "Votes need eight voting members in the room. Anybody can come and talk. Members vote.",
    dex: "We need eight voting members for a vote. Anybody can talk. Members vote. That part I know." }),
  OBJ('Get the meeting started'),
  IF({ flag: 'del_followed' }, [setup(true)], [setup(false)]),
  // ---- the meeting
  HIDE('napper', 'bex', 'del'),
  P('me', BOARD), P('m2', lab(9.0, 8.2, 160)),
  ...sit('m1', 11.3, 6.6, -90), ...sit('napper', 12.6, 7.0, -90), ...sit('regular', 12.4, 5.6, -90), ...sit('haunter', 13.6, 6.4, -90),
  ...sit('bex', 11.2, 5.4, -90), ...sit('marisol', 8.6, 6.4, -60), ...sit('june', 9.6, 5.4, -40), ...sit('gordon', 13.4, 5.2, -110),
  ...sit('kai', 7.6, 5.6, -40),
  IF({ flag: 'sam_joins' }, [...sit('sam', 10.6, 6.6, -80)]),
  IF({ flag: 'root_returns' }, [...sit('root', 14.4, 7.2, -100)]),
  IF({ flag: 'del_returned' }, [P('del', lab(19.4, 6.0, 0)), CLIP('del', 'Interact', { loop: true })]),
  CHAPTER_CARD(),
  SHOT('wide', ['me', 'm1', 'june'], { dist: 7, height: 2.2 }),
  S('me', "Okay. It's seven oh four. Introductions first. Your name and what you're working on."),
  IF({ flag: 'sam_joins' }, [
    S('sam', "Sam. Cardboard furniture. I'm a member now, which is still weird to say out loud.", { shot: 'mcu', clip: 'Sitting_Talking_Loop', emote: 'joy', amount: .5 }),
  ]),
  S('kai', "Kai. Drones you wear, or wearables that fly. It depends what broke last.", { clip: 'Sitting_Talking_Loop', emote: 'fun', amount: .5 }),
  IF({ flag: 'marisol_undo' }, [
    S('marisol', "Marisol. I'm the drawer person. Sorry about the drawers. Again.", { clip: 'Sitting_Talking_Loop', emote: 'sorrow', amount: .3 }),
  ], [
    S('marisol', "Marisol. The drawer wall. There's a map now. Please use the map.", { clip: 'Sitting_Talking_Loop', emote: 'joy', amount: .5 }),
  ]),
  IF({ flag: 'root_returns' }, [
    S('root', "Root. Member since one forty. Member again since last month.", { clip: 'Sitting_Talking_Loop', emote: 'joy', amount: .3 }),
  ]),
  S('gordon', "Gordon. The mill. Same as always.", { clip: 'Sitting_Talking_Loop' }),
  S('napper', "Lou. I'm working on staying awake. It's going okay.", { clip: 'Sitting_Talking_Loop', emote: 'fun', amount: .5 }),
  // station reports, and the one person who knows how a job is done
  S('me', "Station reports. {m2}, the laser?"),
  S('m2', { ruth: "The laser is fine. The lens is clean. I clean it. That's the report.",
    dex: "Laser's fine. Lens is clean. Somebody cleans it, and lately it's been me. That's the report." }),
  S('me', "June. Money."),
  SHOT('mcu', ['june']),
  S('june', "Rent's paid. Eleven new members since the open day, so we're a little ahead for once.", { clip: 'Sitting_Talking_Loop' }),
  IF({ flag: 'root_returns' }, [
    S('june', "Root paid what he owed, so his card's back on. No vote needed. That's the rule."),
    S('root', "Fifty a month. Still robbery.", { emote: 'joy', amount: .4 }),
  ]),
  IF({ flag: 'sam_joins' }, [S('june', "One of the eleven is Sam, who filled in every field on the form. Every one. Thank you, Sam.", { emote: 'joy', amount: .4 })]),
  S('june', "And one more thing. I'm not running for Treasurer again in October.", { shot: 'push' }),
  S('june', "Nine years of forms, all of it in my head. If a light rail train hits me, it hits the lab too."),
  SHOT('react', ['me', 'june']),
  IF({ flag: 'root_returns' }, [
    CHOICE("Nine years of work, and one person who knows how it's done.", [
      A("Raise your hand. Shadow her.", [ADD('trust', 2), ADD('energy', -1), SET('june_shadow_me'),
        S('june', "You? You already host. Are you sure?", { emote: 'sorrow', amount: .3 }),
        S('me', "I'm sure I'd rather learn it from you than from a spreadsheet you left behind."),
      ]),
      O("Root did the books at one forty.", [ADD('trust', 1), ADD('aff_m2', 1), SET('june_shadow_root'),
        S('root', "I did. Badly, on paper, in pen. All right. Show me the new way, June.", { emote: 'joy', amount: .5 }),
        S('june', "There's a new way. It's also pen. Sometimes.", { emote: 'joy', amount: .5 }),
      ], { say: "Root, didn't you do the books back at one forty?" }),
      O("Write it all down first. Then anyone can shadow.", [ADD('safety', 1), SET('june_docs'),
        S('june', "Write it down. I've been meaning to write it down for nine years. Okay. This month.", { emote: 'joy', amount: .3 }),
      ], { say: "Write the whole job down first, June. Then anybody can pick it up, not just whoever's here tonight." }),
    ]),
  ], [
    CHOICE("Nine years of work, and one person who knows how it's done.", [
      A("Raise your hand. Shadow her.", [ADD('trust', 2), ADD('energy', -1), SET('june_shadow_me'),
        S('june', "You? You already host. Are you sure?", { emote: 'sorrow', amount: .3 }),
        S('me', "I'm sure I'd rather learn it from you than from a spreadsheet you left behind."),
      ]),
      O("Write it all down first. Then anyone can shadow.", [ADD('safety', 1), SET('june_docs'),
        S('june', "Write it down. I've been meaning to write it down for nine years. Okay. This month.", { emote: 'joy', amount: .3 }),
      ], { say: "Write the whole job down first, June. Then anybody can pick it up, not just whoever's here tonight." }),
    ]),
  ]),
  // ---- issues and proposals
  S('me', "Proposals. Each one was posted to the group at least nine days ago, and its author is here. Bex?"),
  IF({ flag: 'bex_proposal' }, [
    S('bex', "A kitchen rotation. Four names a month. My name's on month two, not month one. That's the whole proposal.", { shot: 'mcu', clip: 'Sitting_Talking_Loop' }),
    S('me', "All in favor?"),
    SHOT('wide', ['m1', 'bex', 'kai'], { dist: 6 }),
    NOTE('Every hand goes up. Gordon raises his slowly, which still counts.'),
    S('bex', "Oh. Okay. I don't know what to do with my hands now.", { emote: 'joy', amount: .8 }),
    SET('rotation_passed'), ADD('trust', 1), ADD('vibe', 1),
  ], [
    IF({ flag: 'helped_bex' }, [
      S('bex', "No proposal. There's a sign-up sheet on the fridge. Six names so far, and none of them is mine.", { shot: 'mcu', clip: 'Sitting_Talking_Loop', emote: 'joy', amount: .4 }),
    ], [
      S('bex', "No proposal. The jar is back. I think it's learned to open the door from the inside.", { shot: 'mcu', clip: 'Sitting_Talking_Loop', emote: 'angry', amount: .5 }),
      ADD('safety', -1),
    ]),
  ]),
  S('me', "Marisol."),
  IF({ flag: 'marisol_undo' }, [
    S('marisol', "I put the drawers back. Then I posted this, nine days early, like you said.", { clip: 'Sitting_Talking_Loop' }),
    S('marisol', "Colors, with a map on the wall, for a one month trial. If people hate it, I undo it myself."),
  ], [
    S('marisol', "I built it and tested it for a month. People found things. Here's the map. Let's make it official.", { clip: 'Sitting_Talking_Loop', emote: 'joy', amount: .4 }),
  ]),
  SHOT('low', ['gordon']),
  S('gordon', "I move we form a committee to study the drawer question.", { clip: 'Sitting_Talking_Loop' }),
  S('napper', "Seconded. And a committee to pick who's on the committee.", { emote: 'fun', amount: .5 }),
  S('regular', "Quick one. Can the committee have its own chat? A third chat?", { emote: 'joy', amount: .6 }),
  S('haunter', "What if the committee is just vibes? Is that allowed?", { emote: 'fun', amount: .8 }),
  SHOT('push', ['me']),
  S('m2', { ruth: "It's nine forty. If this doesn't get to a vote by ten, it waits a month. I suspect that's the idea.",
    dex: "It's nine forty. If we don't vote by ten it waits a month. That's what committees are for, honestly." }, { to: 'me' }),
  S('me', "Point of order. Everybody who wants a committee, please stand up and volunteer to be on it."),
  S('napper', "Oh. I didn't mean me. I meant a committee of other people.", { emote: 'sorrow', amount: .3 }),
  S('regular', "Same. I'm just going to post about it.", { emote: 'fun', amount: .4 }),
  S('haunter', "Do the committee people get foam swords? Because I brought foam swords.", { emote: 'joy', amount: .9 }),
  FX('flash', .4), STAGE('unfold'),
  P('me', stage(0, 7, 180)), P('napper', stage(-3, -6, 0), 'me'), P('regular', stage(0, -7, 0), 'me'), P('haunter', stage(3, -6, 0), 'me'),
  DRAW('napper', 'STAFF'), DRAW('regular', 'TWIN'), DRAW('haunter', 'SPEAR'),
  SHOT('crane', ['me'], { dist: 8, dur: 2.6 }),
  NOTE('The meeting opens out. Every motion ever tabled hangs in the air, waiting.'),
  WAIT(1.6),
  SHOT('hero', ['me']), DRAW('me', 'ARC'), WAIT(.4),
  SHOT('whip', ['regular']),
  MUSIC('fight'),
  FIGHT('committee', { cant_lose: true }),
  MUSIC('hosting'),
  DRAW('me', -1), DRAW('napper', -1), DRAW('regular', -1), DRAW('haunter', -1),
  FX('flash', .25), STAGE('town'),
  P('me', BOARD),
  ...sit('napper', 12.6, 7.0, -90), ...sit('regular', 12.4, 5.6, -90), ...sit('haunter', 13.6, 6.4, -90),
  NOTE('The committee is dissolved. Lou wipes it off the whiteboard himself.'),
  S('me', "Nine fifty two. All in favor of Marisol's drawers?"),
  SHOT('wide', ['marisol', 'june', 'kai'], { dist: 6 }),
  NOTE('Eleven hands. Gordon abstains, loudly.'),
  S('marisol', "It passed? It passed! I have to implement it in three months. I already did. I'll do it again.", { clip: 'Sitting_Talking_Loop', emote: 'joy', amount: 1 }),
  SET('drawers_passed'), ADD('vibe', 1),
  // Brant, by the rules
  P('brant', lab(7.2, 4.4, 60)),
  SHOT('low', ['brant']),
  MUSIC('tension'),
  S('brant', "Brant Holloway, HiveDesk. I read your bylaws. Anyone can bring a proposal? I'm bringing MakerHub.", { emote: 'joy', amount: 1 }),
  S('m2', { ruth: "Proposals go to the group nine days ahead. Did you post it?", dex: "Proposals have to be posted to the group nine days ahead. Sorry. Did you post it?" }),
  S('brant', "Nine days ago. In the comments under a picture of a cat. It counts. I checked."),
  S('june', "It counts. I saw it. It got two likes, and one of them was the cat's owner.", { emote: 'fun', amount: .5 }),
  S('brant', "So. HiveDesk pays the rent, the lab becomes MakerHub, members only. And what you make is ours. Vote yes.", { shot: 'mcu' }),
  S('me', "That's the same offer as last month. With a cat."),
  S('me', "Members who are voting: all in favor of MakerHub?"),
  SHOT('wide', ['m1', 'bex', 'kai'], { dist: 6 }),
  NOTE('No hands. Not even Gordon.'),
  S('me', "All against?"),
  NOTE('Every hand in the room. Lou raises both.'),
  S('brant', "Okay. Wow. Love the process. Hate the process.", { emote: 'sorrow', amount: .5 }),
  S('brant', "Then I'll do it the other way. Tomorrow. Second Friday. My pop-up, across the street. Bring your makers.", { shot: 'push', emote: 'angry', amount: .4 }),
  SET('brant_voted_down'), ADD('trust', 1),
  HIDE('brant'),
  MUSIC('hosting'),
  // ---- then everybody cleans
  S('me', "That's the agenda. Last item. Everybody cleans."),
  HIDE('m1', 'napper', 'regular', 'haunter', 'bex', 'marisol', 'june', 'kai', 'sam', 'root', 'del'),
  P('gordon', lab(20.6, 5.8, -100)), P('regular', lab(14.2, 4.2, 90)),
  OBJ('Then everybody cleans'),
  ROAM('lab_entry', 2.2, 'Walk the last people out', { indoors: true, hour: 21.9, until: 22.2, need: 2, tasks: [
    TASK('errand', 'board', { at: at('lab_classroom'), label: 'Wipe the whiteboard', where: 'Whiteboard', prompt: 'Wipe it', reward: { vibe: 1 },
      after: [BARK('me', "Every motion, gone. The cat proposal, gone.")] }),
    TASK('carry', 'stack', { at: at('lab_cowork'), to: at('lab_lounge'), n: 2, label: 'Put the chairs back', where: 'Coworking tables', where2: 'Lounge', prompt: 'Grab a chair', reward: { safety: 1 } }),
    TASK('errand', 'free', { at: at('lab_teardown', 0, -.8), optional: true, label: 'Sort the free shelf', where: 'Free shelf', prompt: 'Sort it', reward: { safety: 1, vibe: 1 },
      after: [SET('free_sorted'), NOTE('A sign on the free shelf: THIRD AND FINAL NOTICE. The first and second were implied. It is now a shelf again.')] }),
    TASK('answer', 'stair', { who: 'gordon', at: lab(20.6, 5.8), optional: true, label: 'Gordon is still at the bench', where: 'Electronics bench', prompt: 'Talk', reward: { trust: 1 },
      before: [
        IF({ flag: 'gordon_talked' }, [
          S('gordon', "That newcomer last month. Del. Did she ever come back?", { shot: 'mcu' }),
          IF({ flag: 'del_returned' }, [
            S('me', "She came back tonight. She soldered the whole pedal herself. She's packing up right now."),
            S('gordon', "Then I'm going to go say I'm sorry. And not touch her iron while I say it.", { emote: 'sorrow', amount: .4 }),
            SET('gordon_sorry'), SET('stair_fixed'),
          ], [
            S('me', "No. Not yet."),
            S('gordon', "Huh. Twelve years, and I never once wondered why people stop coming.", { emote: 'sorrow', amount: .5 }),
            SET('gordon_sorry'), SET('stair_fixed'),
          ]),
        ], [
          S('gordon', "Good meeting. Too many feelings about drawers, but a good meeting.", { shot: 'mcu' }),
          CHOICE("Pat has warned three newcomers about Gordon tonight. Gordon has no idea.", [
            A("Write to the Advocate tonight.", [ADD('trust', 1), SET('advocate_written'), SET('stair_fixed'),
              NOTE("You write to the Board's Advocate: what you saw, when, who left. Plain facts. It takes eleven minutes."),
            ]),
            A("Tell Gordon yourself. Kindly.", [ADD('trust', 1), SET('gordon_talked'),
              S('me', "Gordon, people leave after you help them. Del left. I don't think you've noticed."),
              S('gordon', "I was helping.", { emote: 'angry', amount: .3 }),
              S('me', "I know. Next time, ask if they want help, and then hand the iron back."),
              S('gordon', "Ask first. Hand it back. All right. I can try that.", { emote: 'sorrow', amount: .4 }),
              SET('gordon_sorry'), SET('stair_fixed'),
            ]),
            O("Leave it. Everybody knows.", [SET('stair_ignored'),
              S('regular', "That's the spirit. Just route around him.", { emote: 'joy', amount: .3 }),
            ], { say: "Leave it. Everybody knows how Gordon is." }),
          ]),
        ]),
      ] }),
  ], banter: [
    BT('napper', "This is the best part. Everybody gets a rag.", { delay: 4 }),
    BT('regular', "Quick one. The meeting notes. Who's writing them? Okay, I'm posting that nobody is.", { delay: 18 }),
  ] }),
  HIDE('gordon', 'regular'),
  P('me', lab(6.8, 3.6, -90)), P('june', lab(7.8, 3.9, -100), 'me'),
  SHOT('two', ['june', 'me']),
  IF({ flag: 'june_shadow_me' }, [
    CLIP('june', 'Interact'), SHOT('insert', ['june'], { part: 'rhand' }),
    S('june', "Here. How to be Treasurer. I finally wrote it down. Fourteen pages, and nine of them are about the bank.", { emote: 'joy', amount: .4 }),
    S('me', "Nine pages on the bank?"),
    S('june', "The bank is a lot.", { emote: 'fun', amount: .5 }),
    SET('june_docs'),
  ]),
  IF({ flag: 'did_bathroom' }, [
    S('june', "And somebody restocked the bathroom at hack night. I put it in the station report notes, by name. Yours.", { emote: 'joy', amount: .4 }),
  ]),
  S('june', "Tomorrow's Second Friday. Downtown's open late. That man's pop-up will be across the street."),
  S('me', "We'll be out front with a fix-it table. Marisol's idea. Marisol's table."),
  FX('fade_out', 1),
];
// the night's own save point card inside the meeting: a CHAPTER beat
function CHAPTER_CARD() { return { op: 'chapter', hour: 19.1, title: 'The Meeting', sub: 'Introductions, reports, proposals, votes, then cleaning' }; }

export const NIGHT = {
  n: 2, hour: 19, title: 'Hack Your Hackerspace', sub: 'A month on. The second Thursday, seven sharp.', beats, fights: FIGHTS,
  endings: [
    { flag: 'brant_voted_down', title: 'NIGHT TWO', lines: ['The meeting ran to ten fourteen. Somebody wrote the notes.', 'Nobody voted for the man with the cat.'] },
  ],
  stat_lines: [
    { flag: 'rotation_passed', text: 'The kitchen rotation passed unanimously. Bex is on month two.' },
    { flag: 'drawers_passed', text: "Marisol's drawer system is official. The map is laminated." },
    { flag: 'june_shadow_me', text: 'You are shadowing the Treasurer. The bank is a lot.' },
    { flag: 'june_shadow_root', text: 'Root is shadowing the Treasurer, in pen.' },
    { flag: 'june_docs', not: 'june_shadow_me', text: 'June started writing the Treasurer job down. Page one is about the bank.' },
    { flag: 'gordon_sorry', text: 'Gordon asked before he helped. Twice. It looked like it hurt.' },
    { flag: 'advocate_written', text: 'The Advocate read your note and set up a talk with Gordon for next week.' },
    { flag: 'del_returned', text: 'Del soldered a whole pedal herself. It makes a sound. A good one.' },
    { flag: 'free_sorted', text: 'The free shelf is a shelf again.' },
  ],
};
