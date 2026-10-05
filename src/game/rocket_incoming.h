#ifndef ROCKET_INCOMING_H
#define ROCKET_INCOMING_H
struct Object;
/* -1: use native Mario geometry; 0/1: owned car miss/overlap. This does not
 * apply damage. Both host and client evaluate only their own local car. */
int rocket_incoming_overlap(struct Object *a, struct Object *b, int hurtbox);
#endif
