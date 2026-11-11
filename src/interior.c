/* interior.c: a stub until the town fills the table (see interior.h) */
#include "interior.h"
void interior_init(t3_scene *scene) { (void)scene; }
int interior_count(void) { return 0; }
const char *interior_name(int i) { (void)i; return ""; }
bool interior_door(int i, v3 *o, float *yo, v3 *in, float *yi) { (void)i; (void)o; (void)yo; (void)in; (void)yi; return false; }
void interior_enter(int i) { (void)i; }
void interior_exit(void) {}
int interior_current(void) { return -1; }
bool interior_contains(v3 p) { (void)p; return false; }
float interior_ceiling(void) { return 3.f; }
