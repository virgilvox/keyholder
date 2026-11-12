/* memory_props.h: set pieces for the attic's flashbacks (tools/script/attic.mjs PROP names): one forty's pink
 * storefront, the laser's crate, a theatre robot, the move's boxes, HAXMAS lights, the rack on its hand truck.
 * story.c's PROP op passes any name it does not own to memory_prop. */
#ifndef KH_MEMORY_PROPS_H
#define KH_MEMORY_PROPS_H
#include "ds.h"
#include "three.h"
/* the PROP op's mode (0 hide, 1 still, 2 circle, 3 straight): shows the named piece at pos turned by yaw, or
 * hides it; the desert's balloon reads the mode as on its tarp, filling, let go. false if not one of these */
bool memory_prop(const char *name, int mode, v3 pos, float yaw);
void memory_props_hide(void);   /* all of them, when a scene ends */
#endif
