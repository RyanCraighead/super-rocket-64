/* Actual settings widget callbacks with inert UI drawing/allocation services. */
#include "../../../src/pc/djui/djui_rocket_boost.c"
unsigned configRocketSoundMode;
void rocket_audio_stop(void){}
struct UiRecord {struct DjuiBase *base;const char *label;unsigned *value;void (*changed)(struct DjuiBase*);};
static struct UiRecord ui[8];static unsigned uiCount;
static struct DjuiSelectionbox boxes[4];static unsigned boxesUsed;
static struct DjuiSlider sliders[2];static unsigned slidersUsed;
static struct DjuiRect rows[5];static unsigned rowsUsed;
static struct DjuiText labels[5];static unsigned labelsUsed;
struct DjuiSelectionbox *djui_selectionbox_create(struct DjuiBase *parent,const char *label,char **choices,u8 count,unsigned *value,void (*changed)(struct DjuiBase*)){
    (void)parent;(void)choices;CHECK(boxesUsed<4);struct DjuiSelectionbox *box=&boxes[boxesUsed++];
    box->value=value;box->choiceCount=count;ui[uiCount++]=(struct UiRecord){&box->base,label,value,changed};return box;
}
struct DjuiSlider *djui_slider_create(struct DjuiBase *parent,const char *label,unsigned *value,unsigned min,unsigned max,void (*changed)(struct DjuiBase*)){
    (void)parent;CHECK(slidersUsed<2);struct DjuiSlider *slider=&sliders[slidersUsed++];
    slider->value=value;slider->min=min;slider->max=max;ui[uiCount++]=(struct UiRecord){&slider->base,label,value,changed};return slider;
}
struct DjuiRect *djui_rect_container_create(struct DjuiBase *parent,f32 height){(void)parent;(void)height;CHECK(rowsUsed<5);return &rows[rowsUsed++];}
struct DjuiText *djui_text_create(struct DjuiBase *parent,const char *message){(void)parent;CHECK(labelsUsed<5);labels[labelsUsed].message=(char*)message;return &labels[labelsUsed++];}
void djui_text_set_text(struct DjuiText *text,const char *message){text->message=(char*)message;}
/* Layout is exercised by tests/menu-scroll; this fixture only tests rules. */
f32 djui_text_measure_height(struct DjuiText *text,f32 width){(void)text;(void)width;return 96;}
void djui_base_set_enabled(struct DjuiBase *base,bool enabled){base->enabled=enabled;}
void djui_base_set_size_type(struct DjuiBase *b,enum DjuiScreenValueType x,enum DjuiScreenValueType y){(void)b;(void)x;(void)y;}
void djui_base_set_size(struct DjuiBase *b,f32 w,f32 h){(void)b;(void)w;(void)h;}
void djui_selectionbox_update_value(struct DjuiBase *base){(void)base;}
void djui_slider_update_value(struct DjuiBase *base){(void)base;}
static void ui_refresh(void){for(unsigned i=0;i<uiCount;i++)if(ui[i].base->on_render_pre)ui[i].base->on_render_pre(ui[i].base,NULL);}
static void ui_choose(unsigned widget,unsigned value){*ui[widget].value=value;ui[widget].changed(ui[widget].base);ui_refresh();}
static void difficulty_tests(void){
    gNetworkType=NT_NONE;gCLIOpts.offline=false;configRocketSpeedPercent=75;configRocketJumpPercent=50;
    unsigned boost=configRocketBoostMode,surface=configRocketSurfaceMode;
    CHECK(rocket_difficulty()==ROCKET_MEDIUM);
    CHECK(!rocket_difficulty_set(ROCKET_CUSTOM)&&!rocket_difficulty_set(99));
    djui_rocket_boost_create(NULL);CHECK(uiCount==6);CHECK(!strcmp(ui[0].label,"Octane difficulty"));
    CHECK(*ui[0].value==ROCKET_MEDIUM&&ui[0].base->enabled);
    CHECK(sliders[0].min==50&&sliders[1].min==30&&sliders[1].max==100);
    for(unsigned preset=0;preset<3;preset++){
        int before=saves;ui_choose(0,preset);unsigned speed,jump;
        CHECK(rocket_difficulty_values(preset,&speed,&jump));
        CHECK(configRocketSpeedPercent==speed&&configRocketJumpPercent==jump&&rocket_difficulty()==preset);
        CHECK(savedSpeed==speed&&savedJump==jump&&saves==before+1);
        CHECK(*ui[1].value==speed&&*ui[2].value==jump);
        CHECK(configRocketBoostMode==boost&&configRocketSurfaceMode==surface);
        before=saves;ui_choose(0,preset);CHECK(saves==before);
    }
    ui_choose(1,73);CHECK(*ui[0].value==ROCKET_CUSTOM&&rocket_jump_percent()==30);
    ui_choose(2,62);CHECK(*ui[0].value==ROCKET_CUSTOM&&rocket_speed_percent()==73);
    int before=saves;ui_choose(0,ROCKET_CUSTOM);CHECK(saves==before&&rocket_speed_percent()==73&&rocket_jump_percent()==62);
    ui_choose(1,75);ui_choose(2,50);CHECK(*ui[0].value==ROCKET_MEDIUM);
    saveFails=1;before=saves;ui_choose(0,ROCKET_HARD);saveFails=0;
    CHECK(saves==before&&rocket_difficulty()==ROCKET_MEDIUM&&strstr(rocket_difficulty_scope_label(),"Could not save"));
    host();rocket_boost_session_reset();struct Packet initialPreset=join();u32 beforeRevision=rocket_rule_revision();
    struct Packet failed={0};capturedPacket=&failed;saveFails=1;before=saves;
    ui_choose(0,ROCKET_HARD);saveFails=0;capturedPacket=NULL;
    CHECK(!failed.dataLength&&saves==before&&rocket_rule_revision()==beforeRevision);
    CHECK(rocket_speed_percent()==75&&rocket_jump_percent()==50);
    struct Packet hard={0};before=saves;capturedPacket=&hard;ui_choose(0,ROCKET_HARD);capturedPacket=NULL;
    CHECK(saves==before+1&&rocket_rule_revision()==beforeRevision+1);
    CHECK(hard.dataLength&&hard.buffer[hard.dataLength-2]==50&&hard.buffer[hard.dataLength-1]==30);
    struct Packet latePreset=join();struct Packet easy={0};capturedPacket=&easy;ui_choose(0,ROCKET_EASY);capturedPacket=NULL;
    CHECK(easy.buffer[easy.dataLength-2]==100&&easy.buffer[easy.dataLength-1]==100);
    configRocketSpeedPercent=88;configRocketJumpPercent=67;before=saves;
    accept_join(initialPreset);ui_refresh();CHECK(*ui[0].value==ROCKET_MEDIUM);
    CHECK(!ui[0].base->enabled&&!ui[1].base->enabled&&!ui[2].base->enabled);
    for(unsigned preset=0;preset<4;preset++){ui_choose(0,preset);CHECK(rocket_difficulty()==ROCKET_MEDIUM&&saves==before);}
    receive(hard);ui_refresh();CHECK(*ui[0].value==ROCKET_HARD&&*ui[1].value==50&&*ui[2].value==30);
    receive(easy);ui_refresh();CHECK(*ui[0].value==ROCKET_EASY);
    receive(hard);CHECK(rocket_difficulty()==ROCKET_EASY); // stale pair rejected together
    accept_join(latePreset);ui_refresh();CHECK(*ui[0].value==ROCKET_HARD&&*ui[1].value==50&&*ui[2].value==30);
    CHECK(configRocketSpeedPercent==88&&configRocketJumpPercent==67&&saves==before);
    gNetworkType=NT_NONE;ui_refresh();CHECK(*ui[0].value==ROCKET_CUSTOM&&*ui[1].value==88&&*ui[2].value==67);
    CHECK(ui[0].base->enabled&&ui[1].base->enabled&&ui[2].base->enabled);
}
static void surface_rule_tests(void){
    host();rocket_boost_session_reset();configRocketSurfaceMode=2;ui_refresh();
    CHECK(*ui[5].value==0&&ui[5].base->enabled&&boxes[3].choiceCount==3);
    struct Packet initial=join(),on={0},car={0},off={0};u32 first=rocket_rule_revision();
    capturedPacket=&on;ui_choose(5,1);capturedPacket=NULL;
    CHECK(configRocketSurfaceMode==1&&rocket_rule_revision()==first+1);
    capturedPacket=&car;ui_choose(5,2);capturedPacket=NULL;CHECK(configRocketSurfaceMode==0);
    capturedPacket=&off;ui_choose(5,0);capturedPacket=NULL;CHECK(configRocketSurfaceMode==2);
    CHECK(off.dataLength==on.dataLength&&off.dataLength==car.dataLength&&off.buffer[off.dataLength-3]==2);
    struct Packet late=join();int saved=saves;configRocketSurfaceMode=1;
    accept_join(initial);ui_refresh();CHECK(rocket_surface_mode()==2&&*ui[5].value==0&&!ui[5].base->enabled);
    for(unsigned i=0;i<3;i++){ui_choose(5,i);CHECK(rocket_surface_mode()==2&&saves==saved&&configRocketSurfaceMode==1);}
    receive(on);ui_refresh();CHECK(rocket_surface_mode()==1&&*ui[5].value==1);
    receive(car);ui_refresh();CHECK(rocket_surface_mode()==0&&*ui[5].value==2);
    receive(off);ui_refresh();CHECK(rocket_surface_mode()==2&&*ui[5].value==0);
    receive(on);CHECK(rocket_surface_mode()==2); // old revision
    struct Packet bad=on;bad.localIndex=2;bad.addr=(void*)2;bad.cursor=3;bad.buffer[6]=200;
    packet_receive(&bad);CHECK(rocket_surface_mode()==2); // another peer
    bad=off;bad.buffer[bad.dataLength-3]=3;bad.buffer[6]=200;receive(bad);CHECK(rocket_surface_mode()==2);
    accept_join(late);CHECK(rocket_surface_mode()==2&&configRocketSurfaceMode==1&&saves==saved);
    gNetworkType=NT_NONE;ui_refresh();CHECK(rocket_surface_mode()==1&&*ui[5].value==1&&ui[5].base->enabled);
}
