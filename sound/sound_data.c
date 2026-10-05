/* Audio buffers are populated from verified user-local data, then ROM-backed ranges. */
#include "sound_data.h"
#include "pc/local_engine_assets.h"

unsigned char gSoundDataADSR[193920] = {0};
LOCAL_ENGINE_ASSET_REGISTER(gSoundDataADSR, "sound/sound_data.ctl", "e0d97745faf18d59015fcf22bfe25d092e82d7a2d620c9eaa5753141d180c07d");

unsigned char gSoundDataRaw[5997056] = {0};
LOCAL_ENGINE_ASSET_REGISTER(gSoundDataRaw, "sound/sound_data.tbl", "9f88ad119721375883ba6ca21ebfc5561551372a5d1ed09826d48b4b20a29000");

unsigned char gMusicData[117248] = {0};
LOCAL_ENGINE_ASSET_REGISTER(gMusicData, "sound/sequences.bin", "8893b2e8a17282b0f1fc1cef24200c71643e2a4983e5b4aba8f166a2961c0bd7");

unsigned char gBankSetsData[160] = {0};
LOCAL_ENGINE_ASSET_REGISTER(gBankSetsData, "sound/bank_sets", "db58c44e1e5a46f1bdfc11f3572c2ff35e52d3b301dc560d3d60afc43deaa49d");
