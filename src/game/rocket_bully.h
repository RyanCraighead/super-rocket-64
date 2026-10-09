#ifndef ROCKET_BULLY_H
#define ROCKET_BULLY_H
struct MarioState;
struct Object;
/* Only a deliberate local forward ram supplies a native fast-attack intent. */
int rocket_bully_ram(struct MarioState *m,struct Object *bully);
/* One native consequence per continuous car/Bully contact, rearmed by a
 * measured separation. No global damage immunity or save/network fields. */
void rocket_bully_prepare(void);
void rocket_bully_forget(struct Object *bully);
int rocket_bully_repeat(struct MarioState *m,struct Object *bully);
void rocket_bully_record(struct MarioState *m,struct Object *bully,int outgoing);
void rocket_bully_response(struct MarioState *m,struct Object *bully);
int rocket_bully_car_contact(struct MarioState *m,struct Object *bully);
void rocket_bully_separate(struct Object *bully);
#endif
