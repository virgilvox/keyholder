// The cast. Voices are Kokoro 82M, the same model and the same six voices as DOLLY for its six characters.
const hue = (hue, sat = 1, val = 1) => ({ hue, sat, val });
export const NONE = { hair: hue(0), top: hue(0), bottom: hue(0), shoes: hue(0) };
export const CAST = {
  vee:    { name: 'Vee', model: 'C', voice: 'af_heart', speed: 1.06, scale: 1.0, color: 0xf2ab1e, pal: NONE },
  dex:    { name: 'Dex', model: 'D', voice: 'am_michael', speed: 1.05, scale: 1.0, color: 0x6cb4ee, pal: NONE },
  ruth:   { name: 'Ruth', model: 'B', voice: 'bf_emma', speed: 1.02, scale: 1.0, color: 0xf07a52, pal: NONE },
  sam:    { name: 'Sam', model: 'S', voice: 'af_bella', speed: 1.02, scale: 0.98, color: 0x9bd46e, pal: NONE },
  kai:    { name: 'Kai', model: 'F', voice: 'am_puck', speed: 1.05, scale: 0.97, color: 0xc9a0ff, pal: NONE },
  morgan: { name: 'Morgan', model: 'B', voice: 'af_kore', speed: 1.02, scale: 1.0, color: 0x5fd3c8,
            pal: { hair: hue(-40, 1.1, 1.15), top: hue(150, 1.2, .9), bottom: hue(190, .8, .8), shoes: hue(0) } },
  // members around the lab
  napper: { name: 'Lou', model: 'D', voice: 'am_echo', speed: 1.0, scale: 1.0, color: 0xc0b080,
            pal: { hair: hue(40, .8, 1.3), top: hue(100, .6, 1.2), bottom: hue(30, .7, .9), shoes: hue(0) } },
  regular: { name: 'Pat', model: 'B', voice: 'af_river', speed: 1.04, scale: 1.0, color: 0x80c8a0,
            pal: { hair: hue(90, .6, 1.1), top: hue(60, .9, 1), bottom: hue(100, .6, .9), shoes: hue(0) } },
  haunter: { name: 'Jo', model: 'S', voice: 'af_aoede', speed: 1.06, scale: 1.0, color: 0xd080e0,
            pal: { hair: hue(260, 1.2, .7), top: hue(250, .8, .5), bottom: hue(260, .8, .4), shoes: hue(0) } },
  // the people who walk in today
  vic:    { name: 'Vinyl Vic', model: 'F', voice: 'am_fenrir', speed: 1.05, scale: 1.0, color: 0xe05a9c,
            pal: { hair: hue(300, 1.2, 1), top: hue(280, 1.4, 1), bottom: hue(250, 1, .8), shoes: hue(0) } },
  lim:    { name: 'Inspector Lim', model: 'W', voice: 'af_sarah', speed: 1.0, scale: 1.0, color: 0xc8d4e0,
            pal: NONE },
  rennick:{ name: 'Rennick', model: 'H', voice: 'am_liam', speed: 1.1, scale: 1.0, color: 0xa8d060,
            pal: NONE },
  dobbs:  { name: 'Dobbs', model: 'D', voice: 'am_onyx', speed: 1.0, scale: 1.08, color: 0xb09060,
            pal: { hair: hue(20, 1.2, 1.1), top: hue(30, 1.3, 1.1), bottom: hue(25, .6, .7), shoes: hue(0) } },
  champ:  { name: 'High Score', model: 'V', voice: 'af_nova', speed: 1.1, scale: 1.0, color: 0x60e0e0,
            pal: NONE },
  bex:    { name: 'Bex', model: 'C', voice: 'af_nicole', speed: 1.02, scale: 1.0, color: 0x90d8f0,
            pal: { hair: hue(-70, .7, 1.25), top: hue(200, .7, 1.1), bottom: hue(-120, .6, 1.1), shoes: hue(0) } },
  cal:    { name: 'Copper Cal', model: 'D', voice: 'am_adam', speed: 1.0, scale: 1.1, color: 0xd07030,
            pal: { hair: hue(15, 1.5, 1.2), top: hue(20, 1.5, 1.3), bottom: hue(15, .8, .7), shoes: hue(0) } },
  root:   { name: 'Root', model: 'O', voice: 'bm_george', speed: .98, scale: 1.04, color: 0x88a0a8,
            pal: NONE },
  brant:  { name: 'Brant Holloway', model: 'M', voice: 'am_eric', speed: 1.08, scale: 1.02, color: 0xe8c860,
            pal: { hair: hue(-145, .9, 1.35), top: hue(0), bottom: hue(0), shoes: hue(0) } },
  // v1.1: more people. Each has a small beat in the day.
  wes:    { name: 'Wes', model: 'M', voice: 'bm_fable', speed: 1.04, scale: 1.0, color: 0x7fa8d8,     // seventh grade science teacher
            pal: NONE },
  hal:    { name: 'Hal', model: 'P', voice: 'am_santa', speed: .96, scale: 1.02, color: 0xb8b0a0,   // retired machinist
            pal: { hair: hue(0, .1, 1.4), top: hue(0), bottom: hue(0), shoes: hue(0) } },
  reggie: { name: 'Reggie', model: 'U', voice: 'bm_daniel', speed: 1.08, scale: 1.0, color: 0x8c6a3c, // delivery driver
            pal: { hair: hue(0), top: hue(0, 1, .55), bottom: hue(0), shoes: hue(0) } },
  dana:   { name: 'Dana', model: 'G', voice: 'af_jessica', speed: 1.06, scale: 1.0, color: 0xe0a0b0,  // a parent at Tech Tear Apart
            pal: NONE },
  milo:   { name: 'Milo', model: 'E', voice: 'af_alloy', speed: 1.12, scale: .86, color: 0xf0d050,     // Dana's kid
            pal: NONE },
  ines:   { name: 'Ines', model: 'K', voice: 'bf_isabella', speed: 1.06, scale: .98, color: 0x70c0a0, // a student with a robot
            pal: NONE },
  gloria: { name: 'Gloria', model: 'R', voice: 'bf_alice', speed: .98, scale: .97, color: 0xc8a0e0,   // a neighbor from the apartments
            pal: NONE },
  intern: { name: 'Intern', model: 'S', voice: 'af_sky', speed: 1.1, scale: .98, color: 0xd8d8d8,
            pal: { hair: hue(0, .4, .9), top: hue(45, .4, 1.1), bottom: hue(0, .2, .6), shoes: hue(0) } },
  crew:   { name: 'Scrapper', model: 'H', voice: 'am_adam', speed: 1.0, scale: 1.0, color: 0xa06030,
            pal: { hair: hue(20, .8, .8), top: hue(0, 1, .5), bottom: hue(0), shoes: hue(0) } },
};
// who the three playable members are. Choosing one makes the other two m1 and m2.
export const PLAYABLE = ['vee', 'dex', 'ruth'];
export const ROLES = { vee: { m1: 'dex', m2: 'ruth' }, dex: { m1: 'vee', m2: 'ruth' }, ruth: { m1: 'vee', m2: 'dex' } };
