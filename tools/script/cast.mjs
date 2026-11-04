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
  lim:    { name: 'Inspector Lim', model: 'S', voice: 'af_sarah', speed: 1.0, scale: 1.0, color: 0xc8d4e0,
            pal: { hair: hue(190, .5, .55), top: hue(190, .35, .7), bottom: hue(210, .5, .45), shoes: hue(0, .5, .6) } },
  rennick:{ name: 'Rennick', model: 'F', voice: 'am_liam', speed: 1.1, scale: 1.0, color: 0xa8d060,
            pal: { hair: hue(60, 1.3, 1.2), top: hue(90, 1.3, .9), bottom: hue(40, .9, .7), shoes: hue(0) } },
  dobbs:  { name: 'Dobbs', model: 'D', voice: 'am_onyx', speed: 1.0, scale: 1.08, color: 0xb09060,
            pal: { hair: hue(20, 1.2, 1.1), top: hue(30, 1.3, 1.1), bottom: hue(25, .6, .7), shoes: hue(0) } },
  champ:  { name: 'High Score', model: 'C', voice: 'af_nova', speed: 1.1, scale: 1.0, color: 0x60e0e0,
            pal: { hair: hue(180, 1.2, 1.1), top: hue(120, 1.2, 1), bottom: hue(150, 1, 1), shoes: hue(0) } },
  bex:    { name: 'Bex', model: 'C', voice: 'af_nicole', speed: 1.02, scale: 1.0, color: 0x90d8f0,
            pal: { hair: hue(-70, .7, 1.25), top: hue(200, .7, 1.1), bottom: hue(-120, .6, 1.1), shoes: hue(0) } },
  cal:    { name: 'Copper Cal', model: 'D', voice: 'am_adam', speed: 1.0, scale: 1.1, color: 0xd07030,
            pal: { hair: hue(15, 1.5, 1.2), top: hue(20, 1.5, 1.3), bottom: hue(15, .8, .7), shoes: hue(0) } },
  root:   { name: 'Root', model: 'F', voice: 'bm_george', speed: .98, scale: 1.04, color: 0x88a0a8,
            pal: { hair: hue(0, .1, 1.6), top: hue(200, .4, .7), bottom: hue(200, .4, .6), shoes: hue(0) } },
  brant:  { name: 'Brant Holloway', model: 'D', voice: 'am_eric', speed: 1.08, scale: 1.02, color: 0xe8c860,
            pal: { hair: hue(30, 1.2, 1.5), top: hue(45, .4, 2.2), bottom: hue(45, .4, 2.0), shoes: hue(40, .6, 1.3) } },
  intern: { name: 'Intern', model: 'S', voice: 'af_sky', speed: 1.1, scale: .98, color: 0xd8d8d8,
            pal: { hair: hue(0, .4, .9), top: hue(45, .4, 1.1), bottom: hue(0, .2, .6), shoes: hue(0) } },
  crew:   { name: 'Scrapper', model: 'D', voice: 'am_adam', speed: 1.0, scale: 1.0, color: 0xa06030,
            pal: { hair: hue(10, .6, .7), top: hue(15, 1.0, .7), bottom: hue(10, .6, .5), shoes: hue(0) } },
};
// who the three playable members are. Choosing one makes the other two m1 and m2.
export const PLAYABLE = ['vee', 'dex', 'ruth'];
export const ROLES = { vee: { m1: 'dex', m2: 'ruth' }, dex: { m1: 'vee', m2: 'ruth' }, ruth: { m1: 'vee', m2: 'dex' } };
