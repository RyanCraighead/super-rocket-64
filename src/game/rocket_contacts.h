#ifndef SM64_ROCKET_CONTACTS_H
#define SM64_ROCKET_CONTACTS_H
#include <PR/ultratypes.h>
struct Object;
/* One outgoing decision boundary after native tangibility countdown and before
 * player incoming handlers. Native behavior consumers retain all consequences. */
void rocket_contacts_prepare(void);
int rocket_contacts_bobomb_yaw(struct Object *object,s16 *yaw);
void rocket_contacts_forget(struct Object *object);
#endif
