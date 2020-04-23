#ifndef LIVINGCOLORS_H
#define LIVINGCOLORS_H

#define LC_PACKET_LENGTH 14
#define LC_HEADER_LENGTH 1
#define LC_TRAILER_LENGTH 2
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

bool setup();
void start_threads();

void RX_loop();
void TX_loop();
void RX_processing_loop();

void cc2500_ISR();

bool awaitMode(unsigned char);
bool tryMode(unsigned char);

void initiate_reset();
void reset();
void notify_threads();
void join_threads();

bool enqueueStateChange(StateChange &);

StateChange createStateChange(unsigned char *, uint32_t);
unsigned char *createPacket(StateChange &);
unsigned char *createPacketACK(unsigned char *);
bool testACK(unsigned char *);
void inc_sequence_nr(unsigned char *);

void js_log(const char *);
void js_changeState(StateChange &);

} // namespace lc

#endif