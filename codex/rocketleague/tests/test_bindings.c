#include "../../../src/pc/rocket_bindings.h"
#include <assert.h>
#include <limits.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

static RocketInput mapped(const RocketBindings *b, RocketPadSample raw) {
    RocketGamepad pad={0};RocketInput keyboard={0};
    pad.connected=pad.isolated=1;
    rocket_bindings_apply(b,&raw,&pad);
    return rocket_gamepad_merge(&keyboard,&pad);
}
static void parse(char *line) {
    char *tokens[20];int n=0;
    for(char *t=strtok(line," \r\n");t&&n<20;t=strtok(NULL," \r\n"))tokens[n++]=t;
    rocket_bindings_read(tokens,n);
}
int main(void) {
    RocketBindings b=rocket_default_bindings;
    RocketPadSample raw={0};RocketInput out;
    assert(rocket_bindings_valid(&b));
    assert(rocket_bindings_conflicts(&b)==((1u<<RA_SLIDE)|(1u<<RA_ROLL)));
    raw.right_trigger=32767;raw.buttons=3;raw.left_x=32767;raw.left_y=-32768;
    out=mapped(&b,raw);
    assert(out.throttle==1&&out.steer==-1&&out.yaw==-1&&out.pitch==-1&&out.jump&&out.boost);
    assert(!out.powerslide&&!out.roll);
    raw.buttons=4;out=mapped(&b,raw);assert(out.roll==-1&&!out.yaw&&out.powerslide);
    raw=(RocketPadSample){0};raw.left_trigger=16384;
    out=mapped(&b,raw);assert(out.throttle<-.4f&&out.throttle>-.5f&&!out.pitch);
    /* Trigger and stick boundary coverage includes asymmetric signed extrema. */
    for(int x=-32768;x<=32767;x+=257) {
        raw.left_x=x;raw.right_x=x;out=mapped(&b,raw);
        assert(out.steer>=-1&&out.steer<=1);
    }
    b.action[RA_BOOST]=RB_RB;b.action[RA_SLIDE]=b.action[RA_ROLL]=RB_LB;
    b.action[RA_ROLL_LEFT]=RB_WEST;b.action[RA_ROLL_RIGHT]=RB_EAST;
    raw=(RocketPadSample){0};raw.buttons=(1u<<10)|(1u<<9);raw.left_x=-32768;
    out=mapped(&b,raw);assert(out.boost&&out.powerslide&&out.roll==1&&!out.jump);
    raw.buttons=1u<<2;out=mapped(&b,raw);assert(out.roll==1&&!out.powerslide&&!out.yaw);
    raw.buttons=1u<<1;out=mapped(&b,raw);assert(out.roll==-1&&!out.boost);
    raw.buttons=6;out=mapped(&b,raw);assert(!out.roll&&!out.yaw);
    b.action[RA_THROTTLE]=RB_NORTH;b.action[RA_BRAKE]=RB_NORTH;
    raw.buttons=1u<<3;out=mapped(&b,raw);assert(!out.throttle);
    assert(rocket_bindings_conflicts(&b)&(1u<<RA_THROTTLE));
    b.action[RA_BRAKE]=RB_NONE;out=mapped(&b,raw);assert(out.throttle==1);
    b.action[RA_THROTTLE]=RB_NONE;out=mapped(&b,raw);assert(!out.throttle);
    b=rocket_default_bindings;b.stick=b.invert_x=b.invert_y=1;
    raw=(RocketPadSample){0};raw.right_x=-32768;raw.right_y=32767;
    out=mapped(&b,raw);assert(out.steer==-1&&out.pitch<-.999f);
    /* Every selectable button drives an action; reserved menu buttons never do. */
    static const int bits[]={-1,0,1,2,3,9,10,7,8,11,12,13,14};
    for(unsigned binding=RB_NONE;binding<RB_COUNT;++binding) {
        b=rocket_default_bindings;b.action[RA_JUMP]=binding;raw=(RocketPadSample){0};
        if(binding==RB_LT)raw.left_trigger=32767;
        else if(binding==RB_RT)raw.right_trigger=32767;
        else if(binding)raw.buttons=1u<<bits[binding];
        out=mapped(&b,raw);assert(out.jump==(binding!=RB_NONE));
    }
    b=rocket_default_bindings;raw=(RocketPadSample){0};raw.buttons=(1u<<4)|(1u<<5)|(1u<<6)|(1u<<20);
    out=mapped(&b,raw);assert(!out.jump&&!out.boost&&!out.powerslide&&!out.throttle);
    raw=(RocketPadSample){0};assert(rocket_bindings_neutral(&raw));
    raw.right_x=5000;assert(!rocket_bindings_neutral(&raw));
    raw.right_x=0;raw.left_trigger=5000;assert(!rocket_bindings_neutral(&raw));
    /* Separate profiles normalize to identical actions on host and client. */
    RocketBindings custom=rocket_default_bindings;custom.action[RA_BOOST]=RB_RB;
    raw=(RocketPadSample){0};raw.buttons=2;raw.right_trigger=24575;
    RocketInput host=mapped(&b,raw);raw.buttons=1u<<10;RocketInput client=mapped(&custom,raw);
    assert(!memcmp(&host,&client,sizeof(host)));
    /* Real serializer/reloader; absent/invalid records leave the old profile intact. */
    configRocketBindings=custom;
    FILE *file;
#ifdef _WIN32
    /* MSVCRT tmpfile() may choose the unwritable drive root. */
    char tempPath[MAX_PATH],tempFile[MAX_PATH];
    DWORD pathLength=GetTempPathA(sizeof(tempPath),tempPath);
    assert(pathLength&&pathLength<sizeof(tempPath));
    assert(GetTempFileNameA(tempPath,"rcb",0,tempFile));file=fopen(tempFile,"w+b");
#else
    file=tmpfile();
#endif
    assert(file);rocket_bindings_write(file);rewind(file);
    char line[512];assert(fgets(line,sizeof(line),file));fclose(file);
#ifdef _WIN32
    assert(!remove(tempFile));
#endif
    rocket_bindings_reset();parse(line);assert(!memcmp(&configRocketBindings,&custom,sizeof(custom)));
    const char *bad[]={"rocket-bindings: 2 14 13 1 6 3 3 0 0 0 0 0",
        "rocket-bindings: 1 14 13 1 99 3 3 0 0 0 0 0",
        "rocket-bindings: 1 14 13 1 -1 3 3 0 0 0 0 0",
        "rocket-bindings: 1 14 13 1 4294967298 3 3 0 0 0 0 0",
        "rocket-bindings: 1 14 13 1 2x 3 3 0 0 0 0 0",
        "rocket-bindings: 1 14 13 1 2 3 3 0 0 2 0 0",
        "rocket-bindings: 1 14 13 1 2 3 3 0 0 0 0",
        "rocket-bindings: 1 14 13 1 2 3 3 0 0 0 0 0 extra"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);++i) {
        strcpy(line,bad[i]);parse(line);assert(!memcmp(&configRocketBindings,&custom,sizeof(custom)));
    }
    b.action[RA_JUMP]=UINT_MAX;assert(!rocket_bindings_valid(&b));
    raw=(RocketPadSample){0};raw.buttons=1;out=mapped(&b,raw);assert(out.jump);
    rocket_bindings_reset();assert(!memcmp(&configRocketBindings,&rocket_default_bindings,sizeof(custom)));
    puts("PASS car bindings: defaults, analog/digital remaps, shared/opposite/unbound actions, air roll, axes, persistence, invalid profiles, local normalization");
}
