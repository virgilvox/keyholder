/* attic_entrance.c: the lab's roped off back stairs (see attic_int.h): two stanchions, a sagging rope, and a
 * small PVC framed danger sign hung on it. Hung under the town group so it shows in the lab only. The stair
 * barrier and the "go upstairs" prompt are the story's and world.c's; this is only the prop. */
#include "attic_int.h"
#include "fx.h"
#include "world.h"

void attic_entrance_build(void) {
  const uint32_t PVC = 0xe9e9e4, POST = 0x8a8f96, ROPE = 0x6b1f1f, SIGN = 0xf2efe6, RED = 0xc62828;
  float z = -32.0f, x0 = -0.35f, x1 = 1.15f;
  fxp_begin(); fxp_layer(FXP_BODY);
  fxp_cyl(v3_(x0, 0, z), 1.02f, 0.03f, 0.03f, 8, POST); fxp_cyl(v3_(x0, 1.0f, z), 0.06f, 0.05f, 0.05f, 8, POST);
  fxp_cyl(v3_(x1, 0, z), 1.02f, 0.03f, 0.03f, 8, POST); fxp_cyl(v3_(x1, 1.0f, z), 0.06f, 0.05f, 0.05f, 8, POST);
  fxp_bar(v3_(x0, 0.92f, z), v3_((x0 + x1) / 2, 0.78f, z), 0.02f, ROPE);
  fxp_bar(v3_((x0 + x1) / 2, 0.78f, z), v3_(x1, 0.92f, z), 0.02f, ROPE);
  float sx = (x0 + x1) / 2, sy = 0.5f;
  fxp_bar(v3_(sx - 0.03f, 0.78f, z), v3_(sx - 0.03f, sy, z), 0.012f, PVC);   /* the hanger */
  fxp_box(v3_(sx, sy - 0.14f, z), v3_(0.5f, 0.32f, 0.02f), SIGN);            /* the panel */
  fxp_box(v3_(sx, sy - 0.02f, z + 0.015f), v3_(0.52f, 0.06f, 0.012f), RED);
  fxp_box(v3_(sx, sy - 0.26f, z + 0.015f), v3_(0.52f, 0.06f, 0.012f), RED);
  fxp_cyl(v3_(sx - 0.27f, sy - 0.30f, z + 0.01f), 0.32f, 0.018f, 0.018f, 6, PVC);
  fxp_cyl(v3_(sx + 0.27f, sy - 0.30f, z + 0.01f), 0.32f, 0.018f, 0.018f, 6, PVC);
  fxp_bar(v3_(sx - 0.28f, sy - 0.30f, z + 0.01f), v3_(sx + 0.28f, sy - 0.30f, z + 0.01f), 0.018f, PVC);
  fxp_bar(v3_(sx - 0.28f, sy + 0.02f, z + 0.01f), v3_(sx + 0.28f, sy + 0.02f, z + 0.01f), 0.018f, PVC);
  world_add_prop(fxp_end());
}
