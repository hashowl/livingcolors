#ifndef LIVINGCOLORS_H
#define LIVINGCOLORS_H

#define PACKET_LENGTH 14
#define ADDR_LENGHT 4

#define PKT_IDX_LENGTH 0
#define PKT_IDX_DST_ADDR 1
#define PKT_IDX_SRC_ADDR 5
#define PKT_IDX_PTCL_INFO 9
#define PKT_IDX_COMMAND 10
#define PKT_IDX_SEQUENCE 11
#define PKT_IDX_HUE 12
#define PKT_IDX_SATURATION 13
#define PKT_IDX_VALUE 14

#define PTCL_INFO 17

namespace lc
{

class StateChange
{
public:
    uint32_t lamp;
    unsigned char command;
    unsigned char hue;
    unsigned char saturation;
    unsigned char value;
};

int setup();

int enqueueStateChange(StateChange &);

StateChange createStateChange(unsigned char *, uint32_t);
unsigned char *createPacket(const StateChange &);

void js_log(const char *);
void js_changeState(StateChange &);

} // namespace lc

#endif