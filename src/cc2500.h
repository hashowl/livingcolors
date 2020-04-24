#ifndef CC2500_H
#define CC2500_H

#define CC2500_MODE_IDLE 0
#define CC2500_MODE_RX 1
#define CC2500_MODE_TX 2
#define CC2500_MODE_FSTXON 3
#define CC2500_MODE_CALIBRATE 4
#define CC2500_MODE_SETTLING 5
#define CC2500_MODE_RXFIFO_OVERFLOW 6
#define CC2500_MODE_TXFIFO_UNDERFLOW 7

#define CC2500_SFT_CHIP_RDYn 7
#define CC2500_MSK_STATE 0b01110000
#define CC2500_SFT_STATE 4
#define CC2500_MSK_TXOFF_MODE 0b00000011

#define CC2500_CMD_LENGTH 1
#define CC2500_VAL_LENGTH 1

#define CC2500_CMD_OFFSET 0
#define CC2500_PKT_OFFSET 1
#define CC2500_VAL_OFFSET 1

#define CC2500_ERR 0xFF

#define CC2500_PIN_GDO2 5
#define CC2500_PIN_LEN 6

#define CC2500_SPI_FREQ_HZ 6000000
#define CC2500_SPI_CHANNEL 0

namespace cc2500
{

bool setup();
bool setup_wiringPi();
bool setup_SPI();
bool setup_cc2500();
bool reset_cc2500();
void setup_LNA_PA();
bool setup_INT();

void release();

unsigned char get_mode();
bool set_mode(unsigned char);

bool set_TXOFF_mode(unsigned char);

unsigned char get_RX_bytes();
unsigned char get_TX_bytes();

bool empty_RXFIFO();

unsigned char *receive();
bool transmit(unsigned char *);

bool send_strobe_cmd(unsigned char);
bool send_cmd_value(unsigned char, unsigned char);
bool send_cmd(unsigned char, unsigned char *);

unsigned char get_status_byte();
bool check_status_byte(unsigned char);

} // namespace cc2500

#endif