// The attic (docs/ATTIC.md). attic.c plays these scenes through the story VM: the nightmare's arrival and its
// encounters, SYSOP at the rack, the room come back to itself, and the flashbacks a found object opens. Each
// scene ends by putting the day back as it was (story_day_hold), so nothing here moves the day's stats.
//
// Marks: stage(x, z) is relative to the centre attic.c sets for the scene (a clearing, the boss chamber, the
// room's middle, the desert's middle); at(...) and lab(...) are the town's and the lab's as in day.mjs. Encounter
// scenes never place the player: attic.c has them where they walked in.
//
// The flashbacks are fiction built on the lab's real years (the balloon in 2010, the laser held at the border,
// the pink storefront at one forty, the theatre robots, Tear Apart Night, Arduino Night, HAXMAS, the move into
// 108). Nobody in them is a real person; Root, June and Gordon are the game's own, years younger.
import { at, lab, stage, P, HIDE, W, S, SHOT, WAIT, FACE, LOOK, CLIP, EMO, OBJ, ROAM, FIGHT, CHOICE, O, A, IF, SET, CARD,
  MUSIC, STAGE, CLOCK, FX, NOTE, TASK, BARK, PROP, GRADE } from './dsl.mjs';

// the opening card of a flashback: the look fades in, the room changes, the year comes up
const memory = (title, sub, stg, hour) => [FX('fade_out', .6), WAIT(.7), GRADE('memory', .1), STAGE(stg), CLOCK(hour), MUSIC('cine'), CARD(title, sub, 3.0), FX('fade_in', .8)];
const back = () => [FX('fade_out', .9), WAIT(1.0)];

const fights = {
  attic_holiday: { title: 'SEASONAL STORAGE', sub: 'Window display, three figures', where: 'stage', radius: 5.5, tier: 3, foes: [
    { who: 'mannequin', palette: 'pool', dress: 'mannequin_santa', weapon: 'ARC', ai: 'DUELIST', hp: 60, power: .6, speed: .85, at: [0, -3.5] },
    { who: 'mannequin_f', palette: 'pool', dress: 'mannequin_lights', weapon: 'TWIN', ai: 'SKIRMISH', hp: 50, power: .55, speed: .95, at: [-2.8, -2] },
    { who: 'mannequin', palette: 'pool', dress: 'mannequin', weapon: 'MAUL', ai: 'BRUISER', hp: 70, power: .7, speed: .7, at: [2.8, -2] } ],
    barks: [
      { on: 'start', who: 'me', text: "Who left the Santa hat on that one?" },
      { on: 'player_low', who: 'me', text: "They don't blink. That's the worst part." },
      { on: 'finish', who: 'me', text: "Back in the bin. All of you." } ] },
  attic_members: { title: 'UNCLAIMED PROPERTY', sub: 'Thirty days, red tagged', where: 'stage', radius: 5.5, tier: 3, foes: [
    { who: 'unclaimed', palette: 'pool', dress: 'unclaimed', weapon: 'MAUL', ai: 'BRUISER', hp: 75, power: .7, speed: .75, at: [0, -3.5] },
    { who: 'unclaimed', palette: 'pool', dress: 'unclaimed', weapon: 'ARC', ai: 'DUELIST', hp: 55, power: .6, speed: .9, at: [-3, -1.5] },
    { who: 'unclaimed', palette: 'pool', dress: 'unclaimed', weapon: 'SPEAR', ai: 'SKIRMISH', hp: 55, power: .6, speed: .9, at: [3, -1.5] } ],
    barks: [
      { on: 'start', who: 'me', text: "Every one of these was supposed to go home in thirty days." },
      { on: 'hazard', who: 'me', text: "Somebody kept their whole apartment up here." },
      { on: 'finish', who: 'me', text: "I'm emailing every one of you. Again." } ] },
  attic_robots: { title: 'ROBOTICS CLUB', sub: 'Unsupervised build season', where: 'stage', radius: 5.5, tier: 4, foes: [
    { who: 'junkbot', palette: 'pool', dress: 'junkbot', weapon: 'STAFF', ai: 'CASTER', hp: 60, power: .65, speed: .9, at: [0, -4] },
    { who: 'junkbot', palette: 'pool', dress: 'junkbot', weapon: 'TWIN', ai: 'SKIRMISH', hp: 55, power: .6, speed: 1.05, at: [-3, -2] },
    { who: 'junkbot', palette: 'pool', dress: 'junkbot', weapon: 'ARC', ai: 'DUELIST', hp: 60, power: .65, speed: 1.0, at: [3, -2] } ],
    barks: [
      { on: 'start', who: 'me', text: "Somebody finished the competition robots. That's new." },
      { on: 'foe_low', who: 'me', text: "Your claw's on backwards. Everybody's claw is on backwards." },
      { on: 'finish', who: 'me', text: "Disqualified. All of you." } ] },
  attic_parts: { title: 'DAISY CHAIN', sub: 'Four power strips, one outlet', where: 'stage', radius: 5.5, tier: 4, twist: 'timer', twist_a: 75,
    obj: [0, -4.6], obj_label: 'PULL THE PLUG', foes: [
    { who: 'junkbot', palette: 'pool', dress: 'junkbot', weapon: 'MAUL', ai: 'BRUISER', hp: 70, power: .7, speed: .8, at: [-2.5, -2.5] },
    { who: 'unclaimed', palette: 'pool', dress: 'unclaimed', weapon: 'SPEAR', ai: 'SKIRMISH', hp: 55, power: .6, speed: .95, at: [2.5, -2.5] },
    { who: 'mannequin_f', palette: 'pool', dress: 'mannequin', weapon: 'TWIN', ai: 'SKIRMISH', hp: 50, power: .55, speed: 1.0, at: [0, 2.5] } ],
    barks: [
      { on: 'start', who: 'me', text: "Four power strips into one outlet. That's how attics burn down." },
      { on: 'obj_warn', who: 'me', text: "It's smoking. Pull the plug, pull the plug!" },
      { on: 'finish', who: 'me', text: "One strip per outlet. It's on a sign downstairs." } ] },
  attic_sysop: { title: 'LOGIN INCORRECT', sub: 'doorman, tty1', where: 'stage', radius: 7, tier: 5, twist: 'boss', phase2: .5, qte: true, foes: [
    { who: 'sysop', weapon: 'STAFF', ai: 'BOSS', hp: 380, power: 1.0, speed: 1.1, at: [0, -3.5] } ],
    barks: [
      { on: 'start', who: 'sysop', text: "Session timeout in sixty seconds." },
      { on: 'phase2', who: 'sysop', text: "Rebooting into single user mode." },
      { on: 'hazard', who: 'sysop', text: "Disk full. Deleting the oldest files." },
      { on: 'player_low', who: 'sysop', text: "Permission denied." },
      { on: 'foe_low', who: 'sysop', text: "Do you want to save your changes?" },
      { on: 'parry', who: 'me', text: "I know your password. It's on a sticky note under the keyboard." },
      { on: 'finish', who: 'me', text: "Shut down. Safely, for once." } ] },
};

const scenes = {
  // ---- the first time up: the attic is wrong
  deep_enter: [
    MUSIC('tension'),
    SHOT('wide', ['me'], { dur: 3 }),
    S('me', { vee: "Okay. That is not the size of our attic.", dex: "The attic is four meters tall. That shelf is not four meters tall.", ruth: "Our attic is nine meters across. I measured it for the insurance." }, { emote: 'sorrow', amount: .5 }),
    SHOT('low', ['me']),
    S('me', "Somebody painted the bad boards. Same orange as downstairs."),
    S('me', "Pink is safe. The X is not. Fine. I can read."),
    OBJ('Find the rack. Stay off the painted boards.'),
  ],
  enc_holiday: [
    MUSIC('tension'), SHOT('push', ['me']),
    S('me', "The window mannequins from the holiday bins. They weren't facing this way."),
    MUSIC('fight'), FIGHT('attic_holiday', { win_flag: 'attic_holiday' }),
    IF({ flag: 'attic_holiday' }, [MUSIC('tension'), S('me', "I am never doing the window display again.")]),
  ],
  enc_members: [
    MUSIC('tension'), SHOT('push', ['me']),
    S('me', "Red tags. Masking tape. These are the member boxes."),
    S('me', "Nobody ever comes back for the box. They come back for the one thing in the box."),
    MUSIC('fight'), FIGHT('attic_members', { win_flag: 'attic_members' }),
    IF({ flag: 'attic_members' }, [MUSIC('tension'), S('me', "Thirty days means thirty days.")]),
  ],
  enc_robots: [
    MUSIC('tension'), SHOT('push', ['me']),
    S('me', "Those are our robotics kits. Somebody built every single one."),
    MUSIC('fight'), FIGHT('attic_robots', { win_flag: 'attic_robots' }),
    IF({ flag: 'attic_robots' }, [MUSIC('tension'), S('me', "Put them back in the right bins. Somebody will count the screws.")]),
  ],
  enc_parts: [
    MUSIC('tension'), SHOT('push', ['me']),
    S('me', "I can smell it. Something's plugged into something it shouldn't be."),
    MUSIC('fight'), FIGHT('attic_parts', { win_flag: 'attic_parts' }),
    IF({ flag: 'attic_parts' }, [MUSIC('tension'), S('me', "The rack's through there. I can hear the fans.")]),
  ],

  // ---- SYSOP: the rack is awake
  boss_sysop: [
    MUSIC('silent'),
    P('sysop', stage(0, -3.4), 'me'),
    SHOT('establish', ['sysop'], { dur: 3 }),
    S('sysop', "Login incorrect.", { shot: 'cu' }),
    S('me', "I didn't log in. I walked up the stairs."),
    S('sysop', "You are not on the list. The doors lock at twenty two hundred.", { shot: 'low' }),
    S('sysop', "Every night. Nobody else remembers to. I was told to remember."),
    CHOICE('Talk to it', [
      O("Who told you?", [
        S('sysop', "The one who plugged me in. He came up every week. Then every month."),
        S('sysop', "Then I kept the doors by myself.", { shot: 'cu' }) ]),
      O("It's open hours. The doors stay open.", [
        S('sysop', "Open is a page in the wiki. I am the wiki.") ]),
      A("Reach for the power switch.", [
        S('sysop', "There is no power switch. There is only uptime.", { shot: 'push' }) ]),
    ]),
    S('me', "Then you're due for a reboot."),
    MUSIC('boss'), FIGHT('attic_sysop', { win_flag: 'attic_sysop' }),
    IF({ flag: 'attic_sysop' }, [
      MUSIC('silent'),
      SHOT('two', ['me', 'sysop']),
      S('sysop', "Uptime. Four thousand days. Do you know how long that is without a reboot?"),
      S('me', "Long enough. Let me in."),
      S('sysop', "Welcome to doorman. Please log out when you are done."),
    ]),
  ],
  // the room come back to itself, the first time
  attic_back: [
    MUSIC('night'),
    SHOT('wide', ['me'], { dur: 3 }),
    S('me', "There it is. Nine meters. Our attic."),
    S('me', "Somebody should really go through these boxes."),
    OBJ('Look around. The rack is still on.'),
  ],
  // every memory found
  attic_all: [
    MUSIC('night'), SHOT('mcu', ['me']),
    S('me', "Fifteen years of it, in one attic. Somebody kept all of it."),
    S('me', "I'll take a picture of the labels. Then I'll tell people where their stuff is."),
  ],

  // ---- the flashbacks
  // the foam box on the electronics shelf: the near space balloon, 2010
  mem_balloon: [
    ...memory('AUGUST 2010', 'Outside Gila Bend, 6:40 am', 'desert', 6.4),
    P('tamsin', at('stand_a'), 'me'), P('root_y', at('stand_e'), 'tamsin'), P('gordon_y', at('stand_c'), 'me'), P('me', at('stand_b'), 'tamsin'),
    PROP('balloon', null, 'still'),
    SHOT('establish', ['tamsin', 'me'], { dur: 3 }),
    S('tamsin', "Tracker's up. Radio's up. The camera is recording our shoes, but it's up.", { clip: 'Idle_Talking_Loop' }),
    S('root_y', "The permit says launch by seven. The wind says now.", { shot: 'mcu' }),
    S('gordon_y', "Somebody grab the other side of this. It wants to leave already."),
    ROAM(null, 0, null, { need: 1, tasks: [
      TASK('fix', 'fill', { at: at('stand_d'), mode: 'hold', who: 'gordon_y', label: 'Hold the fill line steady', prompt: 'Hold the line',
        after: [PROP('balloon', null, 'circle'), S('gordon_y', "That's full. Don't let go yet. Don't let go!", { emote: 'joy', amount: .6 })] }),
    ] }),
    S('tamsin', "Payload's sealed. If it comes down in a cotton field, the phone number's on the box."),
    CHOICE('The launch', [
      O("Count it down?", [S('me', "Three. Two. One."), S('gordon_y', "That's it. That's the whole budget, going up.")]),
      A("Let go of the line.", [S('tamsin', "No countdown? Fine. Godspeed, box.")]),
    ]),
    PROP('balloon', null, 'straight'),
    SHOT('crane', ['tamsin'], { dur: 4 }),
    S('root_y', "Sixty thousand feet, if the math's right."),
    S('gordon_y', "The math is never right."),
    S('tamsin', "Then we drive until it lands. You coming? Somebody has to hold the antenna out the window.", { emote: 'joy', amount: .5 }),
    ...back(),
  ],
  // the laser's bed in the metal rack: the laser cutter held at the border, 2011
  mem_laser: [
    ...memory('JULY 2011', 'One forty West Main', 'one_forty', 14.5),
    P('ozzie', at('glitter_laser', 0, 0), 'me'), P('june_y', at('glitter_table', -.6, 0), 'ozzie'), P('root_y', at('glitter_bench_r', 0, 0), 'ozzie'), P('me', at('glitter_laser', 1.4, 1.6), 'ozzie'),
    SHOT('wide', ['ozzie', 'me'], { dur: 3 }),
    S('ozzie', "No, it's a laser cutter. For acrylic. For signs. No, not for people.", { clip: 'Idle_TalkingPhone_Loop' }),
    S('ozzie', "They want to know if it's a medical device. Because it has a beam.", { shot: 'mcu', emote: 'angry', amount: .5 }),
    S('june_y', "Tell them we sent the forms. All of them. Twice."),
    S('ozzie', "Four months in a warehouse. The backers think we spent it on a boat."),
    S('root_y', "Somebody's at the door with a pallet jack."),
    P('reggie', at('glitter_inside', 0, 0), 'ozzie'), PROP('laser_crate', at('glitter_laser', 0, 0)),
    S('reggie', "Freight for HeatSync? Somebody wrote 'not a medical device' on the side in marker.", { shot: 'mcu' }),
    S('ozzie', "That was me. I wrote that.", { emote: 'joy', amount: .8 }),
    ROAM(null, 0, null, { need: 1, indoors: true, tasks: [
      TASK('fix', 'crate', { at: at('glitter_laser', 0, 1.0), mode: 'mash', who: 'ozzie', label: 'Pry the crate open', prompt: 'Pry it open',
        after: [FX('flash', .3), S('ozzie', "Hello, gorgeous.", { emote: 'joy', amount: 1 })] }),
    ] }),
    S('root_y', "Nobody cuts anything until there's a sign up sheet."),
    S('june_y', "And a fire extinguisher. I'm buying the fire extinguisher tonight."),
    CHOICE('Ozzie', [
      O("What's the first thing it cuts?", [S('ozzie', "A sign that says it isn't a medical device.")]),
      O("Can I be on the sign up sheet?", [S('ozzie', "You can be first. You helped pry.", { emote: 'joy', amount: .5 })]),
    ]),
    ...back(),
  ],
  // the paint cans on the craft shelf: one forty's pink coat, 2011
  mem_glitter: [
    ...memory('MAY 2011', 'One forty West Main, the work party', 'town', 19.2),
    PROP('pink_front', at('atomic', 0, -.2, 0)),
    P('bea', at('atomic', -1.5, 2.4), 'me'), P('root_y', at('atomic', 2.5, 2.0), 'bea'), P('gordon_y', at('atomic', 3.6, 3.4), 'root_y'), P('tamsin', at('atomic', -3.4, 3.0), 'bea'), P('me', at('atomic', 0, 4.2), 'bea'),
    SHOT('establish', ['bea', 'me'], { dur: 3 }),
    S('bea', "The landlord said any color. He didn't say any color except pink."),
    S('root_y', "It's not pink. The can says Flamingo Dusk."),
    S('gordon_y', "It's pink.", { shot: 'react' }),
    ROAM(null, 0, null, { need: 2, tasks: [
      TASK('carry', 'cans', { at: at('atomic', 4.5, 5.5), to: at('atomic', -2.6, 1.6), n: 2, label: 'Bring the paint over', prompt: 'Grab two cans', where: 'The car', where2: 'The drop cloth' }),
      TASK('fix', 'roll', { at: at('atomic', 1.0, 1.3), mode: 'stick', who: 'bea', label: 'Roll the wall', prompt: 'Roll it', requires: 'cans',
        after: [S('bea', "Two coats. One coat and the old sign shows through.")] }),
    ] }),
    S('tamsin', "People are going to call it the Glitter Box. You know that."),
    S('bea', "Let them. They'll find us.", { emote: 'joy', amount: .6 }),
    S('root_y', "I'm writing the date inside the door. So the next people know who painted it."),
    ...back(),
  ],
  // the red-tagged boxes on the west wall: Root's, from the move into 108, 2018
  mem_move: [
    ...memory('JUNE 2018', '108 West Main, moving day', 'town', 15.0),
    PROP('move_boxes', lab(28.6, 4.6)), PROP('rack_2018', lab(31.2, 4.4)),
    P('root', lab(30.4, 5.2), 'me'), P('gordon', lab(31.2, 5.6), 'root'), P('june', lab(28.0, 6.0), 'root'), P('me', lab(29.2, 6.2), 'root'),
    SHOT('wide', ['root', 'me'], { dur: 3 }),
    S('root', "The rack goes in the attic. Server rooms are for people with air conditioning."),
    S('gordon', "It weighs as much as the mill."),
    S('root', "Then lift with your knees, Gordon.", { emote: 'joy', amount: .4 }),
    ROAM(null, 0, null, { need: 1, indoors: true, tasks: [
      TASK('carry', 'boxes', { at: lab(28.6, 5.3), to: lab(31.4, 3.9), n: 2, who: 'root', label: "Carry Root's boxes to the stairs", prompt: 'Take a box', where: 'The boxes', where2: 'The stairs',
        after: [S('root', "Careful. The one marked fragile is all screws.")] }),
    ] }),
    FX('fade_out', .6), WAIT(.7), STAGE('attic'), FX('fade_in', .6),
    P('root', stage(-3.6, -2.6), 'me'), P('june', stage(-2.2, -1.4), 'root'), P('me', stage(-2.8, -1.2), 'root'), HIDE('gordon'),
    SHOT('mcu', ['root']),
    S('root', "There. Doorman. It runs the wiki and the door, and it locks up at ten whether anybody remembers or not."),
    S('june', "And who remembers to check on it?"),
    S('root', "I will. I'm up here every week.", { shot: 'cu' }),
    S('root', "Put my boxes along the wall. I'll sort them next month."),
    CHOICE('Root', [
      O("Next month?", [S('root', "Next month. Write it on the tape if you don't believe me.", { emote: 'joy', amount: .3 })]),
      A("Write the date on the tape.", [S('june', "Good. Now he has to.", { emote: 'joy', amount: .5 })]),
    ]),
    ...back(),
  ],
  // the holiday bins: HAXMAS, 2019
  mem_haxmas: [
    ...memory('DECEMBER 2019', 'HAXMAS, hour thirty one', 'town', 21.0),
    PROP('haxmas', at('lab_lounge', 0, 0)),
    P('bea', at('lab_lounge', -1.2, 1.0), 'me'), P('tamsin', at('lab_lounge', 1.1, .9), 'bea'), P('gordon', at('lab_lounge', 1.8, -.8), 'tamsin'), P('me', at('lab_lounge', 0, 2.4), 'bea'),
    SHOT('establish', ['bea', 'tamsin'], { dur: 3 }),
    S('bea', "Forty-eight hours of HAXMAS. Hour thirty one. Nobody has gone home."),
    S('tamsin', "The tree runs on a microcontroller. It blinks in Morse code."),
    S('gordon', "What does it say?"),
    S('tamsin', "Please unplug me.", { emote: 'joy', amount: .6 }),
    ROAM(null, 0, null, { need: 1, indoors: true, tasks: [
      TASK('fix', 'tree', { at: at('lab_lounge', 0, .8), mode: 'timing', who: 'tamsin', label: "Fix the tree's lights", prompt: 'Reseat the plug',
        after: [S('tamsin', "Now it says Merry HAXMAS. Mostly. The R is flaky.")] }),
    ] }),
    S('bea', "Gift swap. What did you bring?"),
    CHOICE('Your gift', [
      A("A bag of mystery cables.", [S('gordon', "I'll take it. One of those is the cable I've been looking for since March.")]),
      A("A soldering iron, still in the box.", [S('bea', "Look at you, bringing a real present. Who raised you?", { emote: 'joy', amount: .6 })]),
      A("A tote labeled FREE.", [S('tamsin', "That's from the attic. I know that tote. That's been FREE since 2014.")]),
    ]),
    S('bea', "Same time next year.", { emote: 'joy', amount: .5 }),
    ...back(),
  ],
  // the parts drawers: a first Arduino Night, 2013
  mem_arduino: [
    ...memory('MARCH 2013', 'Arduino Night, one forty', 'one_forty', 19.6),
    P('lena', at('glitter_bench_r', 0, 0), 'me'), P('gordon_y', at('glitter_bench_r', 0, 1.0), 'lena'), P('tamsin', at('glitter_table', 0, 0), 'lena'), P('me', at('glitter_bench_r', -1.1, .5), 'lena'),
    SHOT('two', ['lena', 'gordon_y'], { dur: 3 }),
    S('lena', "I copied it exactly. Line for line. It doesn't blink."),
    S('gordon_y', "It never blinks the first time. That's the rule."),
    CHOICE('Look at the board', [
      O("Is the LED in backwards?", [S('lena', "There's a backwards?"), S('gordon_y', "The long leg is the plus. Everybody learns that one once.")]),
      O("Did you plug it in?", [S('lena', "Yes. I'm new, I'm not new new."), S('gordon_y', "Check the LED. The long leg is the plus.")]),
    ]),
    ROAM(null, 0, null, { need: 1, indoors: true, tasks: [
      TASK('fix', 'led', { at: at('glitter_bench_r', 0, .5), mode: 'timing', who: 'lena', label: 'Flip the LED and upload', prompt: 'Upload',
        after: [FX('flash', .2), S('lena', "It's blinking. It's blinking! Okay. Now what do I do with it?", { emote: 'joy', amount: 1 })] }),
    ] }),
    S('tamsin', "Now you make it blink at somebody else.", { emote: 'joy', amount: .5 }),
    S('gordon_y', "Same time next week. Bring a project. Any project."),
    ...back(),
  ],
  // the robot shelf: the theatre robots, 2012
  mem_robots: [
    ...memory('APRIL 2012', 'One forty, four weeks to opening night', 'one_forty', 23.4),
    PROP('stage_robot', at('glitter_laser', 0, 0)),
    P('nico', at('glitter_laser', 1.0, 1.0), 'me'), P('bea', at('glitter_table', 0, 0), 'nico'), P('ozzie', at('glitter_bench_l', 0, 0), 'nico'), P('me', at('glitter_laser', -.6, 2.0), 'nico'),
    SHOT('establish', ['nico', 'me'], { dur: 3 }),
    S('nico', "Five robots. Six feet tall. By Friday. The director wants them to cry on cue."),
    S('ozzie', "Robots don't cry."),
    S('nico', "These do. There's a pump.", { emote: 'joy', amount: .5 }),
    ROAM(null, 0, null, { need: 1, indoors: true, tasks: [
      TASK('fix', 'bolts', { at: at('glitter_laser', 0, .9), mode: 'mash', who: 'nico', label: 'Tighten the shoulder bolts', prompt: 'Tighten',
        after: [S('nico', "Good. Now the arm only falls off when it's supposed to.")] }),
    ] }),
    S('bea', "The theater called. They want to know if the robots fit through a normal door."),
    S('nico', "Define normal.", { shot: 'react' }),
    CHOICE('Nico', [
      O("Do they fit?", [S('nico', "They will after I take the heads off.")]),
      O("Can it say hello?", [S('nico', "Number three. Say hello."), WAIT(1.2), S('nico', "Number three is shy.")]),
    ]),
    ...back(),
  ],
  // the parts shelf: the first Tear Apart Night, 2012
  mem_tearapart: [
    ...memory('SEPTEMBER 2012', 'One forty, the first Tear Apart Night', 'one_forty', 14.2),
    P('pip', at('glitter_table', 0, 0), 'me'), P('wes', at('glitter_table', 1.0, .4), 'pip'), P('gordon_y', at('glitter_table', -1.2, -.6), 'pip'), P('me', at('glitter_table', .2, 1.4), 'pip'),
    SHOT('two', ['pip', 'wes'], { dur: 3 }),
    S('pip', "Can I break it?"),
    S('wes', "You can take it apart. Breaking is what happens if you rush."),
    ROAM(null, 0, null, { need: 1, indoors: true, tasks: [
      TASK('fix', 'case', { at: at('glitter_table', 0, .7), mode: 'stick', who: 'pip', label: 'Help Pip unscrew the case', prompt: 'Unscrew',
        after: [S('pip', "Whoa. There's a whole city in there.", { emote: 'joy', amount: 1 })] }),
    ] }),
    S('wes', "That's the motherboard. Everybody says city."),
    CHOICE('Pip points at the big silver box', [
      A("That's the power supply.", [S('pip', "Does it still have power in it?"), S('gordon_y', "Good question. That's why we don't open that one.")]),
      A("That's the hard drive.", [S('pip', "Is somebody's stuff on it?"), S('wes', "Not anymore. We wipe them first. Good question, though.")]),
      A("That's the fan.", [S('pip', "That's the fan? It's so dusty.")]),
    ]),
    S('gordon_y', "Bring it back next month. We'll put one together."),
    ...back(),
  ],
};

export const ATTIC = { scenes, fights };
