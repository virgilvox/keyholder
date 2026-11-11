// KEYHOLDER, night three: Second Friday, the next night. Downtown is open late; the lab has a fix-it table on the
// sidewalk, and Brant has a pop-up across the street. Then lock up, and the walk through orientation, given by you.
// Shown, never said: the person who builds a thing sets how it runs (Marisol's table: the owner holds the
// screwdriver); a job needs a second person who knows it; you cannot buy a room full of people who chose to be
// there; the one who trains a replacement is trusted with the next thing; the problem fixed, or not, from night one.
// Carries in: everything. Uses: sam_joins, gloria_quiet, gloria_visits, has_bike, bike_pumped, kai_flew, asked_kai,
// brant_voted_down, gordon_sorry, advocate_written, stair_ignored, del_returned, rotation_passed, june_shadow_*,
// invited_morgan, root_returns. Carries out: mari_backup, dobbs_rides, intern_stays, and the final ending.
import { lab, at, stage, CLOCK, P, HIDE, W, WAITW, S, SHOT, WAIT, CLIP, OBJ, ROAM, SIDE, FIGHT, CHOICE, O, A, IF, SET, ADD, CARD, MUSIC, STAGE, DRAW, DOOR, FX, NOTE, TASK, BT, BARK, BIKE, BIKE_WITH, PROP } from './dsl.mjs';

export const FIGHTS = {
  makerhub: { title: 'A BRIEF WORD FROM OUR SPONSOR, AGAIN', sub: 'MakerHub, by HiveDesk. The hackerspace, minus the hackers', where: 'mid_block', radius: 11, twist: 'defend', twist_a: 80,
    obj: [0, -6], obj_label: 'GUARD THE FIX-IT BINS', tier: 4, foes: [
    { who: 'brant', weapon: 'ARC', ai: 'BOSS', hp: 320, power: 1.0, speed: 1.08, at: [0, -3] },
    { who: 'intern', weapon: 'STAFF', ai: 'CASTER', hp: 45, power: .5, speed: .95, at: [-5, -4] },
    { who: 'intern', weapon: 'TWIN', ai: 'SKIRMISH', hp: 45, power: .5, speed: 1.0, at: [5, -4], palette: 'alt' } ],
    barks: [
      { on: 'start', who: 'brant', text: "Team! We're onboarding their inventory. Every broken lamp is a lead." },
      { on: 'obj_warn', who: 'marisol', text: "They're taking Gloria's lamp! It's half open!" },
      { on: 'hazard', who: 'intern', text: "Per Brant, this toaster is now a subscription." },
      { on: 'parry', who: 'brant', text: "Love the pushback. Still love it. Less than last month." },
      { on: 'player_low', who: 'brant', text: "Have you considered a paid tier? You'd have more energy." },
      { on: 'foe_low', who: 'brant', text: "Okay. Real talk. The blazer is a rental." },
      { on: 'assist', who: 'marisol', text: "My table, my rules! Rule one, hands off the bins!" },
      { on: 'finish', who: 'brant', text: "Fine! Keep your lamps! Keep your whole street!" } ],
    assists: [{ who: 'marisol', flag: 'drawers_passed', trust: 99 }, { who: 'root', flag: 'root_returns', trust: 99 }, { who: 'm1', flag: null, trust: 16 }] },
  handoff_sam: { title: 'WALK THROUGH ORIENTATION', sub: 'As revised again. Given by you', where: 'lounge', radius: 5.5, twist: 'none', tier: 1, foes: [
    { who: 'sam', weapon: 'ARC', ai: 'DUELIST', hp: 60, power: .4, speed: .9, at: [0, -3] } ],
    barks: [
      { on: 'start', who: 'sam', text: "It's cardboard. I laminated it. Guard up, I think?" },
      { on: 'parry', who: 'sam', text: "Oh! I blocked! Is that in the manual?" },
      { on: 'player_low', who: 'm1', text: "Relax, it's orientation. Nobody fails orientation." },
      { on: 'foe_low', who: 'sam', text: "The blade's bending. That's a design note." },
      { on: 'finish', who: 'sam', text: "Did I pass? I held it by the right end." } ] },
  handoff_mari: { title: 'WALK THROUGH ORIENTATION', sub: 'As revised again. Given by you', where: 'lounge', radius: 5.5, twist: 'none', tier: 1, foes: [
    { who: 'marisol', weapon: 'STAFF', ai: 'DUELIST', hp: 60, power: .4, speed: .95, at: [0, -3] } ],
    barks: [
      { on: 'start', who: 'marisol', text: "Hosting orientation! I've been waiting all month for this part." },
      { on: 'parry', who: 'marisol', text: "Good block. I'm labeling it." },
      { on: 'player_low', who: 'm1', text: "Relax, it's orientation. Nobody fails orientation." },
      { on: 'foe_low', who: 'marisol', text: "Okay, okay, I yield. Is yielding a thing? I yield." },
      { on: 'finish', who: 'marisol', text: "Did I pass? I held it by the right end." } ] },
};

const TABLE = at('lab_door', 3.6, 2.4, 0);   /* the fix-it table on the sidewalk, facing Main */
const beats = [
  MUSIC('explore'), STAGE('town'),
  PROP('table', TABLE, 'still'),
  P('marisol', at('lab_door', 3.6, 1.6, 0)), CLIP('marisol', 'Interact', { loop: true }),
  P('me', at('lab_door', -4, 6, 70)), P('m1', at('lab_door', 1.2, 3.4, 120)),
  FX('fade_in', 1),
  SHOT('establish', ['me'], { dist: 15, height: 4.5 }), WAIT(1.2),
  W('me', at('lab_door', 2.0, 3.8), { speed: 1.3 }),
  SHOT('two', ['marisol', 'me']),
  CLIP('marisol', 'Idle_Loop', { loop: true }),
  S('marisol', "Welcome to the fix-it table! My table, so my rules. There are three.", { emote: 'joy', amount: .9 }),
  S('marisol', "The owner holds the screwdriver. We hold the light. And anybody can say try the other way."),
  S('me', "You built a whole table and wrote rules for it. In one month."),
  S('marisol', "I asked in the chat first. Saturday morning. Four people said yes, one said finally.", { emote: 'fun', amount: .6 }),
  S('marisol', "Also, if I get sick, somebody has to know where the zip ties are. Will you be my backup?", { shot: 'mcu' }),
  CHOICE("Marisol's table runs on Marisol.", [
    A("Learn where the zip ties are.", [SET('mari_backup'), ADD('trust', 1),
      S('marisol', "Bottom left. Under the label that says zip ties. I'm so glad you asked where.", { emote: 'joy', amount: .7 }),
    ]),
    O("Write it on the underside of the table.", [SET('mari_backup'), SET('mari_docs'), ADD('safety', 1),
      S('marisol', "The underside! Where only a backup would look. Oh, that's good.", { emote: 'joy', amount: .8 }),
    ], { say: "Write it all on the underside of the table. Then anybody who sits down here is your backup." }),
  ]),
  S('m1', { vee: "Five bucks says Brant opens with a megaphone.", dex: "He's across the street. He has a megaphone. I wish I was guessing." }, { emote: 'fun', amount: .5 }),
  BARK('brant', "MakerHub! The hackerspace, minus the hassle! First hour free!"),
  OBJ('Run the fix-it table'),
  MUSIC('hosting'),
  // broken things, and their owners, who keep hold of the screwdriver
  IF({ flag: 'has_bike' }, [BIKE(at('lab_door', 6.2, 2.0, 90))]),
  ROAM('nile', 4, "Go see Brant's pop-up", { hour: 18.1, until: 18.9, need: 2, tasks: [
    TASK('fix', 'lamp', { who: 'gloria', at: at('lab_door', 4.6, 3.4), mode: 'stick', label: "Help Gloria fix her lamp", where: 'Fix-it table', prompt: 'Hold the light', reward: { trust: 1, vibe: 1 },
      before: [
        IF({ flag: 'gloria_visits' }, [
          S('gloria', "You got my husband's clock running last month. Now his desk lamp is jealous.", { shot: 'mcu', emote: 'joy', amount: .5 }),
        ], [
          IF({ flag: 'gloria_quiet' }, [
            S('gloria', "I'm the neighbor. Your quiet hours held all month, so I brought you a lamp instead of a complaint.", { shot: 'mcu', emote: 'joy', amount: .5 }),
          ], [
            S('gloria', "I'm the neighbor with the slippers. The saw at two in the morning stopped. So I brought you a lamp.", { shot: 'mcu', emote: 'fun', amount: .4 }),
          ]),
        ]),
        S('marisol', "Rule one. You hold the screwdriver, Gloria. We hold the light."),
        S('gloria', "Me? I haven't held a screwdriver since nineteen ninety one.", { emote: 'sorrow', amount: .2 }),
      ],
      after: [
        S('gloria', "It's on. I did that. Write that down somewhere, honey.", { emote: 'joy', amount: 1 }),
        SET('gloria_lamp'),
      ] }),
    TASK('fix', 'car', { who: 'milo', with: 'dana', at: at('lab_door', 2.6, 3.6), mode: 'timing', label: "Milo's car only drives backwards", where: 'Fix-it table', prompt: 'Help', reward: { safety: 1, vibe: 1 },
      before: [
        S('dana', "Hi again. His remote car only goes backwards. He's been driving it backwards to school. Proudly.", { shot: 'mcu', emote: 'fun', amount: .6 }),
        S('milo', "Can I hold the screwdriver? I'm allowed to hold it here. It's a rule."),
        S('me', "It's the main rule. Glasses on. Let's see what's backwards."),
        S('milo', "It's the motor. It's like Ines's robot. Red to black!", { emote: 'joy', amount: 1 }),
      ],
      after: [
        S('milo', "Forwards! Mom, it goes forwards! Can I still drive it backwards sometimes?", { emote: 'joy', amount: 1 }),
        S('dana', "You can drive it any direction you fixed it to go.", { emote: 'joy', amount: .6 }),
        SET('milo_car'),
      ] }),
    TASK('fix', 'dobbs', { who: 'dobbs', at: at('lab_door', 6.6, 2.8), mode: 'hold', optional: true, label: 'Someone with a wobbly wheel', where: 'By the bike rack', prompt: 'Show him', reward: { trust: 1 },
      before: [
        S('dobbs', "Hi. It's me. From the bike thing. I learned to ride. Mostly. My wheel goes like this now.", { shot: 'mcu', emote: 'sorrow', amount: .3 }),
        S('me', "Dobbs. You came to the fix-it table."),
        S('dobbs', "Rennick says it's beneath us. Rennick can't ride either. Can you show me? I'll hold the wrench."),
      ],
      after: [
        S('dobbs', "It's straight. I trued a wheel. I'm going to tell Rennick, and he's going to be so annoyed.", { emote: 'joy', amount: .8 }),
        SET('dobbs_rides'),
      ] }),
  ], sides: [
    SIDE('kai', 'kai', at('lab_door', -2.2, 3.2, 60), 'Talk', [
      IF({ flag: 'asked_kai' }, [
        S('kai', "I've got a lease in Tempe now. A real one, with my name on it. I'm framing it.", { shot: 'mcu', emote: 'joy', amount: .7 }),
      ], [
        S('kai', "Brant offered me a hot desk. I asked if I'd own what I built there. He said spiritually.", { shot: 'mcu', emote: 'fun', amount: .5 }),
      ]),
      IF({ flag: 'kai_flew' }, [S('kai', "Also the drone's going in the Second Friday parade. Above it. On a tether. Supervised.", { emote: 'joy', amount: .8 })]),
    ]),
    SIDE('morgan', 'morgan', at('lab_door', -5.5, 3.6, 70), 'Talk', [
      IF({ flag: 'invited_morgan' }, [
        S('morgan', "My who's in charge piece got forty letters. Half said nobody. Half said the fridge.", { shot: 'mcu', emote: 'fun', amount: .5 }),
        S('morgan', "So tonight's piece is about a table where the owner holds the screwdriver. Who decided that?"),
        S('me', "The person who built the table."),
        S('morgan', "That's the whole article, isn't it.", { emote: 'joy', amount: .4 }),
      ], [
        S('morgan', "Downtown paper. I'm covering Second Friday. What's free here, the advice?", { shot: 'mcu' }),
        S('me', "The advice, the fixing, and the light. The lamp's yours to keep."),
      ]),
    ]),
  ], banter: [
    BT('brant', "MakerHub! Branded aprons for every fixer! Twenty two an hour!", { delay: 6 }),
    BT('marisol', "Somebody tell him we already have aprons. Ours have burn holes.", { delay: 12 }),
    BT('m1', "Thirty people at our table, three at his. I'm not counting. I'm counting.", { delay: 30 }),
  ] }),
  // across the street: the pop-up
  // on the Nile's sidewalk, clear of the parked cars
  P('brant', at('nile', 2.0, -.6, -90)), P('intern', at('nile', 2.9, .4, -110)),
  P('me', at('nile', -.9, -1.0, 90)), P('marisol', at('nile', -2.0, -.2, 80)), P('m1', at('nile', -1.6, -2.0, 60)),
  SHOT('two', ['brant', 'me']),
  MUSIC('tension'),
  S('brant', "Welcome to MakerHub! Paid fixers, branded aprons, twenty two an hour. Marisol. Let's talk about your future.", { emote: 'joy', amount: 1 }),
  S('marisol', "I already have an apron. It has burn holes, and every one of them is mine.", { shot: 'mcu', emote: 'fun', amount: .5 }),
  S('brant', "And what you fix at MakerHub belongs to MakerHub. Standard. Same as last time."),
  IF({ flag: 'brant_voted_down' }, [
    S('me', "The members voted on this last night, Brant. Everybody in the room said no. Even Gordon."),
    S('brant', "Votes are a vibe. Revenue is a fact.", { emote: 'joy', amount: .6 }),
  ], [
    S('me', "You pitched this a month ago. The answer's still on the whiteboard. It says no."),
  ]),
  SHOT('push', ['brant']),
  S('brant', "Fine. If the makers won't come to MakerHub, MakerHub goes to the makers. Team? Onboard their inventory.", { emote: 'angry', amount: .5 }),
  S('intern', "Per Brant, every broken lamp on that table is now a lead."),
  S('marisol', "They're going for the bins! Gloria's lamp is in there, half open!", { emote: 'angry', amount: .8 }),
  DRAW('brant', 'ARC'), DRAW('intern', 'STAFF'),
  FX('fade_out', .3),
  P('me', at('mid_block', -3, 5, 180)), P('brant', at('mid_block', 0, -3, 0), 'me'), P('intern', at('mid_block', -5, -4, 20), 'me'),
  FX('fade_in', .3),
  SHOT('hero', ['me']), DRAW('me', 'ARC'), WAIT(.4),
  SHOT('whip', ['brant']),
  MUSIC('boss'),
  FIGHT('makerhub'),
  MUSIC('cine'),
  DRAW('me', -1), DRAW('brant', -1), DRAW('intern', -1),
  SHOT('two', ['me', 'brant']),
  S('brant', "Real talk. Our pop-up had zero walk ins tonight. Zero. You had what, forty?", { emote: 'sorrow', amount: .5 }),
  S('me', "Thirty one. Marisol counted. She made a tally sheet. It's color coded."),
  S('brant', "I'm going to circle back. I mean that as a threat this time.", { emote: 'angry', amount: .3 }),
  SHOT('react', ['intern', 'me']),
  CLIP('intern', 'Interact'),
  S('intern', "Hey. Is the fix-it table hiring?"),
  S('marisol', "Nobody's hired. You just pick up a screwdriver and somebody holds the light.", { emote: 'joy', amount: .5 }),
  S('intern', "Then I'm taking off the apron. Per me.", { emote: 'joy', amount: .5 }),
  SET('intern_stays'), ADD('vibe', 1), ADD('trust', 1),
  HIDE('brant'),
  // the bench inside, and the person everyone used to route round
  IF({ flag: 'gordon_sorry' }, [
    P('del', at('lab_door', 3.0, 1.6, 0)), P('gordon', at('lab_door', 4.6, 1.2, -60)),
    SHOT('two', ['del', 'gordon']),
    S('del', "Gordon asked if I wanted help. I said no. He said okay, and handed me the flux.", { to: 'me', emote: 'joy', amount: .5 }),
    S('gordon', "I'm working on it. It's harder than the mill.", { emote: 'fun', amount: .3 }),
  ], [
    IF({ flag: 'advocate_written' }, [
      P('del', at('lab_door', 3.0, 1.6, 0)),
      SHOT('mcu', ['del']),
      S('del', "Somebody from the Board called me. Asked what happened, and listened. I came back to see if it's different.", { to: 'me', emote: 'joy', amount: .3 }),
      S('del', "Gordon's at the mill with a buddy tonight. The bench feels like a bench."),
    ], [
      NOTE('Inside, at the bench, a newcomer lets Gordon take her iron. She leaves early. Pat tells the next one not to ask him.'),
    ]),
  ]),
  HIDE('del', 'gordon', 'intern', 'marisol', 'm1', 'gloria', 'dana', 'milo', 'dobbs', 'kai', 'morgan'),
  PROP('table', null, 'hide'),
  BIKE(null),
  // ---- lock up, and the walk through orientation, given by you
  CLOCK(21.9),
  MUSIC('night'),
  P('me', lab(15.1, 4.7, 80)), P('m2', lab(14.2, 5.6, 90)), P('m1', lab(16.6, 6.0, -120)),
  IF({ flag: 'sam_joins' }, [P('sam', lab(16.25, 4.95, -100), 'me')], [P('marisol', lab(16.25, 4.95, -100), 'me')]),
  FX('fade_in', .6),
  SHOT('wide', ['me', 'm2', 'm1'], { dist: 5, height: 2 }),
  S('m2', { ruth: "Lock up. Then one more thing. Next Tuesday needs a host, and they haven't done the walk through orientation.",
    dex: "Lock up's done. One more thing. Next Tuesday needs a host, and they haven't had the walk through orientation." }),
  S('m2', { ruth: "Somebody has to do the training. Tonight, that's you.", dex: "Somebody has to train them. Sorry. Tonight that's you." }),
  IF({ flag: 'sam_joins' }, [
    S('sam', "I signed up to host. Before I could talk myself out of it.", { emote: 'joy', amount: .5, shot: 'mcu' }),
    CLIP('sam', 'Interact'), SHOT('insert', ['sam'], { part: 'rhand' }),
    S('sam', "It's cardboard. I laminated it. The slots are a hair narrower."),
    S('me', "Is that a sword? In the lounge? At ten at night?"),
    S('m1', { vee: "Five bucks says you say exactly what {m2} said to you.", dex: "I bet you say exactly what {m2} said to you. I'd take that bet." }, { emote: 'fun', amount: .6 }),
    S('me', "Weapons get handled carefully, by trained people. So somebody has to train you. Catch."),
    DRAW('sam', 'ARC'),
  ], [
    S('marisol', "I signed up to host next Tuesday. Before anybody could talk me out of it.", { emote: 'joy', amount: .8, shot: 'mcu' }),
    S('me', "Weapons get handled carefully, by trained people. So somebody has to train you."),
    S('m1', { vee: "Five bucks says you say exactly what {m2} said to you.", dex: "I bet you say exactly what {m2} said to you. I'd take that bet." }, { emote: 'fun', amount: .6 }),
    DRAW('marisol', 'STAFF'),
  ]),
  SHOT('hero', ['me']), DRAW('me', 'ARC'), WAIT(.5),
  MUSIC('fight'),
  IF({ flag: 'sam_joins' }, [FIGHT('handoff_sam', { cant_lose: true })], [FIGHT('handoff_mari', { cant_lose: true })]),
  MUSIC('cine'),
  DRAW('me', -1), DRAW('sam', -1), DRAW('marisol', -1),
  SHOT('two', ['me', 'm2']),
  S('me', "You pass. You even held it by the right end."),
  CLIP('me', 'Interact'), SHOT('insert', ['me'], { part: 'rhand' }),
  IF({ flag: 'sam_joins' }, [
    S('me', "Here's the host lanyard. It doesn't open anything. It just means every question on Tuesday goes to you."),
    S('sam', "And when somebody walks in, I look up.", { emote: 'joy', amount: .8 }),
  ], [
    S('me', "Here's the host lanyard. It doesn't open anything. It just means every question on Tuesday goes to you."),
    S('marisol', "And when somebody walks in, I look up. And then I ask before I relabel them.", { emote: 'joy', amount: .8 }),
  ]),
  IF({ stat: 'trust', gte: 22 }, [
    SET('ending_keyholders'),
    S('m1', { vee: "You trained your replacement. Five bucks says they ask you to do the next thing.",
      dex: "You trained your replacement. Somebody's going to ask you to do something bigger now. Sorry in advance." }, { emote: 'joy', amount: .6 }),
    IF({ flag: 'june_shadow_me' }, [S('me', "I already said yes to the bank. The bank is a lot.", { emote: 'fun', amount: .5 })]),
  ], [
    SET('ending_handoff'),
    S('m1', { vee: "Go home. Sleep. That's not a bet, that's an order.", dex: "Go home. Sleep. Somebody else has Tuesday." }, { emote: 'joy', amount: .4 }),
  ]),
  FX('lights_out', .6),
  CLOCK(22),
  STAGE('town'),
  HIDE('m1', 'm2', 'sam', 'marisol'),
  P('me', at('lab_door', 0, 2.4, 180)),
  FX('fade_in', 1.2),
  SHOT('establish', ['me'], { dist: 13, height: 3 }),
  NOTE('The A-frame comes in. The door locks. On the window, a new host is on the calendar for Tuesday.'),
  WAIT(1),
];

export const NIGHT = {
  n: 3, hour: 18.0, title: 'Second Friday', sub: 'The next night. Downtown is open late.', beats, fights: FIGHTS,
  endings: [
    { flag: 'ending_keyholders', title: 'KEYHOLDERS', lines: [
      'You trained the next host, and somebody already asked you to do the next thing.',
      'The door is open Tuesday, Wednesday and Saturday. Anybody can walk in.' ] },
    { flag: 'ending_handoff', title: 'THE NEXT HOST', lines: [
      'Somebody else has Tuesday. You slept until noon.',
      'The door is open Tuesday, Wednesday and Saturday. Anybody can walk in.' ] },
  ],
  stat_lines: [
    { flag: 'mari_backup', text: 'The fix-it table has a backup. The zip ties are bottom left.' },
    { flag: 'gloria_lamp', text: "Gloria's lamp is on in the apartment behind the lab. She fixed it herself." },
    { flag: 'milo_car', text: 'Milo drives forwards now. Mostly.' },
    { flag: 'dobbs_rides', text: 'Dobbs rides to the lab on Tuesdays. Rennick walks.' },
    { flag: 'intern_stays', text: 'One of the interns runs the fix-it table on Second Fridays. Per nobody.' },
    { flag: 'gordon_sorry', text: 'Gordon asks before he helps. New people stay.' },
    { flag: 'advocate_written', not: 'gordon_sorry', text: 'The Board talked with Gordon. The bench feels like a bench.' },
    { flag: 'stair_ignored', not: 'stair_fixed', text: 'People still route round Gordon. Some of them stop coming. Nobody counts them.' },
    { flag: 'rotation_passed', text: 'The kitchen rotation is on month two. Bex is cooking, not cleaning.' },
    { flag: 'sam_joins', text: 'Sam hosts on Tuesdays. Sam looks up.' },
  ],
};
