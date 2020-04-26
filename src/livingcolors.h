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
#define LC_OFFSET_RSSI 15
#define LC_OFFSET_CRC 16
#define LC_OFFSET_LQI 16

#define LC_MSK_CRC 0b10000000

#define LC_PTCL_INFO 17

#include <cstdint>

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

void stop();

void cc2500_ISR();

void RX_loop();
void TX_loop();
void FSCAL_loop();

bool await_mode(unsigned char);
bool try_mode(unsigned char);

void initiate_reset();
void reset();
void notify_threads();
void join_threads();

bool enqueue_StateChange(StateChange &);

StateChange create_StateChange(unsigned char *, uint32_t);
unsigned char *create_packet(StateChange &);
unsigned char *create_packet_ACK(unsigned char *);
bool test_ACK(unsigned char *);
bool test_CRC(unsigned char *);
void inc_sequence_nr(unsigned char *);

void js_log(const char *);
void js_change_state(StateChange &);

} // namespace lc

#endif