/* Controls are shared across launcher modes; progression and other settings
 * remain in their existing save profiles. Included after the config registry. */
#include <errno.h>
#include <limits.h>
#ifdef _WIN32
#include <windows.h>
#endif
static const char *sharedControlsOverride;
static const char *shared_controls_path(void) {
    return sharedControlsOverride ? sharedControlsOverride : getenv("SUPER_ROCKET64_CONTROLS");
}
static bool shared_control_name(const char *name) {
    return !strncmp(name,"key_",4)||!strncmp(name,"stick_",6)||
        !strncmp(name,"bettercam_",10)||!strncmp(name,"romhackcam_",11)||
        !strcmp(name,"rocket_camera_mode")||!strcmp(name,"rocket-bindings:")||
        !strcmp(name,"rumble_strength")||!strcmp(name,"gamepad_number")||
        !strcmp(name,"background_gamepad")||!strcmp(name,"extended_reports")||
        !strcmp(name,"disable_gamepads")||!strcmp(name,"use_standard_key_bindings_chat");
}
static const struct ConfigOption *shared_control_option(const char *name) {
    if(!shared_control_name(name))return NULL;
    for(unsigned i=0;i<ARRAY_LEN(options);i++)if(!strcmp(name,options[i].name))return &options[i];
    return NULL;
}
static bool shared_number(const char *text,unsigned base,unsigned limit,unsigned *value) {
    char *end;unsigned long n;
    if(!*text||*text=='-'||*text=='+')return false;
    errno=0;n=strtoul(text,&end,base);
    if(errno||*end||n>limit)return false;
    *value=(unsigned)n;return true;
}
static bool shared_controls_tokens(char **tokens,unsigned count,bool apply) {
    if(!count||tokens[0][0]=='#')return true;
    if(!strcmp(tokens[0],"rocket-bindings:")) {
        unsigned values[RA_COUNT+3];
        if(count!=RA_COUNT+5||strcmp(tokens[1],"1"))return false;
        for(unsigned i=0;i<RA_COUNT+3;i++)
            if(!shared_number(tokens[i+2],10,i<RA_COUNT?RB_COUNT-1:1,&values[i]))return false;
        if(apply)rocket_bindings_read(tokens,count);
        return true;
    }
    const struct ConfigOption *option=shared_control_option(tokens[0]);
    if(!option)return true; /* Preserve unknown records when saving. */
    unsigned values[MAX_BINDS]={0};
    if(option->type==CONFIG_TYPE_BIND) {
        if(count<2||count>MAX_BINDS+1)return false;
        for(unsigned i=1;i<count;i++)if(!shared_number(tokens[i],16,0xffff,&values[i-1]))return false;
        if(apply)for(unsigned i=1;i<count;i++)option->uintValue[i-1]=values[i-1];
    } else if(option->type==CONFIG_TYPE_BOOL) {
        if(count!=2||(strcmp(tokens[1],"true")&&strcmp(tokens[1],"false")&&strcmp(tokens[1],"0")&&strcmp(tokens[1],"1")))return false;
        if(apply)*option->boolValue=!strcmp(tokens[1],"true")||!strcmp(tokens[1],"1");
    } else if(option->type==CONFIG_TYPE_UINT) {
        if(count!=2||!shared_number(tokens[1],10,!strcmp(tokens[0],"rocket_camera_mode")?1:UINT_MAX,values))return false;
        if(apply)*option->uintValue=values[0];
    } else return false;
    return true;
}
static bool shared_controls_parse(const char *data,bool apply) {
    while(*data) {
        size_t size=strcspn(data,"\n");char line[512],*tokens[20];
        if(size>=sizeof line)return false;
        memcpy(line,data,size);line[size]=0;
        if(!shared_controls_tokens(tokens,tokenize_string(line,ARRAY_LEN(tokens),tokens),apply))return false;
        data+=size;if(*data=='\n')data++;
    }
    return true;
}
static char *shared_controls_read(const char *path) {
    FILE *file=fopen(path,"rb");if(!file)return NULL;
    char *data=malloc(65538);if(!data){fclose(file);return NULL;}
    size_t size=fread(data,1,65537,file);bool failed=ferror(file)||size>65536;
    fclose(file);if(failed||memchr(data,0,size)){free(data);return NULL;}
    data[size]=0;return data;
}
static void shared_controls_emit(FILE *file) {
    for(unsigned i=0;i<ARRAY_LEN(options);i++)if(shared_control_name(options[i].name))configfile_save_option(file,&options[i],false);
    rocket_bindings_write(file);
}
static bool shared_controls_replace(const char *stage,const char *path) {
#ifdef _WIN32
    return MoveFileExA(stage,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    return rename(stage,path)==0;
#endif
}
static bool shared_controls_write_bytes(const char *path,const char *data) {
    char stage[SYS_MAX_PATH];
    if(snprintf(stage,sizeof stage,"%s.tmp",path)>=(int)sizeof stage)return false;
    FILE *file=fopen(stage,"wb");if(!file)return false;
    bool ok=fwrite(data,1,strlen(data),file)==strlen(data);
    if(fclose(file))ok=false;
    if(ok)ok=shared_controls_replace(stage,path);
    if(!ok)remove(stage);
    return ok;
}
static void shared_controls_load(void) {
    const char *path=shared_controls_path();sharedControlsReady=false;
    if(!path||!*path)return;
    char *data=shared_controls_read(path);
    if(!data||!shared_controls_parse(data,false)) {
        fprintf(stderr,"Could not read saved controls: %s. Existing controls were not overwritten.\n",path);
        free(data);return;
    }
    shared_controls_parse(data,true);free(data);sharedControlsReady=true;
}
static void shared_controls_save(void) {
    const char *path=shared_controls_path();if(!sharedControlsReady||!path||!*path)return;
    char stage[SYS_MAX_PATH],backup[SYS_MAX_PATH];
    if(snprintf(stage,sizeof stage,"%s.tmp",path)>=(int)sizeof stage||
       snprintf(backup,sizeof backup,"%s.backup",path)>=(int)sizeof backup)return;
    char *old=shared_controls_read(path);
    if(!old||!shared_controls_parse(old,false)){free(old);return;}
    FILE *file=fopen(stage,"wb");if(!file){free(old);return;}
    fputs("# Super Rocket 64 shared controls\n",file);shared_controls_emit(file);
    /* Keep future or hand-authored records; only replace known controls. */
    const char *p=old;
    while(*p) {
        size_t n=strcspn(p,"\n");char line[512],*tokens[20];memcpy(line,p,n);line[n]=0;
        unsigned count=tokenize_string(line,ARRAY_LEN(tokens),tokens);
        if(count&&tokens[0][0]!='#'&&!shared_control_option(tokens[0])&&strcmp(tokens[0],"rocket-bindings:")) {
            fwrite(p,1,n,file);fputc('\n',file);
        }
        p+=n;if(*p=='\n')p++;
    }
    bool ok=!ferror(file);if(fclose(file))ok=false;
    if(ok)ok=shared_controls_write_bytes(backup,old);
    if(ok)ok=shared_controls_replace(stage,path);
    if(!ok){remove(stage);fprintf(stderr,"Could not save controls to %s; previous controls retained.\n",path);}
    free(old);
}
/* Internal acceptance entrypoint: no graphics, controllers, ROM or networking. */
int configfile_controls_probe(int argc,char **argv) {
    if(argc==2&&!strcmp(argv[1],"--controls-defaults")){shared_controls_emit(stdout);return 0;}
    if(argc==3&&!strcmp(argv[1],"--validate-controls")) {
        char *data=shared_controls_read(argv[2]);
        bool valid=data&&shared_controls_parse(data,false);free(data);
        if(!valid)fprintf(stderr,"Saved controls are unreadable or invalid. Restore controls.cfg.backup or correct controls.cfg; files were kept.\n");
        return valid?0:2;
    }
    if(argc<2||strcmp(argv[1],"--verify-controls"))return -1;
    if(argc!=4&&argc!=5)return 2;
    sharedControlsOverride=argv[3];fs_init(argv[2]);configfile_load();
    if(!sharedControlsReady)return 2;
    if(argc==5) {
        char *edits=shared_controls_read(argv[4]);
        if(!edits||!shared_controls_parse(edits,false)){free(edits);return 2;}
        shared_controls_parse(edits,true);free(edits);configfile_save(configfile_name());
    }
    puts("CONTROLS_BEGIN");shared_controls_emit(stdout);puts("CONTROLS_END");
    for(unsigned i=0;i<15;i++) {
        RocketPadSample raw={0};RocketGamepad pad={0};raw.buttons=1u<<i;
        rocket_bindings_apply(&configRocketBindings,&raw,&pad);
        printf("CONTROL_BUTTON %u %d %d %d\n",i,pad.jump,pad.boost,pad.powerslide);
    }
    RocketPadSample axes={0};RocketGamepad mapped={0};
    axes.left_x=1000;axes.left_y=2000;axes.right_x=12000;axes.right_y=-15000;
    rocket_bindings_apply(&configRocketBindings,&axes,&mapped);
    printf("CONTROL_AXES %d %d\n",mapped.left_x,mapped.left_y);
    return 0;
}
