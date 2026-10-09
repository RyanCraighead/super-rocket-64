/* Actual mixer/event code and RocketSim, no audio device or proprietary fixtures. */
#include "../../../src/pc/rocket_audio.cpp"
#include <cstdlib>
#include <limits>
static int checks, jumpEvents, flipEvents, doubleEvents, marioBoostFrames, marioStops;
#define CHECK(x) do { ++checks; if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);} } while(0)
extern "C" void rocket_audio_mario(unsigned e,int boost,int stop) {
    jumpEvents+=!!(e&ROCKET_SOUND_JUMP);flipEvents+=!!(e&ROCKET_SOUND_FLIP);
    doubleEvents+=!!(e&ROCKET_SOUND_DOUBLE_JUMP);marioBoostFrames+=!!boost;marioStops+=!!stop;
}
static void counts(){jumpEvents=flipEvents=doubleEvents=marioBoostFrames=marioStops=0;}
static bool quiet(){for(const auto &v:voices)if(v.playing)return false;return true;}
int main(int argc,char **argv) {
    rocket_audio_load("/nonexistent-car-audio-test");CHECK(!rocket_audio_available());
    RocketSnapshot s={};s.ticks=4;s.jumped=1;
    rocket_audio_update(&s,1,1);CHECK(jumpEvents==1); // Missing car audio falls back once.
    rocket_audio_update(&s,1,1);CHECK(jumpEvents==1); // Duplicate/render snapshot.
    s.ticks=8;rocket_audio_update(&s,1,1);CHECK(jumpEvents==1); // Held jump.
    s.ticks=12;s.flipped=1;rocket_audio_update(&s,1,1);CHECK(flipEvents==1&&doubleEvents==0);
    s.ticks=16;s.boosting=1;rocket_audio_update(&s,1,0);CHECK(marioBoostFrames==0); // Headless/blocked.
    rocket_audio_reset();counts();s={};s.ticks=4;s.double_jumped=1;
    rocket_audio_update(&s,0,1);CHECK(doubleEvents==1);
    rocket_audio_stop();s.ticks=8;rocket_audio_update(&s,0,1);CHECK(doubleEvents==1); // Pause does not replay.
    // Synthetic stereo signals test the actual saturating mix and bounded loop voices.
    rocket_audio_reset();counts();ready=true;
    for(auto &v:voices)v.samples={10000,-10000,10000,-10000};
    s={};s.ticks=4;s.boosting=1;rocket_audio_update(&s,1,1);
    CHECK(voices[BOOST_START].playing&&voices[BOOST_LOOP].playing&&marioBoostFrames==0);
    int16_t out[8]={};rocket_audio_mix(out,2,.25f);CHECK(out[0]==2500&&out[1]==-2500);
    CHECK(!voices[BOOST_START].playing&&voices[BOOST_LOOP].playing);
    rocket_audio_mix(out,2,0);CHECK(out[0]==2500); // SFX mute adds nothing.
    s.ticks=8;rocket_audio_update(&s,1,1);CHECK(!voices[BOOST_START].playing); // Held boost does not restart.
    s.ticks=12;s.boosting=0;rocket_audio_update(&s,1,1);
    CHECK(!voices[BOOST_LOOP].playing&&!voices[BOOST_START].playing&&voices[BOOST_END].playing);
    s.ticks=16;rocket_audio_update(&s,1,1);rocket_audio_mix(out,4,1);CHECK(quiet());
    s.ticks=20;s.boosting=1;rocket_audio_update(&s,1,1);rocket_audio_stop();CHECK(quiet());
    s.ticks=24;rocket_audio_update(&s,1,1);CHECK(voices[BOOST_LOOP].playing);
    s.ticks=28;rocket_audio_update(&s,0,1);CHECK(quiet()&&marioBoostFrames==1); // Mode change stops car voices.
    s.ticks=32;rocket_audio_update(&s,1,1);CHECK(voices[BOOST_LOOP].playing);
    s.ticks=36;rocket_audio_update(&s,1,0);CHECK(quiet()); // Warp/pause/despawn/blocked use this stop path.
    start(JUMP);out[0]=32000;out[1]=-32000;rocket_audio_mix(out,1,1);
    CHECK(out[0]==32767&&out[1]==-32768);rocket_audio_shutdown();CHECK(quiet()&&!ready);
    // Every decoded effect gets the same single 0.5 mix attenuation. Preserve
    // the existing music/native-audio bus and all user SFX gain/mute choices.
    const struct {float userGain;int added;} levels[]={
        {0,0},{.25f,1500},{.5f,3000},{1,6000},{2,6000},{-1,0},
        {std::numeric_limits<float>::quiet_NaN(),0}};
    for(unsigned effect=0;effect<COUNT;effect++)for(auto level:levels){
        silence();voices[effect].samples={12000,-12000};start(effect);
        int16_t bus[]={1234,-2345};rocket_audio_mix(bus,1,level.userGain);
        CHECK(bus[0]==1234+level.added);CHECK(bus[1]==-2345-level.added);
    }
    silence();int16_t nativeBus[]={2468,-1357};rocket_audio_mix(nativeBus,1,1);
    CHECK(nativeBus[0]==2468&&nativeBus[1]==-1357);rocket_audio_shutdown();
    // Real physics transitions: jump/flip have one edge despite held/repeated ticks;
    // actual fuel exhaustion ends thrust even while the input remains held.
    RocketWorld *w=rocket_world_create();CHECK(w);
    RocketTriangle floor[]={{{{-20000,0,-20000},{20000,0,20000},{20000,0,-20000}},0},
        {{{-20000,0,-20000},{-20000,0,20000},{20000,0,20000}},0}};
    CHECK(rocket_world_mesh(w,0,floor,2));float p[]={0,40,0},v[]={0,0,0};CHECK(rocket_world_reset(w,p,v,0));
    RocketInput input={};uint64_t frame=0;
    auto step=[&](){CHECK(rocket_world_frame(w,++frame,&input,0,0)==4);CHECK(rocket_world_snapshot(w,&s));rocket_audio_update(&s,0,1);};
    for(int i=0;i<30;i++)step();CHECK(s.grounded);counts();
    input.jump=1;for(int i=0;i<8;i++)step();CHECK(jumpEvents==1);
    input.jump=0;step();input.jump=1;input.pitch=-1;
    for(int i=0;i<8;i++)step();CHECK(flipEvents==1&&doubleEvents==0&&jumpEvents==1);
    rocket_audio_stop();rocket_world_interrupt(w);step();CHECK(jumpEvents==1&&flipEvents==1);
    input={};CHECK(rocket_world_reset(w,p,v,0));rocket_audio_reset();frame=0;counts();
    for(int i=0;i<30;i++)step();CHECK(rocket_world_collect_coin(w));input.boost=1;
    int thrust=0;for(int i=0;i<100;i++){step();thrust+=s.boosting;}
    CHECK(thrust>0&&marioBoostFrames==thrust&&!s.boosting&&s.boost==0);
    rocket_world_destroy(w);rocket_audio_shutdown();
    // Optional maintainer-only proof with owned decoded files outside the repository.
    if(argc==2){rocket_audio_load(argv[1]);CHECK(rocket_audio_available());for(const auto &v:voices)CHECK(!v.samples.empty());rocket_audio_shutdown();}
    std::printf("car audio: %d mixer/event/real-physics checks passed\n",checks);
}
