# The attic (v2.0 design, agreed 2026-11-12)

This replaces the v1.4 and v1.5 attic (the red X dodge run, the node trace hack and the text card memories).
The user picked every direction below. Build to it; ask before changing it.

## Why the old one failed

KEYHOLDER is an action visual novel: voiced scenes shot by the director, hosting roams with tasks, and real
anime fights with boss phases and arenas that open up. The old attic ignored all of that and invented cheaper
idioms of its own: an arcade dodge run instead of a fight, a grid hack instead of a scene, and a memory that
was a line of text, because attic.c played memories itself so the story VM would not pop the player out of
the stage. The room was a brown box with grey cubes. The fix is to build the attic out of the game's own
systems and to dress the room like the real one.

## The arc

1. **The stairs.** Unchanged: the rope, the PVC sign, the member's warning, the barrier, the roam prompt.
2. **The nightmare attic.** The first time up, the attic is wrong: far larger than the building, a maze of
   2x4 shelving canyons several storeys tall, the glitched mood. Soft boards are marked the way the real
   ones are, in spray paint (orange and red X marks, circled boards, arrows, "BAD", "NO"), and they creak
   before they give. Stepping on one drops you through to the last landing (a short fall cut, not a death).
   The maze runs through the attic's zones in turn, each with its own enemies, fought with the normal fight
   system (FIGHT ops with twists, barks, assists):
   - holiday supplies: mannequins in lights and Santa hats, stiff and jerky
   - old member storage: the Unclaimed, figures built of totes and red tags
   - robotics: junk-built robot shells
   - metal stock and the computer parts: mixed waves before the rack
3. **SYSOP.** At the back-left corner the rack is awake. A boss built of cable and CRT glow fights in front
   of it. Each phase changes the arena with the existing boss twist (boards give way, the fans blow, shelves
   topple). When it kneels you jack in at the pizza-box terminal on the rack.
4. **The real attic.** The room snaps back to the blueprint (below) in the warm normal mood. From then on
   going upstairs lands here. It is a place to wander: shelves of real lab stuff, things to find.
5. **Flashbacks.** Finding certain objects plays a memory as a real scene: the story VM, the director's
   shots, cast on screen, voiced lines (Kokoro), CHOICEs, and where it fits a small playable part built from
   existing TASK kinds. After it ends you are back in the attic where you stood.
6. **The terminal.** The pizza box runs four games (below), unlocked one after another.

## The blueprint room

From HSL-108: the rear attic floor is plan x 31.3 to 40.4 (9.1 m deep) by plan z 0 to 9.4 (9.4 m wide). The
stair climbs along plan +x at plan z 2.7 to 3.6 and comes up through a hole in the floor near the west side,
facing the back of the building. In stage space (+x east, -z north, metres): the room is x -4.7..4.7,
z -4.55..4.55, the stair hole x -2.0..-1.1, z 0.65..4.55, the player arrives at about (-1.55, 0, 0.6)
facing north. Headroom is raised over the real 1.3 m crawlspace so the follow camera works (knee walls
about 1.4 m, the ridge about 3.4 m, running north to south). Floor is plywood and old boards with spray
paint marks; the hole has a 2x4 guard rail; pink insulation between the rafters.

Wall zones, as the user describes the real room:
- north-west corner: the server rack, a standard 19 inch rack, with the pizza-box terminal on it
- north wall, west half, beside the rack: steel shelving of computer and server parts
- north wall, east half: electronics
- east wall: 2x4 shelving of metalworking stock; arts and crafts
- south wall, east of the stair: VEX robotics; holiday supplies
- west wall: a long run of old member storage boxes
- the floor between: overflow, a path worn to the rack

## Assets

The room is baked by tools/prep-attic.mjs to assets/models/attic*.glb as vertex coloured chunks, like the
arenas, so it matches the game's look. Props come from the CC0 library at ~/Projects/heatsync/3d-assets
(Kenney furniture-kit and holiday-kit, KayKit furniture-bits, low-poly-tools and others), with their texture
colours baked into vertex colours. Shelving, lumber, totes, the rack and spray paint are built in the tool.
Each pack used is credited in CREDITS. The user sees a screenshot of the dressed room before more is built on it.

## Flashbacks

Eight memories from the archive, fictionalised: eras and events are real, people appear only by role and as
the game's own cast. No real names, no conflicts, nothing about the real racks' history, no deaths.
- the balloon launch at dawn in Gila Bend (electronics shelf)
- the laser cutter held at the border for months (metal stock)
- the Glitter Box: repainting the pink storefront, then carrying the first server upstairs (member storage; carry task)
- HAXMAS (holiday bins)
- a first timer's Arduino Night, the LED finally blinks (electronics)
- six foot theatre robots built against the clock (robotics)
- Tear Apart Night, kids opening a donated PC (computer parts)
- one more from the archive list if a shelf needs it

The rack's own story is invented: an old box that ran the lab's wiki and locked the doors at night.

## The terminal games

All four are pad playable, need real decisions, and cannot be won by mashing one button. Each is its own file.
- GLITCH: read an oscilloscope trace and sweep a glitch's offset and width to skip the chip's password check.
- WIRETAP: sniff bits from the old door controller, deduce the card format Mastermind style, replay it before
  the 10 pm relock.
- CORE: TIS-100 style puzzles, pick three to six instructions to turn inputs into outputs, then optimise.
- DAEMON WAR: Core War lite, write a short program and watch it fight the old daemon for a circular memory.

## Keep from v1.5

STAGE_ATTIC and its routing in world.c, the env_normal mood lerp, the save_t fields, the entrance, and the
file split (attic_env.c the room, attic.c the mode, terminal.c the CRT shell).
