/* Wing composes one shared native-cap authority with the ordinary boost rule. */
#include "rocket_wing.h"
#include "sm64.h"
#include "pc/rocket_boost.h"
int rocket_wing_active(unsigned index) { return !!(rocket_caps_active_flags(index) & MARIO_WING_CAP); }
int rocket_wing_boost_mode(void) {
    return rocket_wing_active(0) ? ROCKET_BOOST_INFINITE : rocket_boost_mode();
}
