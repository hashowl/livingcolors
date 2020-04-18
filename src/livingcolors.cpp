#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

#include "index.h"

namespace lc
{

// RX (com with remotes)
std::thread RX_thread;

// TX (com with lamps)
std::thread TX_thread;
std::queue<unsigned char *> TX_queue;
std::mutex TX_queue_mtx;
std::condition_variable TX_data_available_cv;

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
    TX_data_available_cv.notify_one();
    return 0;
}

StateChange createStateChange(unsigned char *pkt, uint32_t lamp)
{
    StateChange sc;
    sc.lamp = lamp;
    sc.command = pkt[PKT_IDX_COMMAND];
    sc.hue = pkt[PKT_IDX_HUE];
    sc.saturation = pkt[PKT_IDX_SATURATION];
    sc.value = pkt[PKT_IDX_VALUE];
    return sc;
}

unsigned char *createPacket(const StateChange &sc)
{
    unsigned char *pkt = (unsigned char *)malloc(sizeof(unsigned char) * (PACKET_LENGTH + 1));
    pkt[PKT_IDX_LENGTH] = PACKET_LENGTH;
    memcpy(pkt + PKT_IDX_DST_ADDR, lamps[sc.lamp], ADDR_LENGHT);
    memcpy(pkt + PKT_IDX_SRC_ADDR, bridge, ADDR_LENGHT);
    pkt[PKT_IDX_PTCL_INFO] = PTCL_INFO;
    pkt[PKT_IDX_COMMAND] = sc.command;
    pkt[PKT_IDX_SEQUENCE] = SEQUENCE_NR;
    pkt[PKT_IDX_HUE] = sc.hue;
    pkt[PKT_IDX_SATURATION] = sc.saturation;
    pkt[PKT_IDX_VALUE] = sc.value;
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