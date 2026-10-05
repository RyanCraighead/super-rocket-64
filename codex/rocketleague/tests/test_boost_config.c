/* Actual config registry/serializer/parser, with an isolated filesystem shim. */
#include "../../../src/pc/configfile.c"
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
    CHECK(argc==2);directory=argv[1];CHECK(configRocketBoostMode==0);CHECK(configRocketSurfaceMode==1);
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
    CHECK(configRocketSurfaceMode==1);
    configRocketSurfaceMode=1;configRocketBoostMode=0;configfile_save("fixture.cfg");
    configRocketSurfaceMode=0;configRocketBoostMode=1;load();CHECK(configRocketSurfaceMode==1&&configRocketBoostMode==0);
    configRocketSurfaceMode=0;configRocketBoostMode=1;configfile_save("fixture.cfg");
    configRocketSurfaceMode=1;configRocketBoostMode=0;load();CHECK(configRocketSurfaceMode==0&&configRocketBoostMode==1);
    const char *invalid[]={"255","-1","broken","1oops","1 extra"};
    for(unsigned i=0;i<5;i++){char text[80];snprintf(text,sizeof text,"rocket_surface_mode %s\n",invalid[i]);content(text);configRocketSurfaceMode=0;load();CHECK(configRocketSurfaceMode==1);}
    content("show_fps true\n");configRocketSurfaceMode=0;load();CHECK(configRocketSurfaceMode==1);
    printf("boost and surface persistence: %d checks passed\n",checks);return 0;
}
