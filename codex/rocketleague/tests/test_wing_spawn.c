/* Compile the real pre-relay validator; keep unrelated legacy spawn execution
 * inert in the packet fixture. */
#define network_receive_spawn_objects fixture_unused_receive_spawn_objects
#include "../../../src/pc/network/packets/packet_spawn_objects.c"
