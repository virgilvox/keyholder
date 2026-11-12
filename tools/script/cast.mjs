// The cast. Voices are Kokoro 82M, the same model and the same six voices as DOLLY for its six characters.
const hue = (hue, sat = 1, val = 1) => ({ hue, sat, val });
export const NONE = { hair: hue(0), top: hue(0), bottom: hue(0), shoes: hue(0) };
export const CAST = {
  vee:    { name: 'Vee', model: 'C', voice: 'af_heart', speed: 1.06, scale: 1.0, color: 0xf2ab1e, pal: NONE },
  dex:    { name: 'Dex', model: 'D', voice: 'am_michael', speed: 1.05, scale: 1.0, color: 0x6cb4ee, pal: NONE },
  ruth:   { name: 'Ruth', model: 'B', voice: 'bf_emma', speed: 1.02, scale: 1.0, color: 0xf07a52, pal: NONE },
  sam:    { name: 'Sam', model: 'S', voice: 'af_bella', speed: 1.02, scale: 0.98, color: 0x9bd46e, pal: NONE },
  kai:    { name: 'Kai', model: 'F', voice: 'am_puck', speed: 1.05, scale: 0.97, color: 0xc9a0ff, pal: NONE },
  morgan: { name: 'Morgan', model: 'T', voice: 'af_kore', speed: 1.02, scale: 1.0, color: 0x5fd3c8,
            pal: NONE },
  // members around the lab
  napper: { name: 'Lou', model: 'J', voice: 'am_echo', speed: 1.0, scale: 1.0, color: 0xc0b080,
            pal: NONE },
  regular: { name: 'Pat', model: 'I', voice: 'af_river', speed: 1.04, scale: 1.0, color: 0x80c8a0,
            pal: NONE },
  haunter: { name: 'Jo', model: 'A', voice: 'af_aoede', speed: 1.06, scale: 1.0, color: 0xd080e0,
            pal: { hair: hue(260, 1.2, .7), top: hue(250, .8, .5), bottom: hue(260, .8, .4), shoes: hue(0) } },
  // the people who walk in today
  vic:    { name: 'Vinyl Vic', model: 'E2', voice: 'am_fenrir', speed: 1.05, scale: 1.0, color: 0xe05a9c,
            pal: NONE },
  lim:    { name: 'Inspector Lim', model: 'X', voice: 'af_sarah', speed: 1.0, scale: 1.0, color: 0xc8d4e0,
            pal: NONE },
  rennick:{ name: 'Rennick', model: 'H', voice: 'am_liam', speed: 1.1, scale: 1.0, color: 0xa8d060,
            pal: NONE },
  dobbs:  { name: 'Dobbs', model: 'B2', voice: 'am_onyx', speed: 1.0, scale: 1.08, color: 0xb09060,
            pal: NONE },
  champ:  { name: 'High Score', model: 'Y', voice: 'af_nova', speed: 1.1, scale: 1.0, color: 0x60e0e0,
            pal: NONE },
  bex:    { name: 'Bex', model: 'Z', voice: 'af_nicole', speed: 1.02, scale: 1.0, color: 0x90d8f0,
            pal: NONE },
  cal:    { name: 'Copper Cal', model: 'C2', voice: 'am_adam', speed: 1.0, scale: 1.1, color: 0xd07030,
            pal: NONE },
  root:   { name: 'Root', model: 'O', voice: 'bm_george', speed: .98, scale: 1.04, color: 0x88a0a8,
            pal: NONE },
  brant:  { name: 'Brant Holloway', model: 'G2', voice: 'am_eric', speed: 1.08, scale: 1.02, color: 0xe8c860,
            pal: NONE },
  // v1.1: more people. Each has a small beat in the day.
  wes:    { name: 'Wes', model: 'F2', voice: 'bm_fable', speed: 1.04, scale: 1.0, color: 0x7fa8d8,     // seventh grade science teacher
            pal: NONE },
  hal:    { name: 'Hal', model: 'P', voice: 'am_santa', speed: .96, scale: 1.02, color: 0xb8b0a0,   // retired machinist
            pal: { hair: hue(0, .1, 1.4), top: hue(0), bottom: hue(0), shoes: hue(0) } },
  reggie: { name: 'Reggie', model: 'U', voice: 'bm_daniel', speed: 1.08, scale: 1.0, color: 0x8c6a3c, // delivery driver
            pal: { hair: hue(0), top: hue(0, 1, .55), bottom: hue(0), shoes: hue(0) } },
  dana:   { name: 'Dana', model: 'G', voice: 'af_jessica', speed: 1.06, scale: 1.0, color: 0xe0a0b0,  // a parent at Tech Tear Apart
            pal: NONE },
  milo:   { name: 'Milo', model: 'N', voice: 'af_alloy', speed: 1.12, scale: .92, color: 0xf0d050,     // Dana's kid
            pal: NONE },
  ines:   { name: 'Ines', model: 'K', voice: 'bf_isabella', speed: 1.06, scale: .98, color: 0x70c0a0, // a student with a robot
            pal: NONE },
  gloria: { name: 'Gloria', model: 'L', voice: 'bf_alice', speed: .98, scale: .97, color: 0xc8a0e0,   // a neighbor from the apartments
            pal: NONE },
  intern: { name: 'Intern', model: 'A2', voice: 'af_sky', speed: 1.1, scale: .98, color: 0xd8d8d8,
            pal: NONE },
  crew:   { name: 'Scrapper', model: 'D2', voice: 'am_adam', speed: 1.0, scale: 1.0, color: 0xa06030,
            pal: NONE },
  // v1.3: the nights. Bodies nobody in the day wears (Q, R, and the two the day's walk-ins used, M and W; the nights
  // name everyone who walks in, so no walk-in takes them there)
  marisol:{ name: 'Marisol', model: 'Q', voice: 'af_nova', speed: 1.06, scale: .99, color: 0xf06090,  // three weeks a member, does things
            pal: NONE },
  june:   { name: 'June', model: 'R', voice: 'bf_lily', speed: .94, scale: .97, color: 0xb0c8a0,      // the Treasurer, nine years of forms
            pal: { hair: hue(0), top: hue(140, .3, .7), bottom: hue(0, .35, .55), shoes: hue(0) } },   /* the body's school vest, muted to a cardigan */
  gordon: { name: 'Gordon', model: 'M', voice: 'bm_lewis', speed: 1.0, scale: 1.03, color: 0x9090c0,  // twelve years at the bench
            pal: { hair: hue(0, .4, .75), top: hue(200, .6, .7), bottom: hue(0), shoes: hue(0) } },
  del:    { name: 'Del', model: 'W', voice: 'af_sky', speed: 1.0, scale: .98, color: 0xe8b060,       // first night, a board that won't take solder
            pal: { hair: hue(20, 1.1, .8), top: hue(150, .8, .9), bottom: hue(220, .6, .7), shoes: hue(0) } },
  // v2.0: the attic. The nightmare's foes (pool bodies, washed one colour all over by actor.h's palette wash, dressed
  // by enemy.c), and the people in the flashbacks: years-younger Root, June and Gordon, and members of those years.
  mannequin: { name: 'Mannequin', model: 'D2', voice: 'am_echo', speed: .9, scale: 1.0, color: 0xe8e0d4, pal: { ...NONE, wash: 0xe8e0d4 } },
  mannequin_f: { name: 'Mannequin', model: 'X', voice: 'am_echo', speed: .9, scale: .98, color: 0xe8e0d4, pal: { ...NONE, wash: 0xe4dcd0 } },
  unclaimed: { name: 'Unclaimed', model: 'N2', voice: 'am_echo', speed: .85, scale: 1.02, color: 0x8a8e94, pal: { ...NONE, wash: 0x8a8e94 } },
  junkbot: { name: 'Junkbot', model: 'Y', voice: 'am_echo', speed: 1.0, scale: 1.0, color: 0xa8b0b8, pal: { ...NONE, wash: 0xa8b0b8 } },
  sysop:  { name: 'SYSOP', model: 'E2', voice: 'am_onyx', speed: .84, scale: 1.06, color: 0x40ff8a, pal: { ...NONE, wash: 0x26302a }, dress: 'sysop' },
  root_y: { name: 'Root', model: 'O', voice: 'bm_george', speed: 1.0, scale: 1.04, color: 0x88a0a8,
            pal: { hair: hue(20, .7, .42), top: hue(0), bottom: hue(0), shoes: hue(0) } },
  june_y: { name: 'June', model: 'R', voice: 'bf_lily', speed: .98, scale: .97, color: 0xb0c8a0,
            pal: { hair: hue(25, 1.4, .5), top: hue(330, .5, .8), bottom: hue(0, .35, .55), shoes: hue(0) } },
  gordon_y: { name: 'Gordon', model: 'M', voice: 'bm_lewis', speed: 1.02, scale: 1.03, color: 0x9090c0,
            pal: { hair: hue(0, .6, .45), top: hue(30, .6, .7), bottom: hue(0), shoes: hue(0) } },
  tamsin: { name: 'Tamsin', model: 'I2', voice: 'af_river', speed: 1.08, scale: 1.0, color: 0xe0a050, pal: NONE },   // radio, the balloon
  ozzie:  { name: 'Ozzie', model: 'J2', voice: 'am_puck', speed: 1.02, scale: 1.0, color: 0xd05050, pal: NONE },    // the laser's champion
  bea:    { name: 'Bea', model: 'O2', voice: 'af_kore', speed: 1.07, scale: .99, color: 0xe070a0, pal: NONE },      // ran the work parties
  nico:   { name: 'Nico', model: 'P2', voice: 'am_liam', speed: 1.0, scale: 1.0, color: 0x60b070, pal: NONE },      // the theatre robots
  pip:    { name: 'Pip', model: 'E', voice: 'af_sky', speed: 1.16, scale: .9, color: 0xf0c040, pal: NONE },        // a kid at the first Tear Apart
  lena:   { name: 'Lena', model: 'K2', voice: 'af_nicole', speed: 1.06, scale: .99, color: 0xf09040, pal: NONE },   // first Arduino Night
};
// who the three playable members are. Choosing one makes the other two m1 and m2.
export const PLAYABLE = ['vee', 'dex', 'ruth'];
export const ROLES = { vee: { m1: 'dex', m2: 'ruth' }, dex: { m1: 'vee', m2: 'ruth' }, ruth: { m1: 'vee', m2: 'dex' } };
