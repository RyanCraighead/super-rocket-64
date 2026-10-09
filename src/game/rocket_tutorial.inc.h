/* Called before the unchanged native dialog state machine. Lua overrides and
 * custom dialog table replacements stay authoritative. Reflow only between
 * page transitions, retaining a page number rather than a stale byte offset. */
#include "rocket_tutorial.h"
static struct DialogEntry *rocket_tutorial_dialog(struct DialogEntry *original) {
    static struct DialogEntry view;
    static uint8_t retained[ROCKET_TUTORIAL_CAPACITY];
    static int retainedId=-1;
    uint8_t next[ROCKET_TUTORIAL_CAPACITY];
    if(!original||original->replaced||gLastDialogResponse||sOverrideDialogHookString||
       !rocket_tutorial_text(gDialogID,original->linesPerBox,next)) {
        retainedId=-1;return original;
    }
    if(retainedId!=gDialogID||gDialogBoxState==DIALOG_STATE_OPENING||gDialogBoxState==DIALOG_STATE_VERTICAL) {
        int length=0;while(next[length]!=DIALOG_CHAR_TERMINATOR)length++;
        if(retainedId!=gDialogID||memcmp(retained,next,length+1)) {
            int page=retainedId==gDialogID&&gDialogBoxState!=DIALOG_STATE_OPENING?
                rocket_tutorial_page(retained,gDialogTextPos,original->linesPerBox):0;
            memcpy(retained,next,length+1);retainedId=gDialogID;
            gDialogTextPos=rocket_tutorial_page_start(retained,page,original->linesPerBox);
            int nextPage=rocket_tutorial_page_start(retained,page+1,original->linesPerBox);
            gLastDialogPageStrPos=nextPage>gDialogTextPos?nextPage:-1;
        }
    }
    view=*original;view.str=retained;return &view;
}
