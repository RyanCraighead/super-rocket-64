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

#include "../../../src/pc/controller/controller_entry_point.c"
static int reconfigured;
static void reconfigure_test(void){reconfigured++;}
struct ControllerAPI controller_sdl={.reconfig=reconfigure_test};
struct ControllerAPI controller_keyboard={.reconfig=reconfigure_test};
int main(int argc,char **argv){
    CHECK(argc==2);directory=argv[1];
    char shared[2048];snprintf(shared,sizeof shared,"%s/shared.cfg",directory);
    FILE *f=fopen(shared,"w");CHECK(f);fputs("key_a 002c 1003 1103\nrocket-bindings: 1 14 13 4 6 5 5 0 0 1 1 1\n",f);fclose(f);
    setenv("SUPER_ROCKET64_CONTROLS",shared,1);configfile_load();
    CHECK(configKeyA[0]==0x2c&&configRocketBindings.action[RA_BOOST]==RB_RB);
    CHECK(configRocketBindings.action[RA_JUMP]==RB_NORTH&&configRocketBindings.action[RA_CAMERA]==RB_NONE);
    configKeyA[0]=0x31;configKeyB[0]=0x32;configKeyCLeft[0]=0x33;
    controller_reconfigure();CHECK(reconfigured==2);
    configKeyA[0]=0;configKeyB[0]=0;configKeyCLeft[0]=0;
    configfile_load();CHECK(configKeyA[0]==0x31&&configKeyB[0]==0x32&&configKeyCLeft[0]==0x33);
    CHECK(configRocketBindings.action[RA_BOOST]==RB_RB);
    for(unsigned binding=RB_NONE;binding<RB_COUNT;binding++){
        configRocketBindings.action[RA_CAMERA]=binding;configRocketCameraMode=binding%2;configKeyY[0]=0x21;
        configfile_save(configfile_name());
        configRocketBindings.action[RA_CAMERA]=RB_NONE;configRocketCameraMode=2;configKeyY[0]=0;
        configfile_load();
        CHECK(configRocketBindings.action[RA_CAMERA]==binding&&configRocketCameraMode==binding%2&&configKeyY[0]==0x21);
        CHECK(configRocketBindings.action[RA_JUMP]==RB_NORTH&&configRocketBindings.action[RA_BOOST]==RB_RB);
    }
    printf("immediate native controls save/reload: %d checks passed\n",checks);return 0;
}
