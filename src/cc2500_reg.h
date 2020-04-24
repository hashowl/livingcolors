// CC2500 configuration registers
#define CC2500_REG_IOCFG2 0x00   // GDO2 output pin configuration
#define CC2500_REG_IOCFG1 0x01   // GDO1 output pin configuration
#define CC2500_REG_IOCFG0 0x02   // GDO0 output pin configuration
#define CC2500_REG_FIFOTHR 0x03  // RX FIFO and TX FIFO thresholds
#define CC2500_REG_SYNC1 0x04    // Sync word, high byte
#define CC2500_REG_SYNC0 0x05    // Sync word, low byte
#define CC2500_REG_PKTLEN 0x06   // Packet length
#define CC2500_REG_PKTCTRL1 0x07 // Packet automation control
#define CC2500_REG_PKTCTRL0 0x08 // Packet automation control
#define CC2500_REG_ADDR 0x09     // Device address
#define CC2500_REG_CHANNR 0x0A   // Channel number
#define CC2500_REG_FSCTRL1 0x0B  // Frequency synthesizer control
#define CC2500_REG_FSCTRL0 0x0C  // Frequency synthesizer control
#define CC2500_REG_FREQ2 0x0D    // Frequency control word, high byte
#define CC2500_REG_FREQ1 0x0E    // Frequency control word, middle byte
#define CC2500_REG_FREQ0 0x0F    // Frequency control word, low byte
#define CC2500_REG_MDMCFG4 0x10  // Modem configuration
#define CC2500_REG_MDMCFG3 0x11  // Modem configuration
#define CC2500_REG_MDMCFG2 0x12  // Modem configuration
#define CC2500_REG_MDMCFG1 0x13  // Modem configuration
#define CC2500_REG_MDMCFG0 0x14  // Modem configuration
#define CC2500_REG_DEVIATN 0x15  // Modem deviation setting
#define CC2500_REG_MCSM2 0x16    // Main Radio Control State Machine configuration
#define CC2500_REG_MCSM1 0x17    // Main Radio Control State Machine configuration
#define CC2500_REG_MCSM0 0x18    // Main Radio Control State Machine configuration
#define CC2500_REG_FOCCFG 0x19   // Frequency Offset Compensation configuration
#define CC2500_REG_BSCFG 0x1A    // Bit Synchronization configuration
#define CC2500_REG_AGCCTRL2 0x1B // AGC control
#define CC2500_REG_AGCCTRL1 0x1C // AGC control
#define CC2500_REG_AGCCTRL0 0x1D // AGC control
#define CC2500_REG_WOREVT1 0x1E  // High byte Event 0 timeout
#define CC2500_REG_WOREVT0 0x1F  // Low byte Event 0 timeout
#define CC2500_REG_WORCTRL 0x20  // Wake On Radio control
#define CC2500_REG_FREND1 0x21   // Front end RX configuration
#define CC2500_REG_FREND0 0x22   // Front end TX configuration
#define CC2500_REG_FSCAL3 0x23   // Frequency synthesizer calibration
#define CC2500_REG_FSCAL2 0x24   // Frequency synthesizer calibration
#define CC2500_REG_FSCAL1 0x25   // Frequency synthesizer calibration
#define CC2500_REG_FSCAL0 0x26   // Frequency synthesizer calibration
#define CC2500_REG_RCCTRL1 0x27  // RC oscillator configuration
#define CC2500_REG_RCCTRL0 0x28  // RC oscillator configuration
#define CC2500_REG_FSTEST 0x29   // Frequency synthesizer calibration control
#define CC2500_REG_PTEST 0x2A    // Production test
#define CC2500_REG_AGCTEST 0x2B  // AGC test
#define CC2500_REG_TEST2 0x2C    // Various test settings
#define CC2500_REG_TEST1 0x2D    // Various test settings
#define CC2500_REG_TEST0 0x2E    // Various test settings

// CC2500 command strobes
#define CC2500_REG_SRES 0x30
#define CC2500_REG_SFSTXON 0x31
#define CC2500_REG_SXOFF 0x32
#define CC2500_REG_SCAL 0x33
#define CC2500_REG_SRX 0x34
#define CC2500_REG_STX 0x35
#define CC2500_REG_SIDLE 0x36
#define CC2500_REG_SWOR 0x38
#define CC2500_REG_SPWD 0x39
#define CC2500_REG_SFRX 0x3A
#define CC2500_REG_SFTX 0x3B
#define CC2500_REG_SWORRST 0x3C
#define CC2500_REG_SNOP 0x3D

// CC2500 status registers
#define CC2500_REG_PARTNUM 0x30
#define CC2500_REG_VERSION 0x31
#define CC2500_REG_FREQEST 0x32
#define CC2500_REG_LQI 0x33
#define CC2500_REG_RSSI 0x34
#define CC2500_REG_MARCSTATE 0x35
#define CC2500_REG_WORTIME1 0x36
#define CC2500_REG_WORTIME0 0x37
#define CC2500_REG_PKTSTATUS 0x38
#define CC2500_REG_VCO_VC_DAC 0x39
#define CC2500_REG_TXBYTES 0x3A
#define CC2500_REG_RXBYTES 0x3B
#define CC2500_REG_RCCTRL1_STATUS 0x3C
#define CC2500_REG_RCCTRL0_STATUS 0x3D

// CC2500 multi byte registers
#define CC2500_REG_PATABLE 0x3E
#define CC2500_REG_TXFIFO 0x3F
#define CC2500_REG_RXFIFO 0x3F

// CC2500 offsets for register access
#define CC2500_REG_WRITE 0b00000000  // Offset for write access to TX FIFO and control registers
#define CC2500_REG_READ 0b10000000   // Offset for read access to RX FIFO and control registers
#define CC2500_REG_SINGLE 0b00000000 // Offset for single access to RX / TX FIFO and control registers
#define CC2500_REG_BURST 0b01000000  // Offset for burst access to RX / TX FIFO and control registers
#define CC2500_REG_READ_STATUS 0xC0  // Offset for single byte read access to STATUS registers