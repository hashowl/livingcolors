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

namespace cc2500
{

unsigned char getMode();
unsigned char getRXbytes();

} // namespace cc2500

#endif