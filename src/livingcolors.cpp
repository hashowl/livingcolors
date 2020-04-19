#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include "index.h"
#include "cc2500.h"

namespace lc
{

// cc2500
std::mutex cc2500_mtx;

// RX thread (communication with remotes)
std::thread RX_thread(RX_loop);
// RX queue
std::queue<unsigned char *> RX_queue;
std::mutex RX_queue_mtx;
std::condition_variable RX_queue_cv;

// TX thread (communication with lamps)
std::thread TX_thread(TX_loop);
// TX queue
std::queue<unsigned char *> TX_queue;
std::mutex TX_queue_mtx;
std::condition_variable TX_queue_cv;

// await RX completion
std::condition_variable RX_completed_cv;
bool await_RX_completion;

// addresses
const unsigned char bridge[] = {0xAA, 0xAA, 0xAA, 0x01};
const int num_remotes = 1;
const unsigned char remote_0[] = {0x7A, 0x8A, 0xF8, 0x09};
const unsigned char *remotes[] = {
    remote_0};
const int num_lamps = 3;
const unsigned char lamp_0[] = {0xF0, 0xC6, 0x18, 0x0F};
const unsigned char lamp_1[] = {0x98, 0x74, 0xDB, 0x00};
const unsigned char lamp_2[] = {0xB8, 0xF8, 0x9C, 0x09};
const unsigned char *lamps[] = {
    lamp_0,
    lamp_1,
    lamp_2};

int SEQUENCE_NR = 0;

int setup()
{
    return 0;
}

void RX_loop()
{
}

void TX_loop()
{
    {
        std::unique_lock<std::mutex> lck(TX_queue_mtx);
        TX_queue_cv.wait(lck, [] { return !TX_queue.empty(); });
        unsigned char *pkt = TX_queue.front();
        TX_queue.pop();
    }
    // a packet has been popped from the TX FIFO and needs to be sent
    {
        std::unique_lock<std::mutex> lck(cc2500_mtx);
        // we have control over the chip
        // but the chip is most likely still in RX and might even be currently receiving a packet
        while (cc2500::getMode() != CC2500_MODE_FSTXON)
        {
            // try going to FSTXON

            // the chip might refuse to change the mode while receivng a packet
            // we continue if the mode is now FSTXON, if not, we try switching again
        }
        if (cc2500::getRXbytes() != 0)
        {
        }
    }
}

int enqueueStateChange(StateChange &sc)
{
    unsigned char *pkt = createPacket(sc);
    {
        std::lock_guard<std::mutex> lck(TX_queue_mtx);
        if (TX_queue.size() < 10)
        {
            TX_queue.push(pkt);
        }
        else
        {
            return -1;
        }
    }
    TX_queue_cv.notify_one();
    return 0;
}

StateChange createStateChange(unsigned char *pkt, uint32_t lamp)
{
    StateChange sc;
    sc.lamp = lamp;
    sc.command = pkt[LC_OFFSET_COMMAND];
    sc.hue = pkt[LC_OFFSET_HUE];
    sc.saturation = pkt[LC_OFFSET_SATURATION];
    sc.value = pkt[LC_OFFSET_VALUE];
    return sc;
}

unsigned char *createPacket(StateChange &sc)
{
    unsigned char *pkt = (unsigned char *)malloc(sizeof(unsigned char) * (LC_PACKET_LENGTH + 1));
    pkt[LC_OFFSET_LENGTH] = LC_PACKET_LENGTH;
    memcpy(pkt + LC_OFFSET_DST_ADDR, lamps[sc.lamp], LC_ADDR_LENGHT);
    memcpy(pkt + LC_OFFSET_SRC_ADDR, bridge, LC_ADDR_LENGHT);
    pkt[LC_OFFSET_PTCL_INFO] = LC_PTCL_INFO;
    pkt[LC_OFFSET_COMMAND] = sc.command;
    pkt[LC_OFFSET_SEQUENCE] = SEQUENCE_NR;
    pkt[LC_OFFSET_HUE] = sc.hue;
    pkt[LC_OFFSET_SATURATION] = sc.saturation;
    pkt[LC_OFFSET_VALUE] = sc.value;
    SEQUENCE_NR = (SEQUENCE_NR + 1) % 0x100;
    return pkt;
}

void js_log(const char *msg)
{
    tsf_log.BlockingCall(new std::string(msg), js_cb_log);
}

void js_changeState(StateChange &sc)
{
    tsf_changeState.BlockingCall(new StateChange(sc), js_cb_changeState);
}

} // namespace lc