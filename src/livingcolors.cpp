#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>
#include "index.h"
#include "cc2500.h"

namespace lc
{

using namespace std::chrono_literals;

// flags
std::atomic<bool> reset_flag(true);

// cc2500
std::mutex cc2500_mtx;
// cc2500 ready
std::condition_variable cc2500_ready_cv;
bool cc2500_ready;

// RX thread (communication with remotes)
std::thread RX_thread;
// RX pending
std::condition_variable RX_pending_cv;
bool RX_pending;
// await RX
std::condition_variable await_RX_cv;
bool await_RX;
// await ACK
std::condition_variable await_ACK_cv;
bool await_ACK;

// TX thread (communication with lamps)
std::thread TX_thread;
// TX queue
std::queue<unsigned char *> TX_queue;
std::mutex TX_queue_mtx;
std::condition_variable TX_queue_cv;
// await TX
std::condition_variable await_TX_cv;
std::atomic<bool> await_TX;

// RX processing thread
std::thread RX_processing_thread;
// RX queue
std::queue<unsigned char *> RX_queue;
std::mutex RX_queue_mtx;
std::condition_variable RX_queue_cv;

// frequency synthesizer calibration thread
std::thread FSCAL_thread;

// ISR
std::atomic<bool> disable_INT(false);

// timeouts
auto cc2500_ready_timeout = 1500ms;
auto cc2500_RX_timeout = 600ms;
auto cc2500_TX_timeout = 500ms;
auto cc2500_ACK_timeout = 1600ms;
auto cc2500_try_mode_timeout = 300ms;
auto cc2500_await_mode_timeout = 300ms;

// delays
auto cc2500_setup_retry_delay = 1000ms;

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

// sequence number
std::mutex SEQUENCE_NR_mtx;
unsigned char SEQUENCE_NR = 0;

// last packet
unsigned char *last_packet;

bool setup()
{
    int i = 0;
    while (!cc2500::setup())
    {
        i++;
        if (i > 2)
        {
            js_log("LivingColors exception: cc2500 setup failed");
            return false;
        }
        std::this_thread::sleep_for(cc2500_setup_retry_delay);
    }
    // the cc2500 is now in IDLE mode
    {
        std::lock_guard<std::mutex> lck_cc2500(cc2500_mtx);
        start_threads();
        cc2500_ready = true;
        reset_flag = false;
        cc2500::set_mode(CC2500_MODE_RX);
    }
    return true;
}

void start_threads()
{
    RX_thread = std::thread(RX_loop);
    TX_thread = std::thread(TX_loop);
    RX_processing_thread = std::thread(RX_processing_loop);
}

void RX_loop()
{
    while (true)
    {
        // always lock on the cc2500
        std::unique_lock<std::mutex> lck_cc2500(cc2500_mtx);
        // wait until we receive an INT associated with RX
        RX_pending_cv.wait(lck_cc2500, [] { return RX_pending || reset_flag.load(); });
        if (reset_flag.load())
            return;
        // we might have received a packet
        RX_pending = false;
        // wait until the cc2500 is ready
        if (!cc2500_ready_cv.wait_for(lck_cc2500, cc2500_ready_timeout, [] { return cc2500_ready; }))
        {
            js_log("LivingColors exception: timeout expired while waiting for cc2500 ready signal");
            initiate_reset();
            return;
        }
        // the mode is RX, FSTXON or a transitional state
        unsigned char mode = cc2500::get_mode();
        if (mode == CC2500_MODE_RX)
        {
            // the mode is still RX, the packet was discarded
            // we don't need to do anything
            continue;
        }
        if (mode != CC2500_MODE_FSTXON)
        {
            // the mode is about to change to FSTXON or to RX
            if (!try_mode(CC2500_MODE_FSTXON))
            {
                initiate_reset();
                return;
            }
        }
        // the mode is FSTXON
        // we need to check the RX FIFO for a packet
        unsigned char RX_bytes = cc2500::get_RX_bytes();
        if (RX_bytes == 0)
        {
            // the RX FIFO was flushed and we can restart RX
            cc2500::set_mode(CC2500_MODE_RX);
            continue;
        }
        else if (RX_bytes == LC_HEADER_LENGTH + LC_PACKET_LENGTH + LC_TRAILER_LENGTH)
        {
            // the RX FIFO contains a packet
            // we read the packet
            unsigned char *pkt = cc2500::receive();
            if (memcmp(pkt + LC_OFFSET_DST_ADDR, bridge, LC_ADDR_LENGHT) == 0)
            {
                if (memcmp(pkt + LC_OFFSET_SRC_ADDR, remotes[0], LC_ADDR_LENGHT) == 0)
                {
                    // we received a packet from the remote
                    if (await_RX)
                    {
                        cc2500::set_TXOFF_mode(CC2500_MODE_FSTXON);
                    }
                    unsigned char *pkt_ACK = create_packet_ACK(pkt);
                    cc2500_ready = false;
                    await_TX = true;
                    cc2500::transmit(pkt_ACK);
                    // the mode is now changing to RX or FSTXON
                    {
                        std::lock_guard<std::mutex> lck_RX_queue(RX_queue_mtx);
                        RX_queue.push(pkt);
                        if (RX_queue.size() > 10)
                        {
                            js_log("LivingColors warning: RX queue size > 10");
                        }
                    }
                    RX_queue_cv.notify_one();
                    free(pkt_ACK);
                    if (!await_TX_cv.wait_for(lck_cc2500, cc2500_TX_timeout, [] { return !await_TX; }))
                    {
                        js_log("LivingColors exception: timeout expired while waiting for TX");
                        initiate_reset();
                        return;
                    }
                    if (await_RX)
                    {
                        if (!await_mode(CC2500_MODE_FSTXON))
                        {
                            initiate_reset();
                            return;
                        }
                        cc2500::set_TXOFF_mode(CC2500_MODE_RX);
                        await_RX = false;
                        lck_cc2500.unlock();
                        await_RX_cv.notify_one();
                        continue;
                    }
                    else
                    {
                        cc2500_ready = true;
                        lck_cc2500.unlock();
                        cc2500_ready_cv.notify_all();
                        continue;
                    }
                }
                else
                {
                    bool valid = false;
                    for (int i = 0; i < num_lamps; i++)
                    {
                        if (memcmp(pkt + LC_OFFSET_SRC_ADDR, lamps[i], LC_ADDR_LENGHT) == 0)
                        {
                            valid = true;
                            break;
                        }
                    }
                    if (valid)
                    {
                        // we received a packet from a lamp
                        if (await_ACK)
                        {
                            if (test_ACK(pkt))
                            {
                                await_ACK = false;
                                cc2500::set_mode(CC2500_MODE_RX);
                                lck_cc2500.unlock();
                                await_ACK_cv.notify_one();
                            }
                            else
                            {
                                js_log("LivingColors warning: received wrong ACK");
                                cc2500::set_mode(CC2500_MODE_RX);
                            }
                            free(pkt);
                            continue;
                        }
                        else
                        {
                            js_log("LivingColors warning: received unexpected ACK");
                        }
                    }
                    else
                    {
                        js_log("LivingColors warning: received packet contained wrong source address");
                    }
                }
            }
            else
            {
                js_log("LivingColors warning: received packet contained wrong destination address");
            }
            free(pkt);
        }
        else
        {
            // the RX FIFO contains invalid data
            js_log("LivingColors warning: RX FIFO contains invalid data");
            cc2500::empty_RXFIFO();
        }
        if (await_RX)
        {
            await_RX = false;
            cc2500_ready = false;
            lck_cc2500.unlock();
            await_RX_cv.notify_one();
            continue;
        }
        cc2500::set_mode(CC2500_MODE_RX);
    }
}

void TX_loop()
{
    unsigned char *pkt;
    {
        std::unique_lock<std::mutex> lck_TX_queue(TX_queue_mtx);
        TX_queue_cv.wait(lck_TX_queue, [] { return !TX_queue.empty() || reset_flag.load(); });
        if (reset_flag.load())
            return;
        pkt = TX_queue.front();
        TX_queue.pop();
    }
    // a packet has been popped from the TX queue and needs to be sent
    {
        // always lock on the cc2500
        std::unique_lock<std::mutex> lck_cc2500(cc2500_mtx);
        // wait until the cc2500 is ready
        if (!cc2500_ready_cv.wait_for(lck_cc2500, cc2500_ready_timeout, [] { return cc2500_ready; }))
        {
            js_log("LivingColors exception: timeout expired while waiting for cc2500 ready signal");
            initiate_reset();
            return;
        }
        // the mode is RX, FSTXON or a transitional state
        if (!try_mode(CC2500_MODE_FSTXON))
        {
            initiate_reset();
            return;
        }
        // the mode is FSTXON
        if (cc2500::get_RX_bytes() != 0)
        {
            // the RX FIFO is not empty and might contain a valid packet
            await_RX = true;
            if (!await_RX_cv.wait_for(lck_cc2500, cc2500_RX_timeout, [] { return !await_RX; }))
            {
                js_log("LivingColors exception: timeout expired while waiting for RX");
                initiate_reset();
                return;
            }
            cc2500_ready = true;
            cc2500_ready_cv.notify_all();
        }
        int i = 0;
        bool ACK = false;
        last_packet = pkt;
        do
        {
            if (i > 0)
            {
                inc_sequence_nr(pkt);
            }
            await_TX = true;
            cc2500::transmit(pkt);
            await_ACK = true;
            ACK = await_ACK_cv.wait_for(lck_cc2500, cc2500_ACK_timeout, [] { return !await_ACK; });
        } while (!ACK && i < 4);
        if (!ACK)
        {
            await_ACK = false;
            js_log("LivingColors warning: no ACK received");
        }
    }
}

void RX_processing_loop()
{
}

void cc2500_ISR()
{
    if (reset_flag.load())
        return;
    if (disable_INT.load())
    {
        js_log("LivingColors warning: unexpected INT received");
        return;
    }
    bool await_TX_capture = await_TX.load();
    std::unique_lock<std::mutex> lck_cc2500(cc2500_mtx);
    if (await_TX_capture)
    {
        await_TX = false;
        lck_cc2500.unlock();
        await_TX_cv.notify_one();
    }
    else if (!await_TX.load())
    {
        RX_pending = true;
        lck_cc2500.unlock();
        RX_pending_cv.notify_one();
    }
    else
    {
        js_log("LivingColors warning: delayed INT received");
        return;
    }
}

bool await_mode(unsigned char mode)
{
    auto t0 = std::chrono::high_resolution_clock::now();
    while (cc2500::get_mode() != mode)
    {
        auto dt = std::chrono::high_resolution_clock::now() - t0;
        if (dt > cc2500_await_mode_timeout)
        {
            std::string msg = "LivingColors exception: timeout expired while waiting for the mode to change to " + std::to_string(mode);
            js_log(msg.c_str());
            return false;
        }
    }
    return true;
}

bool try_mode(unsigned char mode)
{
    auto t0 = std::chrono::high_resolution_clock::now();
    while (cc2500::get_mode() != mode)
    {
        auto dt = std::chrono::high_resolution_clock::now() - t0;
        if (dt > cc2500_try_mode_timeout)
        {
            std::string msg = "LivingColors exception: timeout expired while trying to change the mode to " + std::to_string(mode);
            js_log(msg.c_str());
            return false;
        }
        cc2500::set_mode(mode);
    }
    return true;
}

void initiate_reset()
{
    std::thread reset_thread(reset);
    reset_thread.detach();
}

void reset()
{
    notify_threads();
    join_threads();
    cc2500::release();
    {
        std::lock_guard<std::mutex> lck_cc2500(cc2500_mtx);
        cc2500_ready = false;
        RX_pending = false;
        await_RX = false;
        await_ACK = false;
        await_TX = false;
    }
    {
        std::lock_guard<std::mutex> lck_RX_queue(RX_queue_mtx);
        RX_queue = std::queue<unsigned char *>();
    }
    {
        std::lock_guard<std::mutex> lck_TX_queue(TX_queue_mtx);
        TX_queue = std::queue<unsigned char *>();
    }
    if (!setup())
    {
        js_log("LivingColors exception: reset failed");
        return;
    }
    js_log("LivingColors info: reset successful");
}

void notify_threads()
{
    {
        std::lock_guard<std::mutex> lck_cc2500(cc2500_mtx);
        std::lock_guard<std::mutex> lck_RX_queue(RX_queue_mtx);
        std::lock_guard<std::mutex> lck_TX_queue(TX_queue_mtx);
        reset_flag = true;
    }
    RX_pending_cv.notify_one();
    RX_queue_cv.notify_one();
    TX_queue_cv.notify_one();
}

void join_threads()
{
    RX_thread.join();
    TX_thread.join();
    RX_processing_thread.join();
}

bool enqueue_StateChange(StateChange &sc)
{
    if (reset_flag.load())
    {
        js_log("LivingColors exception: reset flag is set");
        return false;
    }
    unsigned char *pkt = create_packet(sc);
    {
        std::lock_guard<std::mutex> lck_TX_queue(TX_queue_mtx);
        if (TX_queue.size() < 10)
        {
            TX_queue.push(pkt);
        }
        else
        {
            js_log("LivingColors exception: maximum TX queue size reached");
            return false;
        }
    }
    TX_queue_cv.notify_one();
    return true;
}

StateChange create_StateChange(unsigned char *pkt, uint32_t lamp)
{
    StateChange sc;
    sc.lamp = lamp;
    sc.command = pkt[LC_OFFSET_COMMAND];
    sc.hue = pkt[LC_OFFSET_HUE];
    sc.saturation = pkt[LC_OFFSET_SATURATION];
    sc.value = pkt[LC_OFFSET_VALUE];
    return sc;
}

unsigned char *create_packet(StateChange &sc)
{
    unsigned char *pkt = (unsigned char *)malloc(LC_HEADER_LENGTH + LC_PACKET_LENGTH);
    pkt[LC_OFFSET_LENGTH] = LC_PACKET_LENGTH;
    memcpy(pkt + LC_OFFSET_DST_ADDR, lamps[sc.lamp], LC_ADDR_LENGHT);
    memcpy(pkt + LC_OFFSET_SRC_ADDR, bridge, LC_ADDR_LENGHT);
    pkt[LC_OFFSET_PTCL_INFO] = LC_PTCL_INFO;
    pkt[LC_OFFSET_COMMAND] = sc.command;
    {
        std::lock_guard<std::mutex> lck_SEQUENCE_NR(SEQUENCE_NR_mtx);
        pkt[LC_OFFSET_SEQUENCE] = SEQUENCE_NR;
        SEQUENCE_NR = (SEQUENCE_NR + 1) % 0x100;
    }
    pkt[LC_OFFSET_HUE] = sc.hue;
    pkt[LC_OFFSET_SATURATION] = sc.saturation;
    pkt[LC_OFFSET_VALUE] = sc.value;
    return pkt;
}

unsigned char *create_packet_ACK(unsigned char *pkt)
{
    unsigned char *pkt_ACK = (unsigned char *)malloc(LC_HEADER_LENGTH + LC_PACKET_LENGTH);
    memcpy(pkt_ACK, pkt, LC_HEADER_LENGTH + LC_PACKET_LENGTH);
    memcpy(pkt_ACK + LC_OFFSET_DST_ADDR, pkt + LC_OFFSET_SRC_ADDR, LC_ADDR_LENGHT);
    memcpy(pkt_ACK + LC_OFFSET_SRC_ADDR, bridge, LC_ADDR_LENGHT);
    pkt_ACK[LC_OFFSET_COMMAND]++;
    return pkt_ACK;
}

bool test_ACK(unsigned char *pkt_ACK)
{
    if (memcmp(pkt_ACK + LC_OFFSET_SRC_ADDR, last_packet + LC_OFFSET_DST_ADDR, LC_ADDR_LENGHT))
        return false;
    if (pkt_ACK[LC_OFFSET_SEQUENCE] != last_packet[LC_OFFSET_SEQUENCE])
        return false;
    if (pkt_ACK[LC_OFFSET_COMMAND] != last_packet[LC_OFFSET_COMMAND] + 1)
        return false;
    return true;
}

void inc_sequence_nr(unsigned char *pkt)
{
    std::lock_guard<std::mutex> lck_SEQUENCE_NR(SEQUENCE_NR_mtx);
    pkt[LC_OFFSET_SEQUENCE] = SEQUENCE_NR;
    SEQUENCE_NR = (SEQUENCE_NR + 1) % 0x100;
}

void js_log(const char *msg)
{
    tsf_log.BlockingCall(new std::string(msg), js_cb_log);
}

void js_change_state(StateChange &sc)
{
    tsf_change_state.BlockingCall(new StateChange(sc), js_cb_change_state);
}

} // namespace lc