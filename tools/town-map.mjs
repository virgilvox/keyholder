// A top-down plan of the town for placing things: ground, footprints, props, named places and the
// hand-placed set pieces of src/facades_town.inc, with a 10 m grid. Read it, then add a line to a table.
//
//   node tools/town-map.mjs <out.png> [x0 z0 x1 z1] [px per metre]
//
// World axes as in world.h: +x east (right), -z north (up). Reads src/gen/city_data.h and
// src/facades_town.inc, so it shows what the build will place.
import sharp from 'sharp';
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
const [out, ...rest] = process.argv.slice(2);
if (!out) { console.error('usage: node tools/town-map.mjs out.png [x0 z0 x1 z1] [px per metre]'); process.exit(2); }
const [x0, z0, x1, z1] = rest.length >= 4 ? rest.slice(0, 4).map(Number) : [-345, -232, 470, 172];
const S = rest.length >= 5 ? Number(rest[4]) : 2;
const W = Math.round((x1 - x0) * S), H = Math.round((z1 - z0) * S);
const px = (x) => ((x - x0) * S).toFixed(1), pz = (z) => ((z - z0) * S).toFixed(1);

const h = readFileSync(join(ROOT, 'src/gen/city_data.h'), 'utf8');
const arr = (name) => { const m = h.match(new RegExp(name + '\\[\\][^=]*=\\s*\\{([\\s\\S]*?)\\};')); return m ? m[1] : ''; };
const nums = (s) => s.match(/-?\d+(\.\d+)?/g).map(Number);
const rows = (s) => [...s.matchAll(/\{([^{}]*)\}/g)].map((m) => m[1]);

let svg = `<svg xmlns="http://www.w3.org/2000/svg" width="${W}" height="${H}"><rect width="${W}" height="${H}" fill="#2a2a2a"/>`;
const GC = ['#3b3b40', '#b9a48a', '#9c9a94', '#55555a', '#6d7a4f', '#8c8068'];
for (const r of rows(arr('CITY_GROUND'))) { const [a, b, c, d, , t] = nums(r); svg += `<rect x="${px(a)}" y="${pz(b)}" width="${((c - a) * S).toFixed(1)}" height="${((d - b) * S).toFixed(1)}" fill="${GC[t] || '#f0f'}"/>`; }
// footprints
const B = rows(arr('CITY_BUILDING')).map(nums), P = nums(arr('CITY_PTS'));
const SC = ['#c8a77e', '#e6e1d6', '#a5583f', '#cbbf9e', '#7d82c8', '#a9a6a0'];
for (const [first, n, , , ht, st] of B) {
  const pts = []; for (let i = 0; i < n; i++) pts.push(`${px(P[(first + i) * 2])},${pz(P[(first + i) * 2 + 1])}`);
  svg += `<polygon points="${pts.join(' ')}" fill="${SC[st] || '#888'}" fill-opacity=".85" stroke="#111" stroke-width="1"/>`;
  void ht;
}
// grid
for (let x = Math.ceil(x0 / 10) * 10; x <= x1; x += 10) svg += `<line x1="${px(x)}" y1="0" x2="${px(x)}" y2="${H}" stroke="${x % 50 ? '#ffffff22' : '#ffffff66'}"/>` + (x % 50 ? '' : `<text x="${+px(x) + 2}" y="12" fill="#fff" font-size="11">${x}</text>`);
for (let z = Math.ceil(z0 / 10) * 10; z <= z1; z += 10) svg += `<line x1="0" y1="${pz(z)}" x2="${W}" y2="${pz(z)}" stroke="${z % 50 ? '#ffffff22' : '#ffffff66'}"/>` + (z % 50 ? '' : `<text x="2" y="${+pz(z) - 2}" fill="#fff" font-size="11">${z}</text>`);
// props
const PC = ['#6cf', '#ff6', '#4c4', '#2a2', '#963', '#e44', '#585', '#f80', '#ccc', '#0aa', '#555', '#a72', '#a95'];
for (const r of rows(arr('CITY_PROP'))) {
  const [t, x, z, yaw] = nums(r);
  // cars as their footprint (4.6 by 1.9 m, length along local x turned by yaw), the nose marked
  if (t === 5) { const c = Math.cos(yaw), s = Math.sin(yaw), q = [[2.3, .95], [2.3, -.95], [-2.3, -.95], [-2.3, .95]].map(([a, b]) => `${px(x + a * c + b * s)},${pz(z - a * s + b * c)}`); svg += `<polygon points="${q.join(' ')}" fill="#e44" stroke="#000"/><circle cx="${px(x + 1.9 * c)}" cy="${pz(z - 1.9 * s)}" r="${S * .4}" fill="#fff"/>`; continue; }
  svg += `<circle cx="${px(x)}" cy="${pz(z)}" r="${Math.max(1.5, S * .5)}" fill="${PC[t] || '#f0f'}"/>`;
}
// set pieces from facades_town.inc: {SET_KIND, x, z, yaw, v, "name"} lines in TOWN_SET
try {
  const t = readFileSync(join(ROOT, 'src/facades_town.inc'), 'utf8');
  const m = t.match(/TOWN_SET\[\][^=]*=\s*\{([\s\S]*?)\n\};/);
  if (m) for (const l of m[1].matchAll(/\{\s*SET_(\w+),\s*(-?[\d.]+)f?,\s*(-?[\d.]+)f?/g)) {
    const [, k, x, z] = l;
    svg += `<rect x="${+px(+x) - 3}" y="${+pz(+z) - 3}" width="6" height="6" fill="#f3f" stroke="#000"/><text x="${+px(+x) + 4}" y="${+pz(+z) + 3}" fill="#fcf" font-size="9">${k.toLowerCase()}</text>`;
  }
} catch { /* no set pieces yet */ }
// places
for (const r of rows(arr('CITY_PLACE'))) {
  const name = r.match(/"([^"]+)"/)[1], [x, , z, yaw] = nums(r.replace(/"[^"]*"/, ''));
  if (x < x0 || x > x1 || z < z0 || z > z1) continue;
  svg += `<circle cx="${px(x)}" cy="${pz(z)}" r="3" fill="#fff"/><line x1="${px(x)}" y1="${pz(z)}" x2="${px(x + Math.sin(yaw) * 3)}" y2="${pz(z + Math.cos(yaw) * 3)}" stroke="#fff"/><text x="${+px(x) + 4}" y="${+pz(z) - 3}" fill="#fff" font-size="10">${name}</text>`;
}
svg += '</svg>';
await sharp(Buffer.from(svg)).png().toFile(out);
console.log(`${out}: ${W}x${H}, x ${x0}..${x1}, z ${z0}..${z1}, ${S} px/m`);
