#ifndef ROCKET_DIFFICULTY_POLICY_H
#define ROCKET_DIFFICULTY_POLICY_H
/* Derived from the two saved values. No preset key can replace custom choices. */
enum RocketDifficulty { ROCKET_EASY, ROCKET_MEDIUM, ROCKET_HARD, ROCKET_CUSTOM };
static inline unsigned rocket_difficulty_for(unsigned speed,unsigned jump) {
    if(speed==100&&jump==100)return ROCKET_EASY;
    if(speed==75&&jump==50)return ROCKET_MEDIUM;
    if(speed==50&&jump==30)return ROCKET_HARD;
    return ROCKET_CUSTOM;
}
static inline int rocket_difficulty_values(unsigned preset,unsigned *speed,unsigned *jump) {
    static const unsigned values[3][2]={{100,100},{75,50},{50,30}};
    if(preset>=ROCKET_CUSTOM||!speed||!jump)return 0;
    *speed=values[preset][0];*jump=values[preset][1];return 1;
}
#endif
