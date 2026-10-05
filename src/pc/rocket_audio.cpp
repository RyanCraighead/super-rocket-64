/* Optional, locally extracted car effects mixed into the existing 32 kHz output.
 * No proprietary samples, audio device, network events or background downloads. */
#include "rocket_audio.h"
#include "utils/oot_asset_path.h"
#include "utils/rocket_sha256.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <vector>

namespace {
struct Clip { const char *name; size_t size; const char *sha; };
const Clip profile[] = {
    {"jump",432044,"9fcb49f53fe36641796f93963d8ebd96cf7052456597c1d6863a40c82221d065"},
    {"flip",460844,"fbaa08fa9b9f7ff5301a2cf976cafc3b318484afdbcd9578e90299488e20d94e"},
    {"double_jump",437036,"78206288b9d5d7d8a50d3970fc790a75bfd3c9b82338367201aba866e8829bcd"},
    {"boost_start",327692,"9f88bdf4d85765d195095282a84418efbae3ce863b067c30a91284f203e87be8"},
    {"boost_loop",334380,"68e0379c6da74ab2546bc962e57aabe0326c9cd5d105a947bb1a162132b09087"},
    {"boost_end",200076,"98884072d889741c8c700eef550918f1153e5680cc98251f0023a0fd6cdeac40"}
};
enum { JUMP, FLIP, DOUBLE_JUMP, BOOST_START, BOOST_LOOP, BOOST_END, COUNT };
struct Voice { std::vector<int16_t> samples; size_t cursor=0; bool playing=false; };
std::array<Voice,COUNT> voices;
std::mutex mutex;
bool ready=false, boosting=false;
unsigned lastStyle=2;
RocketSnapshot previous={};
void silence() { for(auto &v:voices)v.playing=false; boosting=false; }
void start(unsigned n) { voices[n].cursor=0; voices[n].playing=true; }
uint32_t le(const std::vector<unsigned char> &d,size_t p,unsigned n) {
    uint32_t v=0;for(unsigned i=0;i<n;i++)v|=uint32_t(d.at(p+i))<<(8*i);return v;
}
std::vector<int16_t> convert(const std::vector<unsigned char> &d) {
    // Exact hashes are verified before this fixed, bounded PCM layout is read.
    if(d.size()<48 || std::string(d.begin(),d.begin()+4)!="RIFF" ||
        std::string(d.begin()+8,d.begin()+16)!="WAVEfmt " || le(d,16,4)!=16 ||
        le(d,20,2)!=1 || le(d,22,2)!=2 || le(d,24,4)!=48000 || le(d,34,2)!=16 ||
        std::string(d.begin()+36,d.begin()+40)!="data" || le(d,40,4)!=d.size()-44 || (d.size()-44)%4)
        throw std::runtime_error("invalid car PCM format");
    size_t frames=(d.size()-44)/4;
    std::vector<int16_t> out(((frames*2+2)/3)*2);
    for(size_t i=0;i<out.size()/2;i++)for(unsigned c=0;c<2;c++) {
        size_t p=i*3/2,q=std::min(p+1,frames-1);
        int a=int16_t(le(d,44+p*4+c*2,2)),b=int16_t(le(d,44+q*4+c*2,2));
        out[i*2+c]=int16_t(i%2?(a+b)/2:a);
    }
    return out;
}
}
extern "C" void rocket_audio_load(const char *base) {
    rocket_audio_shutdown();
    try {
        std::array<Voice,COUNT> loaded;
        std::string root=oot_asset_path::canonical(base);
        for(unsigned i=0;i<COUNT;i++) {
            auto data=oot_asset_path::readFile(oot_asset_path::child(root,
                std::string("audio/")+profile[i].name+".wav",240),profile[i].size,profile[i].size);
            if(rocket_assets::sha256(data)!=profile[i].sha)throw std::runtime_error("car sound checksum mismatch");
            loaded[i].samples=convert(data);
        }
        std::lock_guard<std::mutex> lock(mutex);voices=std::move(loaded);ready=true;
        std::fprintf(stderr,"Local car jump, flip and boost sounds verified\n");
    } catch(const std::exception &) {
        std::fprintf(stderr,"Car sounds unavailable; using Mario sounds. Select Rocket League in Setup to add/repair them.\n");
    }
}
extern "C" int rocket_audio_available(void) { std::lock_guard<std::mutex> lock(mutex);return ready; }
extern "C" void rocket_audio_stop(void) {
    {std::lock_guard<std::mutex> lock(mutex);silence();}
    rocket_audio_mario(0,0,1);
}
extern "C" void rocket_audio_reset(void) {
    rocket_audio_stop(); previous={};lastStyle=2;
}
extern "C" void rocket_audio_shutdown(void) {
    rocket_audio_reset();
    std::lock_guard<std::mutex> lock(mutex);ready=false;for(auto &v:voices)v.samples.clear();
}
extern "C" void rocket_audio_update(const RocketSnapshot *s,unsigned style,int active) {
    if(!s || !active) { rocket_audio_stop(); if(s)previous=*s; return; }
    if(s->ticks<=previous.ticks)return;
    unsigned events=0;
    if(s->jumped&&!previous.jumped)events|=ROCKET_SOUND_JUMP;
    if(s->flipped&&!previous.flipped)events|=ROCKET_SOUND_FLIP;
    else if(s->double_jumped&&!previous.double_jumped)events|=ROCKET_SOUND_DOUBLE_JUMP;
    previous=*s;
    bool car=style==1&&rocket_audio_available();
    if(lastStyle!=unsigned(car)){rocket_audio_stop();lastStyle=unsigned(car);}
    if(!car){rocket_audio_mario(events,s->boosting,0);return;}
    std::lock_guard<std::mutex> lock(mutex);
    if(events&ROCKET_SOUND_JUMP)start(JUMP);
    if(events&ROCKET_SOUND_FLIP)start(FLIP);
    if(events&ROCKET_SOUND_DOUBLE_JUMP)start(DOUBLE_JUMP);
    if(s->boosting&&!boosting){voices[BOOST_END].playing=false;start(BOOST_START);start(BOOST_LOOP);}
    if(!s->boosting&&boosting){voices[BOOST_START].playing=false;voices[BOOST_LOOP].playing=false;start(BOOST_END);}
    boosting=!!s->boosting;
}
extern "C" void rocket_audio_mix(int16_t *out,size_t frames,float gain) {
    if(!out)return;
    if(!std::isfinite(gain))gain=0;
    gain=std::max(0.f,std::min(1.f,gain));
    std::lock_guard<std::mutex> lock(mutex);
    for(size_t i=0;i<frames*2;i++) {
        int sample=0;
        for(unsigned n=0;n<COUNT;n++) {
            auto &v=voices[n];if(!v.playing||v.samples.empty())continue;
            sample+=v.samples[v.cursor++];
            if(v.cursor==v.samples.size()){v.cursor=0;if(n!=BOOST_LOOP)v.playing=false;}
        }
        out[i]=int16_t(std::max(-32768,std::min(32767,int(out[i])+int(sample*gain))));
    }
}
