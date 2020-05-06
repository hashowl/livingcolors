#ifndef LIVINGCOLORS_H
#define LIVINGCOLORS_H

#define LC_LENGTH_PAYLOAD 14
#define LC_LENGHT_ADDR 4

// packet
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

#define LC_PTCL_INFO 17

#define LC_COMMAND_HSV 0x03
#define LC_COMMAND_ON 0x05
#define LC_COMMAND_OFF 0x07

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

void ISR_loop();
void TX_loop();
void FSCAL_loop();

bool calibrate();

bool await_mode(unsigned char);
bool try_mode(unsigned char);

void initiate_reset();
void reset();
void notify_threads();
void join_threads();

bool enqueue_StateChange(StateChange &);

StateChange create_StateChange(unsigned char *);
unsigned char *create_TX_CMD_pkt(StateChange &);
unsigned char *create_TX_CMD_pkt(unsigned char *, uint32_t);
unsigned char *create_TX_ACK_pkt(unsigned char *);
bool test_RX_ACK(unsigned char *);
bool test_TX_CMD(unsigned char *);
bool test_CRC(unsigned char *);
bool test_lc(unsigned char *);
void inc_sequence_nr(unsigned char *);

int get_lamp(unsigned char *);

void js_log(const char *);
void js_ack(StateChange &);

} // namespace lc

#endif