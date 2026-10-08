#ifndef ROCKET_SPEED_FIXTURE_STUBS_H
#define ROCKET_SPEED_FIXTURE_STUBS_H
#include "penguin_fixture_stubs.h"
#include "quicksand_fixture_stubs.h"
#include "pole_fixture_stubs.h"
#include "beam_fixture_stubs.h"
/* Explicit 100% rule boundary for pre-existing component tests. Tests of
 * authenticated rules link the real service instead. Never built into game. */
#include "../../../src/pc/rocket_boost.h"
#ifndef ROCKET_BOOST_REAL_TEST
static unsigned fixtureSpeedPercent=100;
static uint32_t fixtureRuleRevision;
unsigned rocket_speed_percent(void){return fixtureSpeedPercent;}
float rocket_speed_scale(void){return rocket_speed_multiplier(fixtureSpeedPercent);}
uint32_t rocket_rule_revision(void){return fixtureRuleRevision;}
#endif
int rocket_runtime_rule_ready(void){return 1;}
#endif
