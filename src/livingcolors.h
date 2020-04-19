#ifndef LIVINGCOLORS_H
#define LIVINGCOLORS_H

#define LC_PACKET_LENGTH 14
#define LC_ADDR_LENGHT 4

#define LC_OFFSET_LENGTH 0
#define LC_OFFSET_DST_ADDR 1
#define LC_OFFSET_SRC_ADDR 5
#define LC_OFFSET_PTCL_INFO 9
#define LC_OFFSET_COMMAND 10
#define LC_OFFSET_SEQUENCE 11
#define LC_OFFSET_HUE 12
#define LC_OFFSET_SATURATION 13
#define LC_OFFSET_VALUE 14

#define LC_PTCL_INFO 17

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

void RX_loop();
void TX_loop();

int enqueueStateChange(StateChange &);

StateChange createStateChange(unsigned char *, uint32_t);
unsigned char *createPacket(StateChange &);

void js_log(const char *);
void js_changeState(StateChange &);

} // namespace lc

#endif