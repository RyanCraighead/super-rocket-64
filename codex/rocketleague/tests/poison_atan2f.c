// Simulate a host exporting a nonstandard float atan2, as SM64 does.
// Physics must never call this symbol, even when it is present at link time.
int rocket_poison_atan2f_calls;
#ifdef _MSC_VER
float atan2f(float y,float x);
#pragma function(atan2f)
#endif
float atan2f(float y,float x) {
    (void)y;(void)x;++rocket_poison_atan2f_calls;return 1234.f;
}
