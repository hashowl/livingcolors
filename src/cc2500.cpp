#include <thread>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <wiringPi.h>
#include <wiringPiSPI.h>
#include "cc2500.h"
#include "cc2500_reg.h"
#include "livingcolors.h"

namespace cc2500
{

using namespace std::chrono_literals;

// state
bool ISR_registered;

// delays
auto cc2500_reset_delay = 10ms;

int SPI_fd;

bool setup()
{
    if (!setup_wiringPi())
    {
        lc::js_log("LivingColors exception: wiringPi setup failed");
        return false;
    }
    if (!setup_SPI())
    {
        lc::js_log("LivingColors exception: SPI setup failed");
        return false;
    }
    if (!setup_cc2500())
    {
        lc::js_log("LivingColors exception: cc2500 device setup failed");
        return false;
    }
    if (!setup_ISR())
    {
        lc::js_log("LivingColors exception: INT setup failed");
        return false;
    }
    lc::js_log("LivingColors info: cc2500 setup successful");
    return true;
}

bool setup_wiringPi()
{
    return !(wiringPiSetupGpio() < 0);
}

bool setup_SPI()
{
    return !((SPI_fd = wiringPiSPISetup(CC2500_SPI_CHANNEL, CC2500_SPI_FREQ_HZ)) < 0);
}

bool setup_cc2500()
{
    bool success = true;
    success &= reset_cc2500();
    success &= reset_cc2500();
    // device configuration
    success &= send_cmd_value(CC2500_REG_IOCFG2, 0x06);   // Output PKTCTRL interrupt on GDO2
    success &= send_cmd_value(CC2500_REG_IOCFG0, 0x5B);   // Output inverted PA_PD signal on GDO0
    success &= send_cmd_value(CC2500_REG_PKTLEN, 0x0E);   // Set maximum PACKET LENGTH of 14 bytes (excl. LENGTH field and CRC)
    success &= send_cmd_value(CC2500_REG_PKTCTRL1, 0x05); // Disable CRC autoflush, append 2 STATUS bytes and perform ADDR check
    success &= send_cmd_value(CC2500_REG_PKTCTRL0, 0x45); // Enable CRC and detection of packet LENGTH
    success &= send_cmd_value(CC2500_REG_ADDR, 0xAA);     // Device address is 0xAA
    // frequency configuration
    success &= send_cmd_value(CC2500_REG_CHANNR, 0x03);
    success &= send_cmd_value(CC2500_REG_FSCTRL1, 0x09);
    success &= send_cmd_value(CC2500_REG_FSCTRL0, 0x00);
    success &= send_cmd_value(CC2500_REG_FREQ2, 0x5D);
    success &= send_cmd_value(CC2500_REG_FREQ1, 0x93);
    success &= send_cmd_value(CC2500_REG_FREQ0, 0xB1);
    success &= send_cmd_value(CC2500_REG_MDMCFG4, 0x2D);
    success &= send_cmd_value(CC2500_REG_MDMCFG3, 0x3B);
    success &= send_cmd_value(CC2500_REG_MDMCFG2, 0x73);
    success &= send_cmd_value(CC2500_REG_MDMCFG1, 0x22);
    success &= send_cmd_value(CC2500_REG_MDMCFG0, 0xF8);
    success &= send_cmd_value(CC2500_REG_DEVIATN, 0x00);
    // device configuration
    success &= send_cmd_value(CC2500_REG_MCSM1, 0x3F);
    success &= send_cmd_value(CC2500_REG_MCSM0, 0x08);
    // frequency configuration
    success &= send_cmd_value(CC2500_REG_FOCCFG, 0x1D);
    success &= send_cmd_value(CC2500_REG_BSCFG, 0x1C);
    success &= send_cmd_value(CC2500_REG_AGCCTRL2, 0xC7);
    success &= send_cmd_value(CC2500_REG_AGCCTRL1, 0x00);
    success &= send_cmd_value(CC2500_REG_AGCCTRL0, 0xB2);
    success &= send_cmd_value(CC2500_REG_FREND1, 0xB6);
    success &= send_cmd_value(CC2500_REG_FREND0, 0x10);
    success &= send_cmd_value(CC2500_REG_FSCAL3, 0xEA);
    success &= send_cmd_value(CC2500_REG_FSCAL2, 0x0A);
    success &= send_cmd_value(CC2500_REG_FSCAL1, 0x00);
    success &= send_cmd_value(CC2500_REG_FSCAL0, 0x11);
    // power configuration
    success &= send_cmd_value(CC2500_REG_PATABLE, 0xA9);
    // go to IDLE mode (should be redundant)
    success &= send_strobe_cmd(CC2500_REG_SIDLE);
    // turn on the LNA PA
    setup_LNA_PA();
    return success;
}

bool reset_cc2500()
{
    bool res = send_strobe_cmd(CC2500_REG_SRES);
    std::this_thread::sleep_for(cc2500_reset_delay);
    return res;
}

void setup_LNA_PA()
{
    pinMode(CC2500_PIN_LEN, OUTPUT);
    digitalWrite(CC2500_PIN_LEN, true);
}

bool setup_ISR()
{
    if (ISR_registered)
        return true;
    return ISR_registered = !(wiringPiISR(CC2500_PIN_GDO2, INT_EDGE_RISING, &lc::cc2500_ISR) < 0);
}

void release()
{
    if (close(SPI_fd))
    {
        lc::js_log("LivingColors exception: failed to close SPI");
    }
}

unsigned char get_mode()
{
    unsigned char status = get_status_byte();
    if (status == CC2500_ERR)
    {
        lc::js_log("LivingColors exception: failed to get the status byte while trying to get the cc2500 mode");
        return CC2500_ERR;
    }
    return (status & CC2500_MSK_STATE) >> CC2500_SFT_STATE;
}

bool set_mode(unsigned char mode)
{
    unsigned char cmd;
    switch (mode)
    {
    case CC2500_MODE_IDLE:
        cmd = CC2500_REG_SIDLE;
        break;
    case CC2500_MODE_RX:
        cmd = CC2500_REG_SRX;
        break;
    case CC2500_MODE_TX:
        cmd = CC2500_REG_STX;
        break;
    case CC2500_MODE_FSTXON:
        cmd = CC2500_REG_SFSTXON;
        break;
    case CC2500_MODE_CALIBRATE:
        cmd = CC2500_REG_SCAL;
        break;
    default:
        lc::js_log("LivingColors exception: wrong argument in set mode");
        return false;
    }
    if (!send_strobe_cmd(cmd))
    {
        lc::js_log("LivingColors exception: strobe command to set mode failed");
        return false;
    }
    return true;
}

bool set_TXOFF_mode(unsigned char mode)
{
    unsigned char cmd = CC2500_REG_MCSM1 | CC2500_REG_READ | CC2500_REG_SINGLE;
    unsigned char MCSM1;
    if (!send_cmd(cmd, &MCSM1))
    {
        lc::js_log("LivingColors exception: getting MCSM1 failed");
        return false;
    }
    cmd = CC2500_REG_MCSM1 | CC2500_REG_WRITE | CC2500_REG_SINGLE;
    MCSM1 &= ~CC2500_MSK_TXOFF_MODE;
    switch (mode)
    {
    case CC2500_MODE_IDLE:
        MCSM1 |= 0x00;
        break;
    case CC2500_MODE_FSTXON:
        MCSM1 |= 0x01;
        break;
    case CC2500_MODE_TX:
        MCSM1 |= 0x02;
        break;
    case CC2500_MODE_RX:
        MCSM1 |= 0x03;
        break;
    default:
        lc::js_log("LivingColors exception: wrong argument in set TXOFF mode");
        return false;
    }
    if (!send_cmd(cmd, &MCSM1))
    {
        lc::js_log("LivingColors exception: setting TXOFF mode failed");
        return false;
    }
    return true;
}

unsigned char get_RX_bytes()
{
    unsigned char cmd = CC2500_REG_RXBYTES | CC2500_REG_READ_STATUS;
    unsigned char RX_bytes;
    if (!send_cmd(cmd, &RX_bytes))
    {
        lc::js_log("LivingColors exception: getting RX BYTES failed");
        return CC2500_ERR;
    }
    return RX_bytes;
}

unsigned char get_TX_bytes()
{
    unsigned char cmd = CC2500_REG_TXBYTES | CC2500_REG_READ_STATUS;
    unsigned char TX_bytes;
    if (!send_cmd(cmd, &TX_bytes))
    {
        lc::js_log("LivingColors exception: getting TX BYTES failed");
        return CC2500_ERR;
    }
    return TX_bytes;
}

bool empty_RXFIFO()
{
    unsigned char RX_bytes = get_RX_bytes();
    if (RX_bytes == CC2500_ERR)
    {
        return false;
    }
    unsigned char *data = (unsigned char *)malloc(CC2500_LENGTH_CMD + RX_bytes);
    data[CC2500_OFFSET_CMD] = CC2500_REG_RXFIFO | CC2500_REG_READ | CC2500_REG_BURST;
    // SPI transfer
    if (wiringPiSPIDataRW(CC2500_SPI_CHANNEL, data, CC2500_LENGTH_CMD + RX_bytes) == -1)
    {
        lc::js_log("LivingColors exception: SPI transfer failed");
        free(data);
        return false;
    }
    bool status = check_status_byte(data[0]);
    free(data);
    return status;
}

unsigned char *receive()
{
    // paket length
    unsigned char *data = (unsigned char *)malloc(CC2500_LENGTH_CMD + CC2500_LENGTH_HEADER);
    data[CC2500_OFFSET_CMD] = CC2500_REG_RXFIFO | CC2500_REG_READ | CC2500_REG_SINGLE;
    // SPI transfer
    if (wiringPiSPIDataRW(CC2500_SPI_CHANNEL, data, CC2500_LENGTH_CMD + CC2500_LENGTH_HEADER) == -1)
    {
        lc::js_log("LivingColors exception: SPI transfer failed");
        free(data);
        return NULL;
    }
    if (!check_status_byte(data[0]))
    {
        free(data);
        return NULL;
    }
    unsigned char length_payload = data[CC2500_OFFSET_DATA];
    free(data);
    // paket
    data = (unsigned char *)malloc(CC2500_LENGTH_CMD + length_payload + CC2500_LENGTH_TRAILER);
    data[CC2500_OFFSET_CMD] = CC2500_REG_RXFIFO | CC2500_REG_READ | CC2500_REG_BURST;
    // SPI transfer
    if (wiringPiSPIDataRW(CC2500_SPI_CHANNEL, data, CC2500_LENGTH_CMD + length_payload + CC2500_LENGTH_TRAILER) == -1)
    {
        lc::js_log("LivingColors exception: SPI transfer failed");
        free(data);
        return NULL;
    }
    if (!check_status_byte(data[0]))
    {
        free(data);
        return NULL;
    }
    unsigned char *pkt = (unsigned char *)malloc(CC2500_LENGTH_HEADER + length_payload + CC2500_LENGTH_TRAILER);
    pkt[CC2500_OFFSET_HEADER] = length_payload;
    memcpy(pkt + CC2500_OFFSET_PAYLOAD, data + CC2500_OFFSET_DATA, length_payload + CC2500_LENGTH_TRAILER);
    free(data);
    return pkt;
}

bool transmit(unsigned char *pkt)
{
    unsigned char TX_bytes = get_TX_bytes();
    if (TX_bytes != 0x00)
    {
        lc::js_log("LivingColors exception: TX FIFO was not empty before transmission");
        return false;
    }
    unsigned char length_payload = pkt[CC2500_OFFSET_HEADER];
    unsigned char *data = (unsigned char *)malloc(CC2500_LENGTH_CMD + CC2500_LENGTH_HEADER + length_payload);
    data[CC2500_OFFSET_CMD] = CC2500_REG_TXFIFO | CC2500_REG_WRITE | CC2500_REG_BURST;
    memcpy(data + CC2500_OFFSET_DATA, pkt, CC2500_LENGTH_HEADER + length_payload);
    // SPI transfer
    if (wiringPiSPIDataRW(CC2500_SPI_CHANNEL, data, CC2500_LENGTH_CMD + CC2500_LENGTH_HEADER + length_payload) == -1)
    {
        lc::js_log("LivingColors exception: SPI transfer failed");
        free(data);
        return false;
    }
    // start TX
    set_mode(CC2500_MODE_TX);
    bool status = check_status_byte(data[0]);
    free(data);
    return status;
}

bool send_strobe_cmd(unsigned char cmd)
{
    unsigned char *data = &cmd;
    // SPI transfer
    if (wiringPiSPIDataRW(CC2500_SPI_CHANNEL, data, CC2500_LENGTH_CMD) == -1)
    {
        lc::js_log("LivingColors exception: SPI transfer failed");
        return false;
    }
    return check_status_byte(data[0]);
}

bool send_cmd_value(unsigned char cmd, unsigned char val)
{
    return send_cmd(cmd, &val);
}

bool send_cmd(unsigned char cmd, unsigned char *val)
{
    unsigned char *data = (unsigned char *)malloc(CC2500_LENGTH_CMD + CC2500_LENGTH_SINGLE_ACCESS);
    data[CC2500_OFFSET_CMD] = cmd;
    data[CC2500_OFFSET_DATA] = *val;
    // SPI transfer
    if (wiringPiSPIDataRW(CC2500_SPI_CHANNEL, data, CC2500_LENGTH_CMD + CC2500_LENGTH_SINGLE_ACCESS) == -1)
    {
        lc::js_log("LivingColors exception: SPI transfer failed");
        free(data);
        return false;
    }
    *val = data[CC2500_OFFSET_DATA];
    bool status = check_status_byte(data[0]);
    free(data);
    return status;
}

unsigned char get_status_byte()
{
    unsigned char cmd = CC2500_REG_SNOP;
    unsigned char *data = &cmd;
    // SPI transfer
    if (wiringPiSPIDataRW(CC2500_SPI_CHANNEL, data, CC2500_LENGTH_CMD) == -1)
    {
        lc::js_log("LivingColors exception: SPI transfer failed");
        return CC2500_ERR;
    }
    if (!check_status_byte(data[0]))
    {
        return CC2500_ERR;
    }
    return data[0];
}

bool check_status_byte(unsigned char status)
{
    unsigned char CHIP_RDYn = status >> CC2500_SFT_CHIP_RDYn;
    unsigned char STATE = (status & CC2500_MSK_STATE) >> CC2500_SFT_STATE;
    if (CHIP_RDYn)
    {
        lc::js_log("LivingColors exception: CHIP_RDYn was high");
        return false;
    }
    if (STATE == CC2500_MODE_RXFIFO_OVERFLOW)
    {
        lc::js_log("LivingColors exception: RX FIFO overflow");
        return false;
    }
    if (STATE == CC2500_MODE_TXFIFO_UNDERFLOW)
    {
        lc::js_log("LivingColors exception: TX FIFO underflow");
        return false;
    }
    return true;
}

unsigned char get_pkt_length(unsigned char *pkt)
{
    return CC2500_LENGTH_HEADER + pkt[CC2500_OFFSET_HEADER] + CC2500_LENGTH_TRAILER;
}

} // namespace cc2500