// Downtown Mesa from OpenStreetMap (source/osm/mesa-overpass.json, ODbL, see CREDITS).
//
//   source/osm/mesa-overpass.json   trimmed by `--trim <raw dump>` to what this tool reads
//   src/gen/city_data.h             the ground rectangles, footprints, lots, places and placed props
//   docs/PLACES.md                  the table of named places
//
// src/city.c builds and chunks the meshes from these tables at load.
// World: +x east, -z north, origin at the lab's door on the street line of West Main.
// Main is straightened by the small rotation (0.29 degrees) that makes its frontages run along x.
//
//   node tools/prep-city.mjs [--trim raw.json]
import { readFileSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
const OSM = join(ROOT, 'source/osm/mesa-overpass.json');
export const BOUNDS = { x0: -345, x1: 470, z0: -232, z1: 172 };

const LAT = 33.4153, LON0 = -111.834497, LAT0 = 33.4153175;
const MX = 111320 * Math.cos((LAT * Math.PI) / 180), MZ = 110574;
const TH = (0.2887 * Math.PI) / 180, FRONT = 3.1;        // the lab's frontage in the raw projection
function project(p) {
  const x = (p.lon - LON0) * MX, z = -(p.lat - LAT0) * MZ;
  return [x * Math.cos(TH) + z * Math.sin(TH), -x * Math.sin(TH) + z * Math.cos(TH) - FRONT];
}
const inBounds = ([x, z], m = 0) => x > BOUNDS.x0 - m && x < BOUNDS.x1 + m && z > BOUNDS.z0 - m && z < BOUNDS.z1 + m;

// --trim: keep buildings, roads, rail, platforms, trees, benches and named nodes near the area
const ai = process.argv.indexOf('--trim');
if (ai > 0) {
  const raw = JSON.parse(readFileSync(process.argv[ai + 1], 'utf8'));
  const keep = (e) => {
    const t = e.tags || {};
    if (!(t.building || t.highway || t.railway || t.public_transport || t.natural === 'tree' || t.amenity || t.name || t['addr:housenumber'] || t.leisure)) return false;
    const pts = e.type === 'node' ? [project(e)] : e.geometry.map(project);
    return pts.some((p) => inBounds(p, 30));
  };
  const out = raw.elements.filter(keep).map((e) => {
    const c = { type: e.type, id: e.id, tags: Object.fromEntries(Object.entries(e.tags || {}).filter(([k]) => /^(building|building:levels|height|name|addr:housenumber|addr:street|highway|service|railway|public_transport|natural|amenity|leisure|shop|lanes)$/.test(k))) };
    if (e.type === 'node') { c.lat = e.lat; c.lon = e.lon; } else c.geometry = e.geometry.map((g) => ({ lat: +g.lat.toFixed(7), lon: +g.lon.toFixed(7) }));
    return c;
  });
  writeFileSync(OSM, JSON.stringify({ copyright: 'Data (c) OpenStreetMap contributors, ODbL. Trimmed by tools/prep-city.mjs.', timestamp: raw.osm3s && raw.osm3s.timestamp_osm_base, elements: out }));
  console.log(`trimmed ${raw.elements.length} to ${out.length} elements`);
}
const J = JSON.parse(readFileSync(OSM, 'utf8'));

// ── the street grid: snapped straight ──
export const MAIN = { nCurb: 7.4, center: 19.5, sCurb: 31.6, bedHalf: 3.8, strip: 1.2 };
const streets = [
  // [name, axis of the line, centre, half width, from, to]  centres from the OSM centrelines after the rotation
  { n: 'Main', a: 'x', c: 19.5, hw: 12.1, s0: BOUNDS.x0 - 60, s1: BOUNDS.x1 + 60, walk: 8 },
  { n: '1st', a: 'x', c: -221, hw: 8, s0: BOUNDS.x0 - 60, s1: BOUNDS.x1 + 60, walk: 3.5 },
  { n: 'Pepper', a: 'x', c: -101, hw: 5.5, s0: -205, s1: 500, walk: 3 },
  { n: 'Mahoney', a: 'x', c: 138, hw: 4.5, s0: BOUNDS.x0 - 60, s1: -205, walk: 2.5 },
  { n: 'Robson', a: 'z', c: -205, hw: 6.5, s0: BOUNDS.z0 - 60, s1: BOUNDS.z1 + 60, walk: 3.5 },
  { n: 'MacDonald', a: 'z', c: 37, hw: 7, s0: BOUNDS.z0 - 60, s1: BOUNDS.z1 + 60, walk: 4 },
  { n: 'Center', a: 'z', c: 279, hw: 9, s0: BOUNDS.z0 - 60, s1: BOUNDS.z1 + 60, walk: 4 },
  { n: 'Drew', a: 'z', c: 159, hw: 5, s0: 18, s1: BOUNDS.z1 + 60, walk: 2.5 },
];
const rectOf = (s, grow = 0) => (s.a === 'x' ? [s.s0, s.c - s.hw - grow, s.s1, s.c + s.hw + grow] : [s.c - s.hw - grow, s.s0, s.c + s.hw + grow, s.s1]);

// ground: coordinate compressed cells, classed, merged into rectangles
const T = { ASPHALT: 0, PAVER: 1, CONCRETE: 2, LOT: 3, BED: 4, GRAVEL: 5 };
const Y = { 0: -0.15, 1: 0, 2: 0, 3: 0, 4: -0.1, 5: 0.02 };
const xs = new Set([BOUNDS.x0 - 60, BOUNDS.x1 + 60]), zs = new Set([BOUNDS.z0 - 60, BOUNDS.z1 + 60]);
for (const s of streets) { for (const g of [0, s.walk]) { const r = rectOf(s, g); xs.add(r[0]); xs.add(r[2]); zs.add(r[1]); zs.add(r[3]); } }
const X = [...xs].sort((a, b) => a - b), Z = [...zs].sort((a, b) => a - b);
const inR = (x, z, r) => x >= r[0] && x <= r[2] && z >= r[1] && z <= r[3];
function classify(x, z) {
  for (const s of streets) if (inR(x, z, rectOf(s))) return T.ASPHALT;
  for (const s of streets) if (inR(x, z, rectOf(s, s.walk))) return s.n === 'Main' ? T.PAVER : T.CONCRETE;
  return T.LOT;
}
const cells = [];
for (let j = 0; j + 1 < Z.length; j++) { const row = []; for (let i = 0; i + 1 < X.length; i++) row.push(classify((X[i] + X[i + 1]) / 2, (Z[j] + Z[j + 1]) / 2)); cells.push(row); }
const ground = [];
{
  const used = cells.map((r) => r.map(() => false));
  for (let j = 0; j < cells.length; j++) for (let i = 0; i < cells[j].length; i++) {
    if (used[j][i]) continue;
    const t = cells[j][i];
    let i1 = i; while (i1 + 1 < cells[j].length && !used[j][i1 + 1] && cells[j][i1 + 1] === t) i1++;
    let j1 = j;
    grow: while (j1 + 1 < cells.length) { for (let k = i; k <= i1; k++) if (used[j1 + 1][k] || cells[j1 + 1][k] !== t) break grow; j1++; }
    for (let b = j; b <= j1; b++) for (let k = i; k <= i1; k++) used[b][k] = true;
    ground.push({ x0: X[i], z0: Z[j], x1: X[i1 + 1], z1: Z[j1 + 1], y: Y[t], t });
  }
}
// the rail bed and its low edges: a layer over the street on Main
ground.push({ x0: BOUNDS.x0 - 60, z0: MAIN.center - MAIN.bedHalf, x1: BOUNDS.x1 + 60, z1: MAIN.center + MAIN.bedHalf, y: Y[T.BED], t: T.BED, over: 1 });
// curbs: every boundary between the street and anything else
const curbs = [];
for (let j = 0; j < cells.length; j++) for (let i = 0; i < cells[j].length; i++) {
  const c = cells[j][i];
  if (c === T.ASPHALT) continue;
  const test = (cc) => cc === T.ASPHALT;
  if (j + 1 < cells.length && test(cells[j + 1][i])) curbs.push([X[i], Z[j + 1], X[i + 1], Z[j + 1], 0, -1]);
  if (j > 0 && test(cells[j - 1][i])) curbs.push([X[i], Z[j], X[i + 1], Z[j], 0, 1]);
  if (i + 1 < cells[j].length && test(cells[j][i + 1])) curbs.push([X[i + 1], Z[j], X[i + 1], Z[j + 1], 1, -1]);
  if (i > 0 && test(cells[j][i - 1])) curbs.push([X[i], Z[j], X[i], Z[j + 1], 1, 1]);
}

// ── footprints ──
// Sutherland-Hodgman against half planes is enough: the cuts are straight lines, the blocks are nearly rectilinear
function clip(poly, axis, at, keepLess) {
  const out = [], s = keepLess ? -1 : 1;
  for (let i = 0; i < poly.length; i++) {
    const a = poly[i], b = poly[(i + 1) % poly.length];
    const da = s * (a[axis] - at), db = s * (b[axis] - at);
    if (da >= 0) out.push(a);
    if ((da >= 0) !== (db >= 0)) { const t = da / (da - db); out.push([a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t]); }
  }
  return out;
}
const area = (p) => { let s = 0; for (let i = 0; i < p.length; i++) { const a = p[i], b = p[(i + 1) % p.length]; s += a[0] * b[1] - b[0] * a[1]; } return s / 2; };
const dedupe = (p) => {
  const o = [];
  for (const q of p) { const l = o[o.length - 1]; if (!l || Math.hypot(l[0] - q[0], l[1] - q[1]) > 0.05) o.push(q); }
  while (o.length > 1 && Math.hypot(o[0][0] - o[o.length - 1][0], o[0][1] - o[o.length - 1][1]) < 0.05) o.pop();
  // collinear points
  for (let i = 0; i < o.length && o.length > 3;) {
    const a = o[(i + o.length - 1) % o.length], b = o[i], c = o[(i + 1) % o.length];
    const cr = (b[0] - a[0]) * (c[1] - b[1]) - (b[1] - a[1]) * (c[0] - b[0]);
    if (Math.abs(cr) < 0.02) o.splice(i, 1); else i++;
  }
  return o;
};
function earclip(poly) {
  // poly is CCW in the (x, z) sense where area() > 0
  const idx = poly.map((_, i) => i), tris = [];
  const cross = (a, b, c) => (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
  const inside = (p, a, b, c) => cross(a, b, p) >= -1e-6 && cross(b, c, p) >= -1e-6 && cross(c, a, p) >= -1e-6;
  let guard = 0;
  while (idx.length > 3 && guard++ < 5000) {
    let done = false;
    for (let i = 0; i < idx.length; i++) {
      const ia = idx[(i + idx.length - 1) % idx.length], ib = idx[i], ic = idx[(i + 1) % idx.length];
      const a = poly[ia], b = poly[ib], c = poly[ic];
      if (cross(a, b, c) <= 1e-6) continue;
      let ok = true;
      for (const k of idx) if (k !== ia && k !== ib && k !== ic && inside(poly[k], a, b, c)) { ok = false; break; }
      if (!ok) continue;
      tris.push(ia, ib, ic); idx.splice(i, 1); done = true; break;
    }
    if (!done) { idx.splice(0, 1); }
  }
  if (idx.length === 3) tris.push(idx[0], idx[1], idx[2]);
  return tris;
}

// Things the map has as one outline but the street has as several buildings: vertical cuts, and heights
const CUTS = [-2.875, 6.725, -8, 6, 100, 228, -176];   // around the lab, the Nile, One Macdonald, Margaritas, 166 W
const HEIGHT = [
  // [x, z inside the part, height, style]  styles: 0 stucco warm, 1 white, 2 brick, 3 beige glass office, 4 arts centre blue, 5 civic concrete
  [0, -66, 19, 1], [-255, -82, 38, 1], [-3, 58, 11, 2], [80, -20, 18, 3], [330, -25, 11, 5], [350, -30, 29, 5],
  [385, 137, 18, 4], [392, 97, 18, 4], [420, 140, 14, 4], [-138, -135, 9, 5], [65, -152, 12, 5], [146, -147, 14, 5], [228, -128, 9, 1],
  [328, -156, 18, 5], [429, -128, 16, 3], [424, -177, 10, 5], [468, -170, 9, 5], [238, 129, 10, 2], [236, 170, 8, 2], [135, 214, 8, 2],
  [-372, -71, 8, 0], [81, 60, 8, 2], [150, 60, 8, 2],
];
const hash = (a, b) => { let h = (Math.round(a * 7) * 73856093) ^ (Math.round(b * 11) * 19349663); h ^= h >>> 13; h = Math.imul(h, 0x5bd1e995); h ^= h >>> 15; return h >>> 0; };
const buildings = [];
const pointIn = (p, x, z) => { let c = false; for (let i = 0, j = p.length - 1; i < p.length; j = i++) { const [xi, zi] = p[i], [xj, zj] = p[j]; if ((zi > z) !== (zj > z) && x < ((xj - xi) * (z - zi)) / (zj - zi) + xi) c = !c; } return c; };
for (const e of J.elements) {
  const t = e.tags || {};
  if (e.type !== 'way' || !t.building) continue;
  let g = e.geometry.map(project);
  if (g.length > 1 && Math.hypot(g[0][0] - g[g.length - 1][0], g[0][1] - g[g.length - 1][1]) < 0.01) g.pop();
  if (!g.some((p) => inBounds(p))) continue;
  // clip to the bounds with a margin so the map's edge is still solid
  g = clip(clip(clip(clip(g, 0, BOUNDS.x0 - 25, false), 0, BOUNDS.x1 + 25, true), 1, BOUNDS.z0 - 25, false), 1, BOUNDS.z1 + 25, true);
  if (g.length < 3) continue;
  let pieces = [g];
  for (const c of CUTS) pieces = pieces.flatMap((p) => { const xs2 = p.map((q) => q[0]); if (Math.min(...xs2) < c - 0.5 && Math.max(...xs2) > c + 0.5) return [clip(p, 0, c, true), clip(p, 0, c, false)].filter((q) => q.length >= 3); return [p]; });
  for (let p of pieces) {
    p = dedupe(p);
    if (p.length < 3) continue;
    if (area(p) < 0) p.reverse();
    if (Math.abs(area(p)) < 6) continue;
    const cx = p.reduce((a, q) => a + q[0], 0) / p.length, cz = p.reduce((a, q) => a + q[1], 0) / p.length;
    // the lab's own slice of the block is the lab
    if (cx > -2.875 && cx < 6.725 && cz < 2 && cz > -45) continue;
    const lev = parseFloat(t['building:levels']) || 0;
    let h = lev ? 1.5 + lev * 3.7 : 5 + (hash(cx, cz) % 3) * 0.7, style = hash(cx, cz + 3) % 3;
    for (const [ox, oz, oh, os] of HEIGHT) if (pointIn(p, ox, oz)) { h = oh; style = os; }
    buildings.push({ p, h, style, name: t.name || '', hn: t['addr:housenumber'] || '', tris: earclip(p), levels: lev || Math.round(h / 4) });
  }
}

// which walls face a street (generic storefronts go there)
const nearStreet = (x, z) => streets.some((s) => inR(x, z, rectOf(s, s.walk + 8)));
for (const b of buildings) b.front = b.p.map((a, i) => {
  const c = b.p[(i + 1) % b.p.length], mx = (a[0] + c[0]) / 2, mz = (a[1] + c[1]) / 2, l = Math.hypot(c[0] - a[0], c[1] - a[1]);
  // CCW in (x, z): the outward normal
  const nx = (c[1] - a[1]) / l, nz = -(c[0] - a[0]) / l;
  return l > 4 && nearStreet(mx + nx * 6, mz + nz * 6) ? 1 : 0;
});

// ── lots: every facade at its real address ──
// side: which way the building's street front faces. N lots are north of Main and face south, and so on.
const FACE = { south: { yaw: 0, n: [0, 1] }, north: { yaw: Math.PI, n: [0, -1] }, east: { yaw: Math.PI / 2, n: [1, 0] }, west: { yaw: -Math.PI / 2, n: [-1, 0] } };
const L = (place, name, addr, face, a, b, facade, src, opt = {}) => ({ place, name, addr, face, a, b, facade, src, ...opt });
const OSMSRC = 'OpenStreetMap node', WEB = 'venue listing';
const LOTS = [
  // North side of Main, 200 block (footprint -307 to -225)
  L('pxg_north', 'Storefront', '220 W Main St', 'south', -307, -295, null, 'OSM number; street view', { style: 0 }),
  L('worth', 'Worth Takeaway', '218 W Main St', 'south', -295, -285, 'worth', `${WEB}; order from OSM, widths from street view`),
  L('burritoholics', 'Burritoholics', '216 W Main St', 'south', -285, -275, 'burritoholics', `${WEB}; order from OSM, widths from street view`),
  L('nct', 'Neighborhood Comedy Theatre / The Sacred Pint', '214 W Main St', 'south', -275, -263, 'nct', 'OSM number; street view'),
  L('gus', "Gus's World Famous Fried Chicken", '212 W Main St', 'south', -263, -251, 'gus', `${WEB}; street view`),
  L('oro', 'Oro Brewing Company', '210 W Main St', 'south', -251, -242, 'oro', 'OSM number; street view'),
  L('old_brick', 'Old Brick House Vintage Market', '206 W Main St', 'south', -242, -232, 'old_brick', 'street view (OSM still has it at 220)'),
  L('ribaudo', 'Ribaudo Fine Arts / chiropractic', '202 W Main St', 'south', -232, -226, null, OSMSRC, { style: 1 }),
  L('chupacabra', 'Chupacabra Taproom', '14 N Robson', 'east', -48, -38, 'chupacabra', 'OSM footprint; fronts Robson', { line: -226 }),
  // 100 block west of the alley (footprints -197 to -82)
  L('boutique', 'The Boutique / De Cesare Design', '166 W Main St', 'south', -197, -164, null, OSMSRC, { style: 3 }),
  L('naysayer', 'Naysayer Tattoo', '160 W Main St', 'south', -164, -152, null, OSMSRC, { style: 2 }),
  L('phantom_fox', 'Phantom Fox Beer Co. / Goat and Ram', '150 W Main St', 'south', -148, -127, 'phantom_fox', `${WEB}; between OSM 158 and 146`),
  L('blossom', 'Blossom', '146 W Main St', 'south', -127, -118, null, OSMSRC, { style: 4 }),
  L('crismons', "Crismon's Flowers", '144 W Main St', 'south', -118, -112, null, OSMSRC, { style: 4 }),
  L('que_chevere', 'Que Chevere', '142 W Main St', 'south', -112, -103, 'que_chevere', OSMSRC),
  L('atomic', 'Atomic Vintage / Buckhorn Vintage', '140 W Main St', 'south', -103, -92, null, OSMSRC, { style: 5 }),
  L('pomeroy', "Pomeroy's Men's Store", '136 W Main St', 'south', -92, -82, 'pomeroy', OSMSRC),
  // the lab's block (footprint -76 to 18)
  L('haunted', "East Valley Paranormal / Mesa's Haunted Museum", '130 W Main St', 'south', -76, -62, 'haunted', 'venue site; street view'),
  L('against_grain', 'Against the Grain Bakery and Cafe', '124 W Main St', 'south', -62, -44, 'against_grain', 'venue site; street view'),
  L('cream_block', 'Vacant storefronts', '120 W Main St', 'south', -44, -23, 'cream_block', 'street view'),
  L('butterfly', 'Butterfly canopy block', '114 W Main St', 'south', -23, -2.875, 'butterfly', 'street view'),
  L('trellis', 'Arcade and trellis', '100 W Main St', 'south', 6.725, 18, 'trellis', 'street view'),
  // One Macdonald block
  L('one_macdonald', 'One Macdonald', '1 N MacDonald', 'south', 60, 100, 'one_macdonald', OSMSRC),
  L('level1', 'Level 1 Arcade Bar', '48 W Main St', 'south', 100, 112, 'level1', `${WEB}; OSM node`),
  L('mad_candy', 'Mad Candy', '46 W Main St', 'south', 112, 123, 'mad_candy', WEB),
  L('mangos', "Mango's Mexican Cafe", '44 W Main St', 'south', 123, 140, 'mangos', OSMSRC),
  L('booms', 'Storefronts', '30 W Main St', 'south', 140, 184, null, 'footprint', { style: 0 }),
  L('twelve_west', '12 West', '12 W Main St', 'south', 196, 228, null, OSMSRC, { style: 2 }),
  L('margaritas', 'Margaritas', '10 W Main St', 'south', 228, 241, 'margaritas', `${WEB}; OSM node`),
  L('lost_dutchman', 'Lost Dutchman Coffee House', '12 N Center St', 'east', -44, -31, 'lost_dutchman', `${WEB}; OSM node`, { line: 262 }),
  // South side of Main
  L('bri', 'B.R.I. Taproom and Arcade', '213 W Main St', 'north', -275, -248, 'bri', `${WEB}; footprint`),
  L('main_burger', 'Main Burger', '161 W Main St', 'north', -184, -172, 'main_burger', `${WEB}; SE corner of Main and Robson`),
  L('az_distilling', 'Arizona Distilling Co.', '155 W Main St', 'north', -154, -119, 'az_distilling', `${WEB}; footprint`),
  L('history', 'History, by George!', '129 W Main St', 'north', -82, -72, null, OSMSRC, { style: 5 }),
  L('chiwas', 'Tacos Chiwas', '127 W Main St', 'north', -72, -64, 'chiwas', OSMSRC),
  L('proof', 'Proof Bread', '121 W Main St', 'north', -64, -56, 'proof', OSMSRC),
  L('espiritu', 'Espiritu', '123 W Main St', 'north', -56, -38, 'espiritu', `${WEB} (OSM 123 is still the former tenant)`),
  L('tre_bella', 'Tre Bella', '119 W Main St', 'north', -38, -20, null, OSMSRC, { style: 4 }),
  L('west_of_nile', 'Storefront', '111 W Main St', 'north', -20, -8, null, OSMSRC, { style: 2 }),
  L('nile', 'The Nile Theater', '105 W Main St', 'north', -8, 6, 'nile', OSMSRC),
  L('oneohone', 'OneOhOne Art Gallery', '101 W Main St', 'north', 6, 18, null, OSMSRC, { style: 1 }),
  L('le_studio', 'Le Studio', '47 W Main St', 'north', 112, 124, null, OSMSRC, { style: 2 }),
  L('linton', 'Linton Milano Music', '45 W Main St', 'north', 124, 132, null, OSMSRC, { style: 0 }),
  L('nunthaporn', "Nunthaporn's Thai Cuisine", '41 W Main St', 'north', 132, 140, null, OSMSRC, { style: 5 }),
  L('drew_lofts', 'The Lofts at The Drew', '39 W Main St', 'north', 140, 153, null, OSMSRC, { style: 2 }),
  // Off Main
  L('casa_copal', 'Casa Copal', '28 S MacDonald', 'east', 58, 73, 'casa_copal', 'venue site (bikemesa refs); block frontage by number', { line: 18 }),
  L('diamond_dons', "Diamond Don's", '42 S MacDonald', 'east', 100, 118, 'diamond_dons', 'venue listings; footprint', { line: 19 }),
  L('organic_bean', 'The Organic Bean Cafe', '40 N MacDonald', 'east', -148, -134, 'organic_bean', OSMSRC, { line: 16.5 }),
];
// find the wall each lot stands on: the first footprint edge from the street
function frontage(lot) {
  const f = FACE[lot.face];
  const mid = (lot.a + lot.b) / 2;
  const along = f.n[1] !== 0;
  let best = null;
  for (const b of buildings) {
    for (let i = 0; i < b.p.length; i++) {
      const a = b.p[i], c = b.p[(i + 1) % b.p.length];
      if (along) {
        const mn = Math.min(a[0], c[0]), mx = Math.max(a[0], c[0]);
        if (mid < mn || mid > mx || mx - mn < 0.5) continue;
        const z = a[1] + ((c[1] - a[1]) * (mid - a[0])) / (c[0] - a[0]);
        const s = f.n[1] > 0 ? z : -z;
                if ((f.n[1] > 0 ? z <= 7.4 && z > -40 : z >= 31.6 && z < 90) && (best === null || s > best)) best = s;
      } else {
        const mn = Math.min(a[1], c[1]), mx = Math.max(a[1], c[1]);
        if (mid < mn || mid > mx || mx - mn < 0.5) continue;
        const x = a[0] + ((c[0] - a[0]) * (mid - a[1])) / (c[1] - a[1]);
        if (lot.line === undefined || Math.abs(x - lot.line) > 22) continue;
        const s = x * f.n[0];
        if (best === null || s > best) best = s;
      }
    }
  }
  if (best === null) { console.log(`  no footprint under ${lot.place}, keeping the street line`); return along ? (f.n[1] > 0 ? 0 : 39.2) : lot.line; }
  return along ? best * f.n[1] : best * f.n[0];
}
const lots = LOTS.map((l) => {
  const wall = frontage(l), f = FACE[l.face], mid = (l.a + l.b) / 2, along = f.n[1] !== 0;
  const cx = along ? mid : wall, cz = along ? wall : mid;
  return { ...l, wall, cx, cz, yaw: f.yaw, w: l.b - l.a, n: f.n };
});

// ── named places ──
const places = [];
const addPlace = (name, x, y, z, yaw) => places.push({ name, x, y, z, yaw });
const standOff = { nile: 4.2, espiritu: 6.5, phantom_fox: 3.9, worth: 4.6, burritoholics: 4.6, gus: 5.5, level1: 4.2, naysayer: 2.6, cream_block: 4.4, booms: 4.4, margaritas: 6.5, lost_dutchman: 6.5, chiwas: 6, mangos: 5.5, oro: 5.5, que_chevere: 5, against_grain: 5.5, main_burger: 6, organic_bean: 7 };
for (const l of lots) {
  const d = standOff[l.place] ?? 3.2;
  const x = l.cx + l.n[0] * d, z = l.cz + l.n[1] * d;
  addPlace(l.place, x, 0, z, Math.atan2(-l.n[0], -l.n[1]));
}
addPlace('lab_door', 0, 0, 2.4, Math.PI);
addPlace('lab_back_door', 3.7, 0, -42.5, 0);
addPlace('lab_lot', -22, 0, -62, Math.PI / 2);
addPlace('lot_fight', -62, 0, -66, 0);
addPlace('mid_block', -28, -0.15, 12.5, Math.PI / 2);
addPlace('macdonald_main', 28.5, 0, 5.6, Math.PI / 2);
addPlace('center_main_station', 340, 0.2, 19.5, Math.PI / 2);
addPlace('city_plaza', 350, 0, 3, Math.PI);
addPlace('arts_center', 385, 0, 35, 0);
// names the design uses for lots above
const alias = { against_grain: 'against_grain' };
for (const [a, b] of Object.entries({ haunted: 'haunted' })) void a, void b, void alias;

// ── what stands on the street ──
const PROP = { POLE: 0, LAMP: 1, TREE: 2, PALM: 3, BENCH: 4, CAR: 5, PLANTER: 6, SIGNAL: 7, TOWER: 8, SHELTER: 9, TRASH: 10, STATUE: 11, TRELLIS: 12 };
const props = [];
const prop = (t, x, z, yaw = 0, v = 0) => props.push([t, +x.toFixed(2), +z.toFixed(2), +yaw.toFixed(3), v]);
const insideBuilding = (x, z, m = 0) => buildings.some((b) => pointIn(b.p, x, z) || (m && (pointIn(b.p, x + m, z) || pointIn(b.p, x - m, z) || pointIn(b.p, x, z + m) || pointIn(b.p, x, z - m))));
const XS = streets.filter((s) => s.a === 'z').map((s) => s.c);
const nearCross = (x, m) => streets.filter((s) => s.a === 'z').some((s) => Math.abs(x - s.c) < s.hw + m);
// catenary poles down the median
for (let x = BOUNDS.x0; x <= BOUNDS.x1; x += 40) { const q = x + 4; if (!nearCross(q, 2)) prop(PROP.POLE, q, MAIN.center, 0); }
// black lamp posts and planters along Main's sidewalks
for (let x = BOUNDS.x0 + 6; x < BOUNDS.x1; x += 19) {
  if (nearCross(x, 6)) continue;
  for (const [z, yaw] of [[MAIN.nCurb - 1.0, Math.PI / 2], [MAIN.sCurb + 1.0, -Math.PI / 2]]) {
    if (Math.abs(x) < 1.2) continue;
    prop(PROP.LAMP, x, z, yaw);
    const px = x + 9.5;
    if (!nearCross(px, 7)) {
      const pz = z < 18 ? z - 0.3 : z + 0.3;
      if (x > 240 && x < 420) prop(PROP.PALM, px, pz, 0, hash(px, 1) % 3);
      else if (hash(px, pz) % 5) prop(PROP.PLANTER, px, pz, 0, hash(pz, px) % 3);
    }
  }
}
// lamps on the cross streets
for (const s of streets.filter((q) => q.a === 'z')) for (let z = BOUNDS.z0 + 10; z < BOUNDS.z1; z += 26) {
  if (z > -2 && z < 40) continue;
  for (const sd of [-1, 1]) prop(PROP.LAMP, s.c + sd * (s.hw + 0.8), z, sd < 0 ? Math.PI : 0);
}
for (const s of streets.filter((q) => q.a === 'x' && q.n !== 'Main')) for (let x = Math.max(BOUNDS.x0, s.s0) + 8; x < Math.min(BOUNDS.x1, s.s1); x += 30) {
  if (nearCross(x, 5)) continue;
  for (const sd of [-1, 1]) prop(PROP.LAMP, x, s.c + sd * (s.hw + 0.8), sd < 0 ? Math.PI / 2 : -Math.PI / 2);
}
// parked cars. Clear in front of the lab, for the street fight, and near corners
for (const [z, flip] of [[8.0, 0], [29.6, 1]]) for (let x = BOUNDS.x0; x < BOUNDS.x1; x += 6.8) {
  if (nearCross(x, 11)) continue;
  if (x > 240 && x < 420) continue;
  if (flip === 0 && x > -60 && x < 12) continue;
  if (hash(x, z) % 100 > 56) continue;
  prop(PROP.CAR, x, z, flip ? Math.PI : 0, hash(z, x) % 6);
}
// signals at the corners
for (const c of XS) for (const [dx, z] of [[-1, MAIN.nCurb - 1], [1, MAIN.sCurb + 1]]) {
  const s = streets.find((q) => q.c === c);
  prop(PROP.SIGNAL, c + dx * (s.hw + 1), z, dx < 0 ? 0 : Math.PI);
}
// OSM trees and benches
for (const e of J.elements) {
  const t = e.tags || {};
  if (e.type !== 'node') continue;
  const p = project(e);
  if (!inBounds(p)) continue;
  if (t.natural === 'tree' && !insideBuilding(p[0], p[1], 1) && !streets.some((s) => inR(p[0], p[1], rectOf(s, 0.5)))) {
    const palm = p[0] > 255 && p[0] < 440 && p[1] > -70 && p[1] < 60;
    prop(palm ? PROP.PALM : PROP.TREE, p[0], p[1], 0, hash(p[0], p[1]) % 3);
  }
  if (t.amenity === 'bench' && Math.abs(p[1] - 3) < 5 + 30 && !insideBuilding(p[0], p[1], 1) && !streets.some((s) => inR(p[0], p[1], rectOf(s, 0.5)))) prop(PROP.BENCH, p[0], p[1], Math.abs(p[1]) < 18 ? 0 : Math.PI, 0);
}
// the corners the street view shows
for (const x of [322, 343, 364, 385]) prop(PROP.TRELLIS, x, MAIN.center, 0, 0);
prop(PROP.TOWER, 24.5, 2.6, 0, 0);      // steel lattice tower, Macdonald and Main
prop(PROP.TOWER, 322, 2, 0, 1);         // and the tall one by the station
prop(PROP.SHELTER, -24.5, 3.6, 0, 0);   // the bus shelter in front of the butterfly block
prop(PROP.BENCH, -33, 3.2, 0, 0); prop(PROP.BENCH, -42, 3.2, 0, 0); prop(PROP.BENCH, 8.6, 4.6, 0, 0);
prop(PROP.STATUE, -80, 4.5, Math.PI / 4, 0); prop(PROP.STATUE, -122.5, 5, 0, 1); prop(PROP.STATUE, 182, 35, Math.PI, 0);
prop(PROP.TRASH, -36, 5.9, 0, 0); prop(PROP.TRASH, -90, 5.9, 0, 0); prop(PROP.TRASH, 110, 5.9, 0, 0); prop(PROP.TRASH, -20, 32.5, 0, 0); prop(PROP.TRASH, 60, 32.5, 0, 0);

// road paint: crosswalks at Main's corners, lanes, stall lines in the lots
const stripes = [];
const stripe = (x0, z0, x1, z1, c) => stripes.push([+x0.toFixed(2), +z0.toFixed(2), +x1.toFixed(2), +z1.toFixed(2), c]);
const WHITE = 0, YELLOW = 1;
for (const c of XS) {
  const s = streets.find((q) => q.c === c);
  for (const sd of [-1, 1]) {
    const cx = c + sd * (s.hw + 1.8);
    for (let z = MAIN.nCurb + 0.3; z < MAIN.sCurb - 0.2; z += 1.2) stripe(cx - 1.4, z, cx + 1.4, z + 0.6, WHITE);
  }
  for (const [z0, z1] of [[MAIN.nCurb - 8 - 3.2, MAIN.nCurb - 8 - 0.4], [MAIN.sCurb + 8 + 0.4, MAIN.sCurb + 8 + 3.2]]) void z0, void z1;
  for (const z of [MAIN.nCurb - 2.2, MAIN.sCurb + 2.2]) for (let x = c - s.hw + 0.3; x < c + s.hw - 0.3; x += 1.2) stripe(x, z - 1.4, x + 0.6, z + 1.4, WHITE);
}
// lanes on Main: double yellow by the rail bed, and the parking lane line
for (let x = BOUNDS.x0 - 50; x < BOUNDS.x1 + 50; x += 12) {
  const x1 = x + 12;
  if (XS.some((c) => { const s = streets.find((q) => q.c === c); return x1 > c - s.hw - 4 && x < c + s.hw + 4; })) continue;
  for (const dz of [-0.09, 0.09]) { stripe(x, MAIN.center - MAIN.bedHalf - MAIN.strip - 0.15 + dz - 0.05, x1, MAIN.center - MAIN.bedHalf - MAIN.strip - 0.15 + dz + 0.05, YELLOW); stripe(x, MAIN.center + MAIN.bedHalf + MAIN.strip + 0.15 + dz - 0.05, x1, MAIN.center + MAIN.bedHalf + MAIN.strip + 0.15 + dz + 0.05, YELLOW); }
  stripe(x, MAIN.nCurb + 2.5, x1, MAIN.nCurb + 2.58, WHITE); stripe(x, MAIN.sCurb - 2.58, x1, MAIN.sCurb - 2.5, WHITE);
}
// stalls along the parking aisles
for (const e of J.elements) {
  const t = e.tags || {};
  if (e.type !== 'way' || t.highway !== 'service' || t.service !== 'parking_aisle') continue;
  const g = e.geometry.map(project);
  for (let i = 0; i + 1 < g.length; i++) {
    const a = g[i], b = g[i + 1], l = Math.hypot(b[0] - a[0], b[1] - a[1]);
    if (l < 6) continue;
    const ux = (b[0] - a[0]) / l, uz = (b[1] - a[1]) / l;
    const axial = Math.abs(ux) > 0.97 || Math.abs(uz) > 0.97;
    if (!axial) continue;
    for (let d = 2; d < l - 1; d += 2.8) for (const sd of [-1, 1]) {
      const px = a[0] + ux * d + -uz * sd * 3.2, pz = a[1] + uz * d + ux * sd * 3.2;
      const qx = px + -uz * sd * 5, qz = pz + ux * sd * 5;
      if (!inBounds([px, pz]) || !inBounds([qx, qz])) continue;
      if (insideBuilding(px, pz) || insideBuilding(qx, qz) || streets.some((s) => inR(qx, qz, rectOf(s, s.walk)) || inR(px, pz, rectOf(s, s.walk)))) continue;
      if (Math.abs(ux) > 0.97) stripe(Math.min(px, qx) - 0.05, Math.min(pz, qz), Math.max(px, qx) + 0.05, Math.max(pz, qz), WHITE);
      else stripe(Math.min(px, qx), Math.min(pz, qz) - 0.05, Math.max(px, qx), Math.max(pz, qz) + 0.05, WHITE);
      // a few cars in the lots
      if (hash(px, pz) % 7 === 0) { const mx = px + -uz * sd * 2.5 + ux * 1.4, mz = pz + ux * sd * 2.5 + uz * 1.4; if (!(mx > -85 && mx < -40 && mz > -80 && mz < -50) && !insideBuilding(mx, mz, 3)) prop(PROP.CAR, mx, mz, Math.atan2(-uz * sd, ux * sd) * -1 + 0, hash(pz, px) % 6); }
    }
  }
}
// the far skyline: a ring of low boxes past the fog
const skyline = [];
for (let a = 0; a < 96; a++) {
  const an = (a / 96) * Math.PI * 2, r = 620 + (hash(a, 3) % 300);
  skyline.push([+(35 + Math.cos(an) * r * 1.05).toFixed(1), +(-30 + Math.sin(an) * r).toFixed(1), 10 + (hash(a, 5) % 60) * (hash(a, 9) % 5 === 0 ? 0.7 : 0.2), 30 + (hash(a, 7) % 40)]);
}

// ── write ──
const q = (s) => JSON.stringify(s);
const f = (v) => { const r = Number(v).toFixed(2); return `${r}f`; };
let h = '/* generated by tools/prep-city.mjs from OpenStreetMap (ODbL, see CREDITS): do not edit */\n#ifndef CITY_DATA_H\n#define CITY_DATA_H\n';
h += `#define CITY_X0 ${f(BOUNDS.x0)}\n#define CITY_X1 ${f(BOUNDS.x1)}\n#define CITY_Z0 ${f(BOUNDS.z0)}\n#define CITY_Z1 ${f(BOUNDS.z1)}\n`;
h += `#define MAIN_N_CURB ${f(MAIN.nCurb)}\n#define MAIN_S_CURB ${f(MAIN.sCurb)}\n#define MAIN_CENTER ${f(MAIN.center)}\n#define MAIN_BED_HALF ${f(MAIN.bedHalf)}\n#define MAIN_STRIP ${f(MAIN.strip)}\n`;
h += 'enum { GT_ASPHALT, GT_PAVER, GT_CONCRETE, GT_LOT, GT_BED, GT_GRAVEL };\n';
h += `enum { ${Object.keys(PROP).map((k) => `CP_${k}`).join(', ')} };\n`;
h += 'typedef struct { float x0, z0, x1, z1, y; int type; int over; } city_rect;\n';
h += `#define CITY_NGROUND ${ground.length}\nstatic const city_rect CITY_GROUND[] = {\n${ground.map((g) => `  {${f(g.x0)},${f(g.z0)},${f(g.x1)},${f(g.z1)},${f(g.y)},${g.t},${g.over || 0}},`).join('\n')}\n};\n`;
h += 'typedef struct { float x0, z0, x1, z1; int axis, dir; } city_curb;\n';
h += `#define CITY_NCURB ${curbs.length}\nstatic const city_curb CITY_CURB[] = {\n${curbs.map((c) => `  {${f(c[0])},${f(c[1])},${f(c[2])},${f(c[3])},${c[4]},${c[5]}},`).join('\n')}\n};\n`;
const pts = [], fr = [], tri = [];
let bs = '';
for (const b of buildings) {
  bs += `  {${pts.length / 2},${b.p.length},${tri.length},${b.tris.length},${f(b.h)},${b.style}},\n`;
  for (const p of b.p) pts.push(p[0], p[1]);
  fr.push(...b.front); tri.push(...b.tris);
}
h += 'typedef struct { int first, n, tfirst, tn; float h; int style; } city_building;\n';
h += `#define CITY_NBUILDING ${buildings.length}\nstatic const city_building CITY_BUILDING[] = {\n${bs}};\n`;
h += `static const float CITY_PTS[] = {${pts.map((v) => v.toFixed(2) + 'f').join(',')}};\n`;
h += `static const unsigned char CITY_FRONT[] = {${fr.join(',')}};\n`;
h += `static const unsigned short CITY_TRI[] = {${tri.join(',')}};\n`;
h += 'typedef struct { const char *place, *name, *facade; float cx, cz, yaw, w; int style; } city_lot;\n';
h += `#define CITY_NLOT ${lots.length}\nstatic const city_lot CITY_LOT[] = {\n${lots.map((l) => `  {${q(l.place)},${q(l.name)},${l.facade ? q(l.facade) : 'NULL'},${f(l.cx)},${f(l.cz)},${l.yaw.toFixed(4)}f,${f(l.w)},${l.style ?? 0}},`).join('\n')}\n};\n`;
h += 'typedef struct { const char *name; float x, y, z, yaw; } city_place_t;\n';
h += `#define CITY_NPLACE ${places.length}\nstatic const city_place_t CITY_PLACE[] = {\n${places.map((p) => `  {${q(p.name)},${f(p.x)},${f(p.y)},${f(p.z)},${p.yaw.toFixed(4)}f},`).join('\n')}\n};\n`;
h += 'typedef struct { short t; float x, z, yaw; short v; } city_prop;\n';
h += `#define CITY_NPROP ${props.length}\nstatic const city_prop CITY_PROP[] = {\n${props.map((p) => `  {${p[0]},${f(p[1])},${f(p[2])},${f(p[3])},${p[4]}},`).join('\n')}\n};\n`;
h += 'typedef struct { float x0, z0, x1, z1; int c; } city_stripe;\n';
h += `#define CITY_NSTRIPE ${stripes.length}\nstatic const city_stripe CITY_STRIPE[] = {\n${stripes.map((s) => `  {${f(s[0])},${f(s[1])},${f(s[2])},${f(s[3])},${s[4]}},`).join('\n')}\n};\n`;
h += `#define CITY_NSKY ${skyline.length}\nstatic const float CITY_SKY[][4] = {\n${skyline.map((s) => `  {${f(s[0])},${f(s[1])},${f(s[2])},${f(s[3])}},`).join('\n')}\n};\n`;
h += '#endif\n';
writeFileSync(join(ROOT, 'src/gen/city_data.h'), h);

// PLACES.md
const sideOf = (l) => (l.face === 'south' ? 'north side of Main' : l.face === 'north' ? 'south side of Main' : l.addr.includes('MacDonald') ? (l.addr.includes(' S ') ? 'S MacDonald, faces east' : 'N MacDonald, faces east') : l.addr.includes('Robson') ? 'N Robson, faces east' : 'N Center, faces east');
let md = '# Named places\n\nMade by tools/prep-city.mjs. World x, z are metres (+x east, -z north, origin at the lab door on the street line). The point listed is the facade or lot centre on its wall; `world_place` gives the standing point in front.\n\n';
md += '| Place id | Name | Address | Side | World x | World z | Frontage m | Source |\n|---|---|---|---|---|---|---|---|\n';
for (const l of lots) md += `| \`${l.place}\` | ${l.name} | ${l.addr} | ${sideOf(l)} | ${l.cx.toFixed(1)} | ${l.cz.toFixed(1)} | ${l.w.toFixed(1)} | ${l.src} |\n`;
md += '\nOther places: ' + places.filter((p) => !lots.some((l) => l.place === p.name)).map((p) => `\`${p.name}\` (${p.x.toFixed(1)}, ${p.z.toFixed(1)})`).join(', ') + '.\n';
md += '\n## Left out: not on the mapped stretch\n\n| Venue | Address | Why |\n|---|---|---|\n| Republica Empanada | 204 E 1st Ave, Mesa | about 900 m east of the lab, south of Main; off the map |\n| Hope\'s Frybread | 144 S Mesa Dr, Mesa | about 1.6 km east; off the map |\n| Pizza Mart | 1329 E Main St, Mesa | at Stapley, about 3 km east |\n| The Guadalupe on Main | 1526 E Main St, Mesa | past Stapley, about 3.5 km east |\n';
writeFileSync(join(ROOT, 'docs/PLACES.md'), md);
console.log(`city: ${ground.length} ground rects, ${curbs.length} curbs, ${buildings.length} building parts, ${lots.length} lots, ${places.length} places, ${props.length} props, ${stripes.length} stripes; header ${(h.length / 1024).toFixed(0)} KB`);
for (const l of lots) console.log(`  ${l.place.padEnd(16)} wall ${l.wall.toFixed(1).padStart(7)}  ${l.face}`);
