#include <stdlib.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>
#include "index.h"
#include "cc2500.h"
#include "cc2500_reg.h"

extern Napi::ThreadSafeFunction tsf_log;
extern Napi::ThreadSafeFunction tsf_ack;

namespace lc
{

using namespace std::chrono_literals;
using steady_clock = std::chrono::steady_clock;
using time_point = std::chrono::time_point<steady_clock>;
using duration = steady_clock::duration;

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

// flags
std::atomic<bool> reset_flag(true);

// cc2500
std::mutex cc2500_mtx;

// ISR thread
std::thread ISR_thread;
std::condition_variable INT_pending_cv;
std::mutex INT_mtx;
std::queue<time_point> INT_queue;
time_point last_INT;
time_point last_RX_CMD_INT;

// TX thread
std::thread TX_thread;
std::condition_variable TX_pending_cv;

// packets
std::mutex pkt_mtx;
// RX ACK packets
unsigned char *RX_ACK_pkt_last[num_lamps];
// TX CMD packets
std::queue<unsigned char *> TX_CMD_pkt_queue;
unsigned char *TX_CMD_pkt_current;
bool TX_CMD_pending;
int TX_CMD_try;
// TX ACK packet
unsigned char *TX_ACK_pkt_current;
bool TX_ACK_pending;

// await RX
std::condition_variable await_RX_cv;
bool await_RX;
// await RX ACK
std::condition_variable await_RX_ACK_cv;
bool await_RX_ACK;
// await TX
std::condition_variable await_TX_cv;
bool await_TX;

// FSCAL thread
std::thread FSCAL_thread;
std::condition_variable FSCAL_cv;

// timeouts
auto cc2500_RX_timeout = 5ms;
auto cc2500_RX_ACK_timeout = 12ms;
auto cc2500_TX_timeout = 5ms;
auto cc2500_try_mode_timeout = 10ms;
auto cc2500_await_mode_timeout = 10ms;

// delays
auto cc2500_setup_retry_delay = 100ms;
auto cc2500_TX_CMD_delay = 6ms;
auto cc2500_TX_ACK_delay = 3ms;
auto cc2500_FSCAL_retry_delay = 1min;
auto cc2500_FSCAL_last_INT_delay = 30s;

// intervals
auto cc2500_FSCAL_interval = 15min;

// retry counts
int cc2500_setup_retries = 2;
int cc2500_FSCAL_retries = 9;
int cc2500_TX_CMD_retries = 3;

// sequence number
std::mutex SEQUENCE_NR_mtx;
unsigned char SEQUENCE_NR = 0;

bool setup()
{
    int i = 0;
    while (!cc2500::setup())
    {
        i++;
        if (i > cc2500_setup_retries)
        {
            js_log("LivingColors exception: cc2500 setup failed too many times");
            return false;
        }
        std::this_thread::sleep_for(cc2500_setup_retry_delay);
    }
    // the mode is IDLE
    {
        std::lock_guard<std::mutex> lck_cc2500(cc2500_mtx);
        // perform FSCAL
        if (!calibrate())
        {
            js_log("LivingColors exception: cc2500 calibration during setup failed");
            return false;
        }
        reset_flag = false;
        start_threads();
        if (!cc2500::set_mode(CC2500_MODE_RX))
        {
            js_log("LivingColors exception: starting RX during setup failed");
            return false;
        }
    }
    return true;
}

void start_threads()
{
    ISR_thread = std::thread(ISR_loop);
    TX_thread = std::thread(TX_loop);
    FSCAL_thread = std::thread(FSCAL_loop);
}

void stop()
{
    notify_threads();
    join_threads();
    cc2500::release();
}

void cc2500_ISR()
{
    if (reset_flag.load())
        return;
    {
        std::lock_guard<std::mutex> lck_INT(INT_mtx);
        last_INT = steady_clock::now();
        INT_queue.push(last_INT);
    }
    INT_pending_cv.notify_one();
}

void ISR_loop()
{
    while (true)
    {
        time_point INT_time;
        {
            std::unique_lock<std::mutex> lck_INT(INT_mtx);
            INT_pending_cv.wait(lck_INT, [] { return !INT_queue.empty() || reset_flag.load(); });
            if (reset_flag.load())
                return;
            INT_time = INT_queue.front();
            INT_queue.pop();
        }
        std::unique_lock<std::mutex> lck_cc2500(cc2500_mtx);
        if (await_TX)
        {
            // TX ISR
            if (TX_ACK_pending)
            {
                TX_ACK_pending = false;
                free(TX_ACK_pkt_current);
            }
            await_TX = false;
            lck_cc2500.unlock();
            await_TX_cv.notify_one();
            continue;
        }
        // RX ISR
        if (!try_mode(CC2500_MODE_FSTXON))
        {
            initiate_reset();
            return;
        }
        // the mode is FSTXON
        unsigned char RX_bytes = cc2500::get_RX_bytes();
        if (RX_bytes == CC2500_ERR)
        {
            initiate_reset();
            return;
        }
        if (RX_bytes == 0x00)
        {
            // the packet was discarded, restart RX
            if (!cc2500::set_mode(CC2500_MODE_RX))
            {
                js_log("LivingColors exception: restarting RX failed");
                initiate_reset();
                return;
            }
            continue;
        }
        bool RX_completed = false;
        bool restart_RX = true;
        if (await_RX)
        {
            await_RX = false;
            restart_RX = false;
        }
        goto l_receive_pkt;
    l_next_INT:
    {
        std::lock_guard<std::mutex> lck_INT(INT_mtx);
        if (!INT_queue.empty())
        {
            INT_time = INT_queue.front();
            INT_queue.pop();
        }
        else
        {
            js_log("LivingColors warning: INT missed");
        }
    }
    l_receive_pkt:
        if (RX_bytes >= CC2500_LENGTH_HEADER + LC_LENGTH_PAYLOAD + CC2500_LENGTH_TRAILER)
        {
            // the RX FIFO contains a packet
            unsigned char *RX_pkt = cc2500::receive();
            if (!RX_pkt)
            {
                js_log("LivingColors exception: RX failed");
                initiate_reset();
                return;
            }
            unsigned char length = cc2500::get_pkt_length(RX_pkt);
            if (RX_bytes >= length)
            {
                RX_bytes -= length;
            }
            else
            {
                js_log("LivingColors exception: header contained invalid packet length field");
                initiate_reset();
                return;
            }
            if (RX_bytes == 0x00)
            {
                RX_completed = true;
                {
                    std::lock_guard<std::mutex> lck_INT(INT_mtx);
                    if (!INT_queue.empty())
                    {
                        INT_queue = std::queue<time_point>();
                        js_log("LivingColors warning: unexpected INT received");
                    }
                }
            }
            if (test_lc(RX_pkt) && test_CRC(RX_pkt))
            {
                if (memcmp(RX_pkt + LC_OFFSET_DST_ADDR, bridge, LC_LENGHT_ADDR) == 0)
                {
                    if (memcmp(RX_pkt + LC_OFFSET_SRC_ADDR, remotes[0], LC_LENGHT_ADDR) == 0)
                    {
                        // we received a packet from the remote
                        unsigned char *RX_CMD_pkt = RX_pkt;
                        {
                            std::lock_guard<std::mutex> lck_pkt(pkt_mtx);
                            if (TX_CMD_pending)
                            {
                                goto l_discard_pkt;
                            }
                            if (TX_ACK_pending)
                            {
                                free(TX_ACK_pkt_current);
                            }
                            TX_ACK_pkt_current = create_TX_ACK_pkt(RX_CMD_pkt);
                            TX_ACK_pending = true;
                            for (int i = 0; i < num_lamps; i++)
                            {
                                unsigned char *TX_CMD_pkt = create_TX_CMD_pkt(RX_CMD_pkt, i);
                                TX_CMD_pkt_queue.push(TX_CMD_pkt);
                                if (TX_CMD_pkt_queue.size() > 10)
                                {
                                    js_log("LivingColors warning: TX CMD queue size > 10");
                                }
                            }
                        }
                        last_RX_CMD_INT = INT_time;
                        free(RX_CMD_pkt);
                        if (RX_completed)
                        {
                            lck_cc2500.unlock();
                        }
                        TX_pending_cv.notify_one();
                        if (RX_completed)
                        {
                            continue;
                        }
                        restart_RX = false;
                        goto l_next_INT;
                    }
                    else
                    {
                        int lamp = get_lamp(RX_pkt + LC_OFFSET_SRC_ADDR);
                        if (lamp > -1)
                        {
                            // we received a packet from a lamp
                            unsigned char *RX_ACK_pkt = RX_pkt;
                            if (await_RX_ACK)
                            {
                                if (test_RX_ACK(RX_ACK_pkt))
                                {
                                    {
                                        std::lock_guard<std::mutex> lck_pkt(pkt_mtx);
                                        if (RX_ACK_pkt_last[lamp])
                                            free(RX_ACK_pkt_last[lamp]);
                                        RX_ACK_pkt_last[lamp] = RX_ACK_pkt;
                                    }
                                    StateChange sc = create_StateChange(RX_ACK_pkt_last[lamp]);
                                    js_ack(sc);
                                    await_RX_ACK = false;
                                    if (RX_completed)
                                    {
                                        lck_cc2500.unlock();
                                    }
                                    await_RX_ACK_cv.notify_one();
                                    await_RX_cv.notify_one();
                                    if (RX_completed)
                                    {
                                        continue;
                                    }
                                    restart_RX = false;
                                    goto l_next_INT;
                                }
                                js_log("LivingColors warning: received wrong ACK");
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
            }
            else
            {
                js_log("LivingColors warning: received packet was not a valid LivingColors packet or CRC failed");
            }
        l_discard_pkt:
            free(RX_pkt);
            if (!RX_completed)
            {
                goto l_next_INT;
            }
        }
        else
        {
            // the RX FIFO contains an invalid number of bytes < 17
            js_log("LivingColors warning: RX FIFO contains invalid number of bytes (RX BYTES < 17)");
            if (!cc2500::empty_RXFIFO())
            {
                js_log("LivingColors exception: emptying RX FIFO failed");
                initiate_reset();
                return;
            }
            {
                std::lock_guard<std::mutex> lck_INT(INT_mtx);
                if (!INT_queue.empty())
                {
                    INT_queue = std::queue<time_point>();
                    js_log("LivingColors warning: multiple INTs received but RX BYTES was < 17");
                }
            }
        }
        if (restart_RX)
        {
            if (!cc2500::set_mode(CC2500_MODE_RX))
            {
                js_log("LivingColors exception: restarting RX failed");
                initiate_reset();
                return;
            }
            continue;
        }
        lck_cc2500.unlock();
        await_RX_cv.notify_one();
    }
}

void TX_loop()
{
    while (true)
    {
        std::unique_lock<std::mutex> lck_cc2500(cc2500_mtx, std::defer_lock);
        std::unique_lock<std::mutex> lck_pkt(pkt_mtx, std::defer_lock);
        if (!TX_CMD_pending)
        {
            lck_pkt.lock();
            while (true)
            {
                while (!TX_CMD_pkt_queue.empty())
                {
                    TX_CMD_pkt_current = TX_CMD_pkt_queue.front();
                    TX_CMD_pkt_queue.pop();
                    if (!test_TX_CMD(TX_CMD_pkt_current))
                    {
                        free(TX_CMD_pkt_current);
                        continue;
                    }
                    TX_CMD_try = 0;
                    TX_CMD_pending = true;
                    break;
                }
            l_TX_pending_wait:
                if (TX_CMD_pending || TX_ACK_pending || reset_flag.load())
                {
                    lck_pkt.unlock();
                    break;
                }
                TX_pending_cv.wait(lck_pkt);
            }
        }
        if (reset_flag.load())
            return;
        lck_cc2500.lock();
        if (TX_ACK_pending)
        {
            if (TX_CMD_pending)
            {
                if (!cc2500::set_TXOFF_mode(CC2500_MODE_FSTXON))
                {
                    initiate_reset();
                    return;
                }
            }
            duration dt = steady_clock::now() - last_RX_CMD_INT;
            duration t_remaining = std::chrono::milliseconds(cc2500_TX_ACK_delay) - dt;
            if (t_remaining > duration::zero())
            {
                std::this_thread::sleep_for(t_remaining);
            }
            await_TX = true;
            if (!cc2500::transmit(TX_ACK_pkt_current))
            {
                js_log("LivingColors exception: TX failed");
                initiate_reset();
                return;
            }
            if (!await_TX_cv.wait_for(lck_cc2500, cc2500_TX_timeout, [] { return !await_TX; }))
            {
                js_log("LivingColors exception: timeout expired while waiting for TX");
                initiate_reset();
                return;
            }
            if (TX_CMD_pending)
            {
                if (!await_mode(CC2500_MODE_FSTXON))
                {
                    initiate_reset();
                    return;
                }
                if (!cc2500::set_TXOFF_mode(CC2500_MODE_RX))
                {
                    initiate_reset();
                    return;
                }
                goto l_send_TX_CMD_pkt;
            }
            continue;
        }
        // TX_CMD_pending is true
        // the mode is (or is transitioning to) RX or FSTXON
        if (!try_mode(CC2500_MODE_FSTXON))
        {
            initiate_reset();
            return;
        }
        // the mode is FSTXON
        {
            unsigned char RX_bytes = cc2500::get_RX_bytes();
            if (RX_bytes == CC2500_ERR)
            {
                initiate_reset();
                return;
            }
            if (RX_bytes != 0x00)
            {
                await_RX = true;
                if (!await_RX_cv.wait_for(lck_cc2500, cc2500_RX_timeout, [] { return !await_RX; }))
                {
                    js_log("LivingColors exception: timeout expired while waiting for RX");
                    initiate_reset();
                    return;
                }
            }
        }
    l_send_TX_CMD_pkt:
        if (TX_CMD_try == 0)
        {
            await_RX_ACK = true;
        }
        else
        {
            if (!await_RX_ACK)
            {
                goto l_ACK_received;
            }
            inc_sequence_nr(TX_CMD_pkt_current);
        }
        {
            duration dt = steady_clock::now() - last_INT;
            duration t_remaining = std::chrono::milliseconds(cc2500_TX_CMD_delay) - dt;
            if (t_remaining > duration::zero())
            {
                std::this_thread::sleep_for(t_remaining);
            }
        }
        await_TX = true;
        if (!cc2500::transmit(TX_CMD_pkt_current))
        {
            js_log("LivingColors exception: TX failed");
            initiate_reset();
            return;
        }
        if (!await_TX_cv.wait_for(lck_cc2500, cc2500_TX_timeout, [] { return !await_TX; }))
        {
            js_log("LivingColors exception: timeout expired while waiting for TX");
            initiate_reset();
            return;
        }
        if (!await_RX_ACK_cv.wait_for(lck_cc2500, cc2500_RX_ACK_timeout, [] { return !await_RX_ACK; }))
        {
            if (++TX_CMD_try > cc2500_TX_CMD_retries)
            {
                lck_pkt.lock();
                await_RX_ACK = false;
                TX_CMD_pending = false;
                free(TX_CMD_pkt_current);
                TX_CMD_try = 0;
                js_log("LivingColors warning: no ACK received");
            }
            continue;
        }
    l_ACK_received:
        lck_pkt.lock();
        TX_CMD_pending = false;
        free(TX_CMD_pkt_current);
        TX_CMD_try = 0;
        while (!TX_CMD_pkt_queue.empty())
        {
            TX_CMD_pkt_current = TX_CMD_pkt_queue.front();
            TX_CMD_pkt_queue.pop();
            if (!test_TX_CMD(TX_CMD_pkt_current))
            {
                free(TX_CMD_pkt_current);
                continue;
            }
            TX_CMD_pending = true;
            lck_pkt.unlock();
            goto l_send_TX_CMD_pkt;
        }
        if (!cc2500::set_mode(CC2500_MODE_RX))
        {
            js_log("LivingColors exception: restarting RX failed");
            initiate_reset();
            return;
        }
        lck_cc2500.unlock();
        goto l_TX_pending_wait;
    }
}

void FSCAL_loop()
{
    while (true)
    {
        std::unique_lock<std::mutex> lck_cc2500(cc2500_mtx);
        time_point t0 = steady_clock::now();
        duration t_remaining = cc2500_FSCAL_interval;
        do
        {
            FSCAL_cv.wait_for(lck_cc2500, t_remaining);
            if (reset_flag.load())
                return;
            duration dt = steady_clock::now() - t0;
            t_remaining = cc2500_FSCAL_interval - dt;
        } while (t_remaining > duration::zero());
        int i = 0;
        unsigned char RX_bytes = 0x00;
        do
        {
            bool recent_action = (steady_clock::now() - last_INT) < cc2500_FSCAL_last_INT_delay;
            while (await_RX || await_RX_ACK || recent_action || (RX_bytes != 0x00))
            {
                if (++i > cc2500_FSCAL_retries)
                {
                    js_log("LivingColors exception: failed to perform calibration too many times");
                    initiate_reset();
                    return;
                }
                time_point t0 = steady_clock::now();
                duration t_remaining = cc2500_FSCAL_retry_delay;
                do
                {
                    FSCAL_cv.wait_for(lck_cc2500, t_remaining);
                    if (reset_flag.load())
                        return;
                    duration dt = steady_clock::now() - t0;
                    t_remaining = cc2500_FSCAL_retry_delay - dt;
                } while (t_remaining > duration::zero());
                recent_action = (steady_clock::now() - last_INT) < cc2500_FSCAL_last_INT_delay;
            }
            // the mode is (or is transitioning to) RX or FSTXON
            if (!try_mode(CC2500_MODE_FSTXON))
            {
                initiate_reset();
                return;
            }
            // the mode is FSTXON
            RX_bytes = cc2500::get_RX_bytes();
            if (RX_bytes == CC2500_ERR)
            {
                initiate_reset();
                return;
            }
        } while (RX_bytes != 0x00);
        // perform FSCAL
        if (!calibrate())
        {
            initiate_reset();
            return;
        }
        // restart RX
        if (!cc2500::set_mode(CC2500_MODE_RX))
        {
            js_log("LivingColors exception: restarting RX failed");
            initiate_reset();
            return;
        }
    }
}

bool calibrate()
{
    if (!try_mode(CC2500_MODE_IDLE))
    {
        return false;
    }
    if (!cc2500::send_strobe_cmd(CC2500_REG_SCAL))
    {
        js_log("LivingColors exception: strobe command SCAL failed");
        return false;
    }
    if (!await_mode(CC2500_MODE_IDLE))
    {
        return false;
    }
    return true;
}

bool await_mode(unsigned char mode)
{
    auto t0 = std::chrono::steady_clock::now();
    unsigned char res;
    while ((res = cc2500::get_mode()) != mode)
    {
        if (res == CC2500_ERR)
        {
            return false;
        }
        auto dt = std::chrono::steady_clock::now() - t0;
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
    auto t0 = std::chrono::steady_clock::now();
    unsigned char res;
    while ((res = cc2500::get_mode()) != mode)
    {
        if (res == CC2500_ERR)
        {
            return false;
        }
        auto dt = std::chrono::steady_clock::now() - t0;
        if (dt > cc2500_try_mode_timeout)
        {
            std::string msg = "LivingColors exception: timeout expired while trying to change the mode to " + std::to_string(mode);
            js_log(msg.c_str());
            return false;
        }
        if (!cc2500::set_mode(mode))
        {
            js_log("LivingColors exception: setting mode in try mode failed");
            return false;
        }
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
    stop();
    {
        std::lock_guard<std::mutex> lck_INT(INT_mtx);
        INT_queue = std::queue<time_point>();
    }
    {
        std::lock_guard<std::mutex> lck_cc2500(cc2500_mtx);
        await_RX = false;
        await_RX_ACK = false;
        await_TX = false;
    }
    {
        std::lock_guard<std::mutex> lck_pkt(pkt_mtx);
        TX_CMD_pending = false;
        TX_CMD_try = 0;
        TX_CMD_pkt_queue = std::queue<unsigned char *>();
        TX_ACK_pending = false;
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
        std::lock_guard<std::mutex> lck_INT(INT_mtx);
        std::lock_guard<std::mutex> lck_cc2500(cc2500_mtx);
        std::lock_guard<std::mutex> lck_pkt(pkt_mtx);
        reset_flag = true;
    }
    INT_pending_cv.notify_one();
    TX_pending_cv.notify_one();
    FSCAL_cv.notify_one();
}

void join_threads()
{
    if (ISR_thread.joinable())
        ISR_thread.join();
    if (TX_thread.joinable())
        TX_thread.join();
    if (FSCAL_thread.joinable())
        FSCAL_thread.join();
}

bool enqueue_StateChange(StateChange &sc)
{
    if (reset_flag.load())
    {
        js_log("LivingColors exception: reset flag is set");
        return false;
    }
    unsigned char *pkt = create_TX_CMD_pkt(sc);
    {
        std::lock_guard<std::mutex> lck_pkt(pkt_mtx);
        if (TX_CMD_pkt_queue.size() < 10)
        {
            TX_CMD_pkt_queue.push(pkt);
        }
        else
        {
            free(pkt);
            js_log("LivingColors exception: maximum TX queue size reached");
            return false;
        }
    }
    TX_pending_cv.notify_one();
    return true;
}

StateChange create_StateChange(unsigned char *RX_ACK_pkt)
{
    StateChange sc;
    int lamp = get_lamp(RX_ACK_pkt + LC_OFFSET_SRC_ADDR);
    sc.lamp = lamp;
    sc.command = RX_ACK_pkt[LC_OFFSET_COMMAND];
    sc.hue = RX_ACK_pkt[LC_OFFSET_HUE];
    sc.saturation = RX_ACK_pkt[LC_OFFSET_SATURATION];
    sc.value = RX_ACK_pkt[LC_OFFSET_VALUE];
    return sc;
}

unsigned char *create_TX_CMD_pkt(StateChange &sc)
{
    unsigned char *TX_CMD_pkt = (unsigned char *)malloc(CC2500_LENGTH_HEADER + LC_LENGTH_PAYLOAD);
    TX_CMD_pkt[LC_OFFSET_LENGTH] = LC_LENGTH_PAYLOAD;
    memcpy(TX_CMD_pkt + LC_OFFSET_DST_ADDR, lamps[sc.lamp], LC_LENGHT_ADDR);
    memcpy(TX_CMD_pkt + LC_OFFSET_SRC_ADDR, bridge, LC_LENGHT_ADDR);
    TX_CMD_pkt[LC_OFFSET_PTCL_INFO] = LC_PTCL_INFO;
    TX_CMD_pkt[LC_OFFSET_COMMAND] = sc.command;
    {
        std::lock_guard<std::mutex> lck_SEQUENCE_NR(SEQUENCE_NR_mtx);
        TX_CMD_pkt[LC_OFFSET_SEQUENCE] = SEQUENCE_NR;
        SEQUENCE_NR = (SEQUENCE_NR + 1) % 0x100;
    }
    TX_CMD_pkt[LC_OFFSET_HUE] = sc.hue;
    TX_CMD_pkt[LC_OFFSET_SATURATION] = sc.saturation;
    TX_CMD_pkt[LC_OFFSET_VALUE] = sc.value;
    return TX_CMD_pkt;
}

unsigned char *create_TX_CMD_pkt(unsigned char *RX_CMD_pkt, uint32_t lamp)
{
    unsigned char *TX_CMD_pkt = (unsigned char *)malloc(CC2500_LENGTH_HEADER + LC_LENGTH_PAYLOAD);
    memcpy(TX_CMD_pkt, RX_CMD_pkt, CC2500_LENGTH_HEADER + LC_LENGTH_PAYLOAD);
    memcpy(TX_CMD_pkt + LC_OFFSET_DST_ADDR, lamps[lamp], LC_LENGHT_ADDR);
    memcpy(TX_CMD_pkt + LC_OFFSET_SRC_ADDR, bridge, LC_LENGHT_ADDR);
    {
        std::lock_guard<std::mutex> lck_SEQUENCE_NR(SEQUENCE_NR_mtx);
        TX_CMD_pkt[LC_OFFSET_SEQUENCE] = SEQUENCE_NR;
        SEQUENCE_NR = (SEQUENCE_NR + 1) % 0x100;
    }
    return TX_CMD_pkt;
}

unsigned char *create_TX_ACK_pkt(unsigned char *RX_CMD_pkt)
{
    unsigned char *TX_ACK_pkt = (unsigned char *)malloc(CC2500_LENGTH_HEADER + LC_LENGTH_PAYLOAD);
    memcpy(TX_ACK_pkt, RX_CMD_pkt, CC2500_LENGTH_HEADER + LC_LENGTH_PAYLOAD);
    memcpy(TX_ACK_pkt + LC_OFFSET_DST_ADDR, RX_CMD_pkt + LC_OFFSET_SRC_ADDR, LC_LENGHT_ADDR);
    memcpy(TX_ACK_pkt + LC_OFFSET_SRC_ADDR, bridge, LC_LENGHT_ADDR);
    TX_ACK_pkt[LC_OFFSET_COMMAND]++;
    return TX_ACK_pkt;
}

bool test_RX_ACK(unsigned char *RX_ACK_pkt)
{
    if (memcmp(RX_ACK_pkt + LC_OFFSET_SRC_ADDR, TX_CMD_pkt_current + LC_OFFSET_DST_ADDR, LC_LENGHT_ADDR))
        return false;
    if (RX_ACK_pkt[LC_OFFSET_SEQUENCE] != TX_CMD_pkt_current[LC_OFFSET_SEQUENCE])
        return false;
    if (RX_ACK_pkt[LC_OFFSET_COMMAND] != TX_CMD_pkt_current[LC_OFFSET_COMMAND] &&
        RX_ACK_pkt[LC_OFFSET_COMMAND] != TX_CMD_pkt_current[LC_OFFSET_COMMAND] + 1)
        return false;
    return true;
}

bool test_TX_CMD(unsigned char *TX_CMD_pkt)
{
    if (TX_CMD_pkt[LC_OFFSET_COMMAND] == LC_COMMAND_ON || TX_CMD_pkt[LC_OFFSET_COMMAND] == LC_COMMAND_OFF)
        return true;
    int lamp = get_lamp(TX_CMD_pkt + LC_OFFSET_DST_ADDR);
    if (!RX_ACK_pkt_last[lamp])
        return true;
    if (RX_ACK_pkt_last[lamp][LC_OFFSET_COMMAND] != TX_CMD_pkt[LC_OFFSET_COMMAND] + 1)
        return true;
    if (RX_ACK_pkt_last[lamp][LC_OFFSET_HUE] != TX_CMD_pkt[LC_OFFSET_HUE])
        return true;
    if (RX_ACK_pkt_last[lamp][LC_OFFSET_SATURATION] != TX_CMD_pkt[LC_OFFSET_SATURATION])
        return true;
    if (RX_ACK_pkt_last[lamp][LC_OFFSET_VALUE] != TX_CMD_pkt[LC_OFFSET_VALUE])
        return true;
    return false;
}

bool test_CRC(unsigned char *RX_pkt)
{
    return RX_pkt[LC_OFFSET_CRC] & CC2500_MSK_CRC;
}

bool test_lc(unsigned char *RX_pkt)
{
    return RX_pkt[LC_OFFSET_LENGTH] == LC_LENGTH_PAYLOAD && RX_pkt[LC_OFFSET_PTCL_INFO] == LC_PTCL_INFO;
}

void inc_sequence_nr(unsigned char *TX_CMD_pkt)
{
    std::lock_guard<std::mutex> lck_SEQUENCE_NR(SEQUENCE_NR_mtx);
    TX_CMD_pkt[LC_OFFSET_SEQUENCE] = SEQUENCE_NR;
    SEQUENCE_NR = (SEQUENCE_NR + 1) % 0x100;
}

int get_lamp(unsigned char *lamp_addr)
{
    int lamp = -1;
    for (int i = 0; i < num_lamps; i++)
    {
        if (memcmp(lamp_addr, lamps[i], LC_LENGHT_ADDR) == 0)
        {
            lamp = i;
            break;
        }
    }
    return lamp;
}

void js_log(const char *msg)
{
    tsf_log.BlockingCall(new std::string(msg), js_cb_log);
}

void js_ack(StateChange &sc)
{
    tsf_ack.BlockingCall(new StateChange(sc), js_cb_ack);
}

} // namespace lc