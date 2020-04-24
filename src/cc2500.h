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

#define CC2500_PIN_GDO2 5
#define CC2500_PIN_LEN 6

#define CC2500_SPI_FREQ_HZ 6000000
#define CC2500_SPI_CHANNEL 0

namespace cc2500
{

bool setup();
bool setup_SPI();

void release();

unsigned char get_mode();
void set_mode(unsigned char);

void set_TXOFF_mode(unsigned char);

unsigned char get_RX_bytes();

void empty_RXFIFO();

unsigned char *receive();
void transmit(unsigned char *);

} // namespace cc2500

#endif