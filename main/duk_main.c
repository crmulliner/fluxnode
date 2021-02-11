/*
 * Copyright: Collin Mulliner <collin AT mulliner.org>
 */

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "soc/sens_periph.h"
#include "soc/rtc.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include <duktape.h>

#include "log.h"
#include "vfs_config.h"
#include "queue.h"
#include "duk_fs.h"
#include "duk_util.h"
#include "lora_main.h"
#include "duk_helpers.h"
#include "duk_main.h"
#include "platform.h"
#include "web_service.h"
#include "duk_crypto.h"
#include "button_service.h"
#include "board.h"
#include "util.h"

//#define DUK_MAIN_DEBUG 1
//#define TIMER_DEBUG 1

typedef struct event_msg_t event_msg_t;
typedef event_msg_t *event_msg_ptr_t;
struct event_msg_t
{
    event_msg_ptr_t next;
    event_msg_ptr_t prev;

    event_msg_type msg_type;
    event_direction_type msg_direction;

    int rssi;
    time_t ts;
    uint8_t *payload;
    size_t payload_len;
};

struct duk_globals_t
{
    // duktape engine
    duk_context *ctx;
    int reset;

    // timer related
    unsigned int delay;
    unsigned int last_ticks;
    unsigned long int wake_up_timeMS;

    // task
    TaskHandle_t duk_main_task_handle;

    work_queue_t *event_queue;
    // function to send to UI client
    ui_msg_send_func *send_func;

    int load_index;
    char *load_file;
};

#define MS_PER_TICK 10

#define LOAD_FILES_NUM 2
const char *load_files[LOAD_FILES_NUM] = {
    BASE_PATH "/main.js",
    BASE_PATH "/recovery.js",
};

static struct duk_globals_t *g;

static void duk_main_wake()
{
    xTaskNotifyGive(g->duk_main_task_handle);
}

void duk_main_set_load_file(char *fname)
{
    if (g->load_file)
    {
        free(g->load_file);
        g->load_file = NULL;
    }
    g->load_file = fname;
}

static int send_event(duk_context *ctx, event_msg_ptr_t event)
{
    int result = 1;
    duk_push_global_object(ctx);
    duk_get_prop_string(ctx, -1, "OnEvent");

    duk_push_object(ctx);
    duk_push_number(ctx, event->msg_type);
    duk_put_prop_string(ctx, -2, "EventType");

    if (event->payload_len && event->payload != NULL)
    {
        uint8_t *buf = (uint8_t *)duk_push_fixed_buffer(ctx, event->payload_len);
        memcpy(buf, event->payload, event->payload_len);
        duk_put_prop_string(ctx, -2, "EventData");
    }
    if (event->rssi)
    {
        duk_push_number(ctx, event->rssi);
        duk_put_prop_string(ctx, -2, "LoRaRSSI");
    }
    if (event->ts)
    {
        duk_push_number(ctx, event->ts);
        duk_put_prop_string(ctx, -2, "TimeStamp");
    }

    if (event->msg_type == BUTTON_PRESSED)
    {
        duk_push_number(ctx, event->payload_len);
        duk_put_prop_string(ctx, -2, "NumPress");
    }

    duk_insert(ctx, -1);
    if (duk_pcall(ctx, 1 /*nargs*/) != 0)
    {
#ifdef DUK_MAIN_DEBUG
        logprintf("%s: eval failed: '%s'\n", __func__, duk_safe_to_string(ctx, -1));
#endif
        result = 0;
    }
    duk_pop(ctx);
    return result;
}

static int timer_set_check()
{
    g->last_ticks = xTaskGetTickCount();
    unsigned long int delay = g->wake_up_timeMS / MS_PER_TICK;
    delay = delay * 0.9;                                               // wake up after 90% of the delay time has passed
    delay = delay == 0 ? 1 : delay;                                    // minimal delay of 1 tick
    delay = delay > (100 * 60 * 60 * 24) ? 100 * 60 * 60 * 24 : delay; // maximal delay of ticks equiv to 24h
    g->delay = delay;
#ifdef TIMER_DEBUG
    logprintf("%s: delay: %d\n", __func__, g->delay);
#endif
    return 1;
}

static int timer_check()
{
#if 0
    uint64_t tm = rtc_time_get();
    logprintf("%s: tm: %lld\n", __func__, tm);
#endif

    if (g->wake_up_timeMS == 0)
    {
#ifdef TIMER_DEBUG
        logprintf("wake_up_timeMS == 0\n");
#endif
        g->delay = portMAX_DELAY;
        g->last_ticks = 0;
        return 0;
    }

    unsigned long int cur = xTaskGetTickCount();
    unsigned long int ticks_passed = cur - g->last_ticks;
    if (ticks_passed == 0)
    {
#ifdef TIMER_DEBUG
        logprintf("ticks_passed == 0\n");
#endif
        return 0;
    }
    // ticks_passed is now MS passed
    ticks_passed *= MS_PER_TICK;
    // we hit the timer or passed it
    if (ticks_passed >= g->wake_up_timeMS)
    {
        // clear timer
        g->last_ticks = 0;
        g->wake_up_timeMS = 0;
        // timer expired
        return 1;
    }
    // update wake up
    g->wake_up_timeMS = g->wake_up_timeMS - ticks_passed;
    timer_set_check();
    return 0;
}

int duk_main_set_wake_up_time(unsigned long int wake_up_in_MS)
{
    g->wake_up_timeMS = wake_up_in_MS;
#ifdef TIMER_DEBUG
    logprintf("set wake up time: %ld\n", g->wake_up_timeMS);
#endif
    timer_set_check();
    return 1;
}

void duk_main_set_reset(int rst)
{
    g->reset = rst;
}

void duk_main_set_send_func(ui_msg_send_func *func)
{
    g->send_func = func;
}

int duk_main_add_full_event(event_msg_type msg_type, event_direction_type direction, uint8_t *payload, size_t len, int rssi, time_t ts)
{
    event_msg_ptr_t m = malloc(sizeof(event_msg_t));
    m->next = NULL;
    m->prev = NULL;
    m->msg_type = msg_type;
    m->msg_direction = direction;
    m->payload = payload;
    m->ts = ts;
    m->rssi = rssi;
    m->payload_len = len;
    if (m->payload_len == 0 && m->payload != NULL)
    {
        m->payload_len = strlen((char *)payload);
    }
    WORK_QUEUE_RECV_ADD(g->event_queue, m);
    duk_main_wake();
    return 1;
}

int duk_main_add_event(event_msg_type msg_type, event_direction_type direction, uint8_t *payload, size_t len)
{
    return duk_main_add_full_event(msg_type, direction, payload, len, 0, 0);
}

static void work_queue_delete(work_queue_t *q)
{
    for (;;)
    {
        event_msg_ptr_t msg = NULL;
        WORK_QUEUE_RECV_GET(g->event_queue, msg);
        if (msg != NULL)
        {
            if (msg->payload)
            {
                free(msg->payload);
            }
            free(msg);
        }
        else
        {
            break;
        }
    }
}

static void duk_init()
{
#ifdef DUK_MAIN_DEBUG
    logprintf("%s: start\n", __func__);
#endif

load:
    if (g->ctx != NULL)
    {
        duk_destroy_heap(g->ctx);
    }
    g->ctx = duk_create_heap_default();
    // clear queue
    work_queue_delete(g->event_queue);

    g->delay = portMAX_DELAY;
    g->wake_up_timeMS = 0;
    g->reset = 0;

    duk_util_register(g->ctx);
    platform_register(g->ctx);
    duk_fs_register(g->ctx);
    lora_main_register(g->ctx);
    crypto_register(g->ctx);

    char *load_name = g->load_file;
    if (load_name == NULL)
    {
        load_name = (char*) load_files[g->load_index];
    }

#ifdef DUK_MAIN_DEBUG
    logprintf("%s: loading %s\n", __func__, load_name);
#endif
    if (duk_util_load_and_run(g->ctx, load_name, "OnStart();") == 0)
    {
        g->load_index++;
        g->load_index = g->load_index % LOAD_FILES_NUM;

        if (g->load_file == load_name)
        {
            free(g->load_file);
            g->load_file = NULL;
        }

        goto load;
    }
    else
    {
        g->load_index = 0;
    }
    if (g->load_file == load_name)
    {
        free(g->load_file);
        g->load_file = NULL;
    }
}

static void duktape_task(void *ignored)
{
    g->duk_main_task_handle = xTaskGetCurrentTaskHandle();
#ifdef DUK_MAIN_DEBUG
    logprintf("%s started\n", __func__);
#endif

    // trigger init
    g->reset = 1;

    for (;;)
    {
        while (g->reset)
        {
            duk_init();
        }

        int notify = ulTaskNotifyTake(pdTRUE, g->delay);
        // time out
        if (notify == 0)
        {
        }

        // process event_queue, timers
        for (;;)
        {
            if (timer_check())
            {
                duk_util_run(g->ctx, "OnTimer();");
            }

            event_msg_ptr_t msg = NULL;
            WORK_QUEUE_RECV_GET(g->event_queue, msg);
            if (msg == NULL)
            {
                // back to sleep
                break;
            }

            if (msg->msg_direction == INCOMING)
            {
                send_event(g->ctx, msg);
            }
            else
            {
                if (msg->msg_type == UI_MSG)
                {
                    if (g->send_func) {
                        g->send_func(msg->payload, msg->payload_len);
                    }
                }
                else
                {
#ifdef DUK_MAIN_DEBUG
                    logprintf("%s: got OUTGOING event that is not UI_MSG\n", __func__);
#endif
                }
            }
            if (msg->payload)
            {
                free(msg->payload);
            }
            free(msg);
        }
    }
}

static int control_callback(int id, char *data, size_t data_len)
{
#ifdef DUK_MAIN_DEBUG
    logprintf("%s: control callback cmd = %d\n", __func__, id);
#endif
    switch (id)
    {
    case CMD_RESET:
    {
        g->reset = 1;
        duk_main_wake();
    }
    break;
    case CMD_SET_LOAD:
    {
        if (data != NULL)
        {
            duk_main_set_load_file(strdup(data));
        }
    }
    break;
    case CMD_REBOOT:
    {
        esp_restart();
    }
    break;
    }
    return 1;
}

static int button_callback(int presses, unsigned long int timeout, void *cb_data)
{
    // re-enable button
    button_service_enable(1);
    duk_main_add_event(BUTTON_PRESSED, INCOMING, NULL, presses);
    return 1;
}

void duk_main_start()
{
#ifdef DUK_MAIN_DEBUG
    logprintf("%s started\n", __func__);
#endif

    platform_init();
    lora_main_start();

    board_config_t *board = get_board_config();

    if (board->button_gpio != -1)
    {
#ifdef DUK_MAIN_DEBUG
        logprintf("%s: button service active\n", __func__);
#endif
        button_service_start(button_callback, NULL);
    }

    webserver_set_control_func(control_callback);

    g = malloc(sizeof(struct duk_globals_t));
    g->load_file = NULL;
    g->load_index = 0;
    g->send_func = NULL;
    g->event_queue = NULL;
    g->event_queue = malloc(sizeof(work_queue_t));
    WORK_QUEUE_INIT(g->event_queue);
    g->ctx = NULL;

    xTaskCreatePinnedToCore(&duktape_task, "duktape_task", 16 * 1024, NULL, 5, NULL, tskNO_AFFINITY);
}
