/* Actual config registry/serializer/parser, with an isolated filesystem shim. */
#include "../../../src/pc/configfile.c"
#include "../physics/difficulty_policy.h"
#include <sys/stat.h>
#include <unistd.h>
static int checks;
#define CHECK(x) do { ++checks; if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);} } while(0)
struct CLIOptions gCLIOpts;
struct Mods gLocalMods;
char **gBanAddresses, **gModeratorAddresses;
bool *gBanPerm, *gModerator;
u16 gBanCount, gModeratorCount;
f32 gMasterVolume;
struct PcDebug gPcDebug;
static const char *directory;
const char *fs_get_write_path(const char *name){static char path[2048];snprintf(path,sizeof path,"%s/%s",directory,name);return path;}
fs_file_t *fs_open(const char *name){
    FILE *f=fopen(fs_get_write_path(name),"r");if(!f)return NULL;
    fs_file_t *file=calloc(1,sizeof *file);file->handle=f;return file;
}
const char *fs_readline(fs_file_t *file,char *dst,uint64_t size){return fgets(dst,(int)size,file->handle);}
bool fs_eof(fs_file_t *file){return feof(file->handle);}
void fs_close(fs_file_t *file){fclose(file->handle);free(file);}
void ban_list_add(char *address,bool perm){(void)address;(void)perm;}
void moderator_list_add(char *address,bool perm){(void)address;(void)perm;}
void mods_enable(char *relativePath){(void)relativePath;}
bool network_player_name_valid(char *name){return name&&*name;}
int dynos_pack_get_count(void){return 0;}
bool dynos_pack_get_enabled(int i){(void)i;return false;}
const char *dynos_pack_get_name(int i){(void)i;return "";}
static void load(void){bool error=false;configfile_load_internal("fixture.cfg",&error);CHECK(!error);}
static void content(const char *text){FILE *f=fopen(fs_get_write_path("fixture.cfg"),"w");CHECK(f);fputs(text,f);fclose(f);}
int main(int argc,char **argv){
    CHECK(argc==2);directory=argv[1];CHECK(configRocketBoostMode==0);CHECK(configRocketSurfaceMode==2);
    rocket_bindings_reset();
    configRocketBindings.action[RA_BOOST]=RB_NORTH;
    configRocketBoostMode=1;configfile_save("fixture.cfg");configRocketBoostMode=0;load();CHECK(configRocketBoostMode==1);
    CHECK(configRocketBindings.action[RA_BOOST]==RB_NORTH);
    rocket_bindings_reset();CHECK(configRocketBoostMode==1);
    configRocketBindings.action[RA_JUMP]=RB_RB;
    configRocketBoostMode=0;configfile_save("fixture.cfg");configRocketBoostMode=1;load();CHECK(configRocketBoostMode==0);
    CHECK(configRocketBindings.action[RA_JUMP]==RB_RB);
    content("rocket_boost_mode 255\n");load();CHECK(configRocketBoostMode==0);
    content("rocket_boost_mode -1\n");load();CHECK(configRocketBoostMode==0);
    content("rocket_boost_mode broken\n");configRocketBoostMode=1;load();CHECK(configRocketBoostMode==0);
    content("rocket_boost_mode 1oops\n");load();CHECK(configRocketBoostMode==0);
    content("rocket_boost_mode 1 extra\n");load();CHECK(configRocketBoostMode==0);
    content("show_fps true\n");configRocketBoostMode=1;load();CHECK(configRocketBoostMode==0);
    content("background_gamepad 1\n");load();CHECK(configBackgroundGamepad);
    content("background_gamepad 0\n");load();CHECK(!configBackgroundGamepad);
    CHECK(configRocketSurfaceMode==2);
    configRocketSurfaceMode=1;configRocketBoostMode=0;configfile_save("fixture.cfg");
    configRocketSurfaceMode=0;configRocketBoostMode=1;load();CHECK(configRocketSurfaceMode==1&&configRocketBoostMode==0);
    configRocketSurfaceMode=0;configRocketBoostMode=1;configfile_save("fixture.cfg");
    configRocketSurfaceMode=1;configRocketBoostMode=0;load();CHECK(configRocketSurfaceMode==0&&configRocketBoostMode==1);
    for(unsigned mode=0;mode<3;mode++){
        char text[80];snprintf(text,sizeof text,"rocket_surface_mode %u\n",mode);content(text);load();
        CHECK(configRocketSurfaceMode==mode);configfile_save("fixture.cfg");
        configRocketSurfaceMode=99;load();CHECK(configRocketSurfaceMode==mode);
        CHECK(configRocketBindings.action[RA_JUMP]==RB_RB);
    }
    const char *invalid[]={"255","-1","broken","1oops","1 extra"};
    for(unsigned i=0;i<5;i++){char text[80];snprintf(text,sizeof text,"rocket_surface_mode %s\n",invalid[i]);content(text);configRocketSurfaceMode=0;load();CHECK(configRocketSurfaceMode==2);}
    content("show_fps true\n");configRocketSurfaceMode=0;load();CHECK(configRocketSurfaceMode==2);
    CHECK(configRocketSoundMode==1);
    for(unsigned mode=0;mode<2;mode++){
        configRocketSoundMode=mode;configfile_save("fixture.cfg");configRocketSoundMode=1-mode;load();CHECK(configRocketSoundMode==mode);
        CHECK(configRocketSurfaceMode==2);
    }
    for(unsigned i=0;i<5;i++){char text[80];snprintf(text,sizeof text,"rocket_sound_mode %s\n",invalid[i]);content(text);configRocketSoundMode=0;load();CHECK(configRocketSoundMode==1);}
    content("show_fps true\n");configRocketSoundMode=0;load();CHECK(configRocketSoundMode==1);
    CHECK(configRocketCameraMode==1);
    for(unsigned mode=0;mode<2;mode++){
        configRocketCameraMode=mode;configfile_save("fixture.cfg");configRocketCameraMode=1-mode;load();CHECK(configRocketCameraMode==mode);
    }
    for(unsigned i=0;i<5;i++){char text[80];snprintf(text,sizeof text,"rocket_camera_mode %s\n",invalid[i]);content(text);configRocketCameraMode=0;load();CHECK(configRocketCameraMode==1);}
    content("show_fps true\n");configRocketCameraMode=0;load();CHECK(configRocketCameraMode==1);
    CHECK(configRocketSpeedPercent==100);
    for(unsigned percent=50;percent<=100;percent++){
        configRocketSpeedPercent=percent;configfile_save("fixture.cfg");configRocketSpeedPercent=0;
        load();CHECK(configRocketSpeedPercent==percent);
    }
    const char *badSpeed[]={"0","49","101","-75","75oops","75 extra","broken","99999999999999999999999999","75.0","+75"};
    for(unsigned i=0;i<sizeof badSpeed/sizeof *badSpeed;i++){
        char text[128];snprintf(text,sizeof text,"rocket_speed_percent %s\n",badSpeed[i]);
        content(text);configRocketSpeedPercent=75;load();CHECK(configRocketSpeedPercent==100);
    }
    content("show_fps true\n");configRocketSpeedPercent=75;load();CHECK(configRocketSpeedPercent==100);
    CHECK(configRocketJumpPercent==100);
    for(unsigned percent=30;percent<=100;percent++){
        configRocketJumpPercent=percent;configRocketSpeedPercent=75;
        configfile_save("fixture.cfg");configRocketJumpPercent=0;configRocketSpeedPercent=100;
        load();CHECK(configRocketJumpPercent==percent&&configRocketSpeedPercent==75);
    }
    const char *badJump[]={"0","29","101","-50","50oops","50 extra","broken","99999999999999999999999999","50.0","+50"};
    for(unsigned i=0;i<sizeof badJump/sizeof *badJump;i++){
        char text[128];snprintf(text,sizeof text,"rocket_jump_height_percent %s\nrocket_speed_percent 100\n",badJump[i]);
        content(text);configRocketJumpPercent=50;load();CHECK(configRocketJumpPercent==100&&configRocketSpeedPercent==100);
    }
    content("rocket_speed_percent 100\nrocket_camera_mode 0\n");configRocketJumpPercent=50;load();
    CHECK(configRocketJumpPercent==100&&configRocketSpeedPercent==100&&configRocketCameraMode==0);
    /* Derived presets never replace saved custom values, including old files. */
    content("rocket_speed_percent 88\nrocket_jump_height_percent 67\n");load();
    CHECK(rocket_difficulty_for(configRocketSpeedPercent,configRocketJumpPercent)==ROCKET_CUSTOM);
    CHECK(configfile_save_atomic("fixture.cfg"));configRocketSpeedPercent=0;configRocketJumpPercent=0;load();
    CHECK(configRocketSpeedPercent==88&&configRocketJumpPercent==67);
    for(unsigned preset=0;preset<3;preset++){
        unsigned speed,jump;CHECK(rocket_difficulty_values(preset,&speed,&jump));
        configRocketSpeedPercent=speed;configRocketJumpPercent=jump;CHECK(configfile_save_atomic("fixture.cfg"));
        configRocketSpeedPercent=0;configRocketJumpPercent=0;load();
        CHECK(rocket_difficulty_for(configRocketSpeedPercent,configRocketJumpPercent)==preset);
    }
    /* A failed staged write cannot leave half of a saved preset. */
    char blocked[2048];snprintf(blocked,sizeof blocked,"%s/fixture.cfg.tmp",directory);CHECK(!mkdir(blocked,0700));
    configRocketSpeedPercent=100;configRocketJumpPercent=100;CHECK(!configfile_save_atomic("fixture.cfg"));
    load();CHECK(configRocketSpeedPercent==50&&configRocketJumpPercent==30);CHECK(!rmdir(blocked));
    content("show_fps true\n");load();CHECK(rocket_difficulty_for(configRocketSpeedPercent,configRocketJumpPercent)==ROCKET_EASY);
    content("rocket_speed_percent 49\nrocket_jump_height_percent 29\n");load();
    CHECK(rocket_difficulty_for(configRocketSpeedPercent,configRocketJumpPercent)==ROCKET_EASY);
    content("rocket_speed_percent 88\n");load();CHECK(configRocketSpeedPercent==88&&configRocketJumpPercent==100);
    content("rocket_jump_height_percent 30\n");load();CHECK(configRocketSpeedPercent==100&&configRocketJumpPercent==30);
    printf("boost, surface, sound and camera persistence: %d checks passed\n",checks);return 0;
}
