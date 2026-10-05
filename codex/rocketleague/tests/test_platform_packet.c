/* Exercise the existing object payload serializer without sockets/windows. */
#include "../../../src/pc/network/packets/packet_object.c"
void platform_test_write(struct Packet *packet,struct Object *object) {
    packet_write_object_standard_fields(packet,object);
    packet_write_object_extra_fields(packet,object);
}
void platform_test_read(struct Packet *packet,struct Object *object) {
    packet_read_object_standard_fields(packet,object);
    packet_read_object_extra_fields(packet,object);
}
