#include <wiringPi.h>
#include <wiringPiSPI.h>
#include "cc2500.h"
#include "cc2500_reg.h"
#include "livingcolors.h"

namespace cc2500
{

bool setup()
{
    if (!setup_SPI())
    {
        lc::js_log("LivingColors exception: SPI setup failed");
        return false;
    }
}

bool setup_SPI()
{
}

void release()
{
}

unsigned char get_mode()
{
}

void set_mode(unsigned char mode)
{
}

void set_TXOFF_mode(unsigned char mode)
{
}

unsigned char get_RX_bytes()
{
}

void empty_RXFIFO()
{
}

unsigned char *receive()
{
}

void transmit(unsigned char *pkt)
{
}

} // namespace cc2500