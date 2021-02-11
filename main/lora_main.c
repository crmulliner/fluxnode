/*
 * Copyright: Collin Mulliner <collin AT mulliner.org>
 */

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "soc/sens_periph.h"
#include "soc/rtc.h"
#include "freertos/queue.h"
#include <time.h>

#include "lora.h"

#include <duktape.h>

#include "log.h"
#include "util.h"
#include "duk_helpers.h"
#include "duk_main.h"
#include "board.h"

//#define LORA_MAIN_DEBUG 1


/* jsondoc
{
"class": "LoRa",
"longtext": "
Documentation for the LoRa modem API.

The library supports every feature needed for LoRaWAN.
Additionally it supports Frequency hopping spread spectrum (FHSS), see: [setHopping](#sethoppinghopshopfreqs).
The library has been tested on [RFM95](https://www.hoperf.com/modules/lora/RFM95.html).

"
}
*/

static double *fqtable = NULL;
static unsigned int fqtable_entries = 0;

#define ISR_TASK_READ_PACKET 1
#define ISR_TASK_STOP 2
#define ISR_TASK_FHSS 3

enum LoRaMode_T
{
    LORA_IDLE = 0,
    LORA_SLEEP,
    LORA_RECV
};

static enum LoRaMode_T lora_mode;
static xQueueHandle isr_recv_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t cmd = ISR_TASK_READ_PACKET;
    BaseType_t task_woken;
    xQueueSendFromISR(isr_recv_queue, &cmd, &task_woken);
    if (task_woken)
    {
        // important: this will make isr_recv_task() active
        portYIELD_FROM_ISR();
    }
}

static void IRAM_ATTR gpio_fhss_isr_handler(void *arg)
{
    uint32_t cmd = ISR_TASK_FHSS;
    BaseType_t task_woken;
    xQueueSendFromISR(isr_recv_queue, &cmd, &task_woken);
    if (task_woken)
    {
        // important: this will make isr_recv_task() active
        portYIELD_FROM_ISR();
    }
}

void lm_stop_isr_task()
{
    uint32_t cmd = ISR_TASK_STOP;
    xQueueSend(isr_recv_queue, &cmd, portMAX_DELAY);
}

static void isr_recv_task(void *arg)
{
    for (;;)
    {
        uint32_t cmd;
        if (xQueueReceive(isr_recv_queue, (void *)&cmd, portMAX_DELAY))
        {
            // handle hopping
            if (lora_fhss_handle(fqtable, fqtable_entries))
            {
                continue;
            }

            if (cmd != ISR_TASK_READ_PACKET)
            {
                logprintf("%s: isr task cmd is not read_packet: %d\n", __func__, cmd);
            }

            uint8_t *buf = malloc(LORA_MSG_MAX_SIZE + 1);
            int bytes_recv = lora_receive_packet(buf, LORA_MSG_MAX_SIZE + 1);
            int rssi = lora_packet_rssi();
            lora_receive();
#ifdef LORA_MAIN_DEBUG
            logprintf("LoRa received: %d bytes\n", bytes_recv);
#endif
            if (bytes_recv > 0)
            {
                duk_main_add_full_event(LORA_MSG, INCOMING, buf, bytes_recv, rssi, time(NULL));
            }
        }
    }
}

/* jsondoc
{
"name": "loraReceive",
"args": [],
"text": "Set LoRa modem to Receive.
Packets will arrive via OnEvent().
The type of EventData is plain buffer, see: https://wiki.duktape.org/howtobuffers2x",
"example": "
function OnStart() {
  LoRa.loraReceive();
}
function OnEvent(evt) {
  if (evt.EventType == 1) {
    pkt = evt.EventData;
    rssi = evt.LoRaRSSI;
    timestamp = evt.TimeStamp;
  }
}
"
}
*/
int recv_enable()
{
    lora_install_irq_recv(gpio_isr_handler);
    lora_enable_irq_recv(LORA_IRQ_ENABLE);
    lora_install_irq_fhss(gpio_fhss_isr_handler);
    lora_enable_irq_fhss(LORA_IRQ_ENABLE);
    lora_receive();
    lora_mode = LORA_RECV;
    return 0;
}

/* jsondoc
{
"name": "loraSleep",
"args": [],
"text": "Set LoRa modem to sleep. Lowest power consumption.",
"example": "
LoRa.loraSleep();
"
}
*/
int sleep_set()
{
    if (lora_mode == LORA_RECV)
    {
        lora_enable_irq_recv(LORA_IRQ_DISABLE);
    }
    lora_sleep();
    lora_mode = LORA_SLEEP;
    return 0;
}

/* jsondoc
{
"name": "loraIdle",
"args": [],
"text": "Set LoRa modem to idle.",
"example": "
LoRa.loraIdle();
"
}
*/
int idle_set()
{
    if (lora_mode == LORA_RECV)
    {
        lora_enable_irq_recv(LORA_IRQ_DISABLE);
    }
    lora_idle();
    lora_mode = LORA_IDLE;
    return 0;
}

/* jsondoc
{
"name": "sendPacket",
"args": [{"name": "packet_bytes", "vtype": "Plain Buffer", "text": "packet bytes length 1-255"}],
"text": "Send a LoRa packet. LoRa.loraReceive() has to be called before sending. 
The modem can be put into idle or sleep right after sendPacket returns. 
For plain buffers see: https://wiki.duktape.org/howtobuffers2x",
"example": "
LoRa.sendPacket(Uint8Array.plainOf('Hello'));
"
}
*/
static int send_packet(duk_context *ctx)
{
    size_t len;
    uint8_t *buf = duk_require_buffer(ctx, 0, &len);
#if LORA_MAIN_DEBUG
    logprintf("%s: len = %d\n", __func__, len);
#endif
    lora_send_packet_irq(buf, len);
    // back to lora receive
    if (lora_mode == LORA_RECV)
    {
        lora_receive();
    }
    return 0;
}

/* jsondoc
{
"name": "setFrequency",
"args": [{"name": "freq", "vtype": "double", "text": "902.0 - 928.0 (US-915)"}],
"text": "Set the frequency.",
"return": "boolean status",
  "example": "
LoRa.setFrequency(902.3);
"
}
*/
static int set_freq(duk_context *ctx)
{
    double freq = duk_require_number(ctx, 0);
    if (freq < 902.0 || freq > 928.0)
    {
        duk_push_boolean(ctx, 0);
        return 1;
    }
    lora_set_frequency(freq);
    duk_push_boolean(ctx, 1);
    return 1;
}

/* jsondoc
{
"name": "setBW",
"args": [{"name": "bw", "vtype": "uint", "text": "bandwidth 7.8E3 - 5000E3"}],
"return": "boolean status",
"text": "Set the bandwidth.",
"example": "
LoRa.setBW(125E3);
"
}
*/
static int set_bandwidth(duk_context *ctx)
{
    int bw = duk_require_int(ctx, 0);
    if (bw < 7.8E3 || bw > 500E3)
    {
        duk_push_boolean(ctx, 0);
        return 1;
    }
    lora_set_bandwidth(bw);
    duk_push_boolean(ctx, 1);
    return 1;
}

/* jsondoc
{
"name": "setSF",
"args": [{"name": "sf", "vtype": "uint", "text": "spreading factor: 6 - 12"}],
"return": "boolean status",
"text": "Set the spreading factor.",
"example": "
LoRa.setSF(7);
"
}
*/
static int set_sf(duk_context *ctx)
{
    int sf = duk_require_int(ctx, 0);
    if (sf < 6 || sf > 12)
    {
        duk_push_boolean(ctx, 0);
        return 1;
    }
    lora_set_spreading_factor(sf);
    duk_push_boolean(ctx, 1);
    return 1;
}

/* jsondoc
{
"name": "setCR",
"args": [{"name": "cr", "vtype": "uint", "text": "coding rate 5 - 8"}],
"return": "boolean status",
"text": "Set the coding rate.",
"example": "
LoRa.setCR(5);
"
}
*/
static int set_cr(duk_context *ctx)
{
    int cr = duk_require_int(ctx, 0);
    if (cr < 5 || cr > 8)
    {
        duk_push_boolean(ctx, 0);
        return 1;
    }
    lora_set_coding_rate(cr);
    duk_push_boolean(ctx, 1);
    return 1;
}

/* jsondoc
{
"name": "setPreambleLen",
"args": [{"name": "length", "vtype": "uint", "text": "number of symbols: 0 - 65335"}],
"return": "boolean status",
"text": "Set the length of the preamble in symbols.",
"example": "
LoRa.setPreambleLen(8);
"
}
*/
static int set_preamble_len(duk_context *ctx)
{
    int pl = duk_require_int(ctx, 0);
    if (pl < 0 || pl > 0xffff)
    {
        duk_push_boolean(ctx, 0);
        return 1;
    }
    lora_set_preamble_length(pl);
    duk_push_boolean(ctx, 1);
    return 1;
}

/* jsondoc
{
"name": "setSyncWord",
"args": [{"name": "syncword", "vtype": "uint8", "text": "sync word"}],
"text": "Set the LoRa sync word.",
"example": "
LoRa.setSyncWord(0x42);
"
}
*/
static int set_sync_word(duk_context *ctx)
{
    uint sw = duk_require_uint(ctx, 0);
    lora_set_sync_word((uint8_t)sw);
    return 0;
}

/* jsondoc
{
"name": "setCRC",
"args": [{"name": "crc", "vtype": "boolean", "text": "enable = True"}],
"return": "boolean status",
"text": "Enable / Disable packet CRC.",
"example": "
LoRa.setCRC(true);
"
}
*/
static int set_crc(duk_context *ctx)
{
    int crc = duk_require_boolean(ctx, 0);
    if (crc)
        lora_enable_crc();
    else
        lora_disable_crc();
    return 0;
}

/* jsondoc
{
"name": "setPayloadLen",
"args": [{"name": "length", "vtype": "uint", "text": "0 - 255"}],
"text": "Set payload length. If length is set to 0 the header will contain the payload length for each packet.",
"return": "boolean status",
"example": "
LoRa.setPayloadLen(0);
"
}
*/
static int set_payload_len(duk_context *ctx)
{
    int pl = duk_require_int(ctx, 0);
    if (pl > 0 && pl < 256)
    {
        lora_implicit_header_mode(pl);
    }
    else if (pl == 0)
    {
        lora_explicit_header_mode();
    }
    else
    {
        duk_push_boolean(ctx, 0);
        return 1;
    }
    duk_push_boolean(ctx, 1);
    return 1;
}

/* jsondoc
{
"name": "setTxPower",
"args": [{"name": "level", "vtype": "uint", "text": "power level 2 - 17 (17 = PA boost)."}],
"return": "boolean status",
"text": "Set LoRa modem TX power level.",
"example": "
LoRa.setTxPower(17);
"
}
*/
static int set_tx_power(duk_context *ctx)
{
    uint txp = duk_require_uint(ctx, 0);
    if (txp < 2 || txp > 17)
    {
        duk_push_boolean(ctx, 0);
        return 1;
    }
    lora_set_tx_power(txp);
    duk_push_boolean(ctx, 1);
    return 1;
}

/* jsondoc
{
"name": "setIQMode",
"args": [{"name": "iq_invert", "vtype": "boolean", "text": "enable IQ invert"}],
"text": "Enable / Disable IQ invert.",
"example": "
LoRa.setIQMode(true);
"
}
*/
static int set_iq_mode(duk_context *ctx)
{
    int iq = duk_require_boolean(ctx, 0);
    if (iq)
        lora_enable_invert_iq();
    else
        lora_disable_invert_iq();
    return 0;
}

/* jsondoc
{
"name": "setHopping",
"args": [
{"name": "hops", "vtype": "uint", "text": "number of hops, 0 = don't use frequency hopping"},
{"name": "hopfreqs", "vtype": "double[]", "text": "the hopping frequencies, needs to contain at least 1 entry if hops > 0"}
],
"text": "Configure the frequency hopping functionality. Disable frequency hopping by setting hops to `0`.",
"return": "boolean status",
"example": "
// 5 hops
LoRa.setHopping(5, [905,906,907,908,909]);
// disable hopping
LoRa.setHopping(0, []);
"
}
 */
static int set_hopping(duk_context *ctx)
{
    int hops = duk_require_int(ctx, 0);

    if (fqtable)
    {
        free(fqtable);
        fqtable = NULL;
    }

    if (hops == 0)
    {
        lora_fhss_sethops(hops);
        duk_push_boolean(ctx, 1);
        return 1;
    }

    if (!duk_is_array(ctx, 1))
    {
        duk_push_boolean(ctx, 0);
        return 1;
    }
    duk_size_t i, n;

    n = duk_get_length(ctx, 1);
    if (n == 0)
    {
        duk_push_boolean(ctx, 0);
        return 1;
    }

    fqtable_entries = n;
    fqtable = (double *)malloc(sizeof(double) * fqtable_entries);
#ifdef LORA_MAIN_DEBUG
    logprintf("%s: hops %d table %d\n", __func__, hops, fqtable_entries);
#endif

    for (i = 0; i < n; i++)
    {
        if (duk_get_prop_index(ctx, 1, i))
        {
            fqtable[i] = duk_to_number(ctx, -1);
#ifdef LORA_MAIN_DEBUG
            logprintf("%s: entry[%d] = %f\n", __func__, i, fqtable[i]);
#endif
        }
        else
        {
            // element is not present
            logprintf("%s: elemenet not present\n", __func__);
        }
        duk_pop(ctx);
    }

    lora_fhss_sethops(hops);

    duk_push_boolean(ctx, 1);
    return 1;
}

static duk_function_list_entry lora_funcs[] = {
    {"setCRC", set_crc, 1},
    {"setTxPower", set_tx_power, 1},
    {"setPayloadLen", set_payload_len, 1},
    {"setIQMode", set_iq_mode, 1},
    {"setSyncWord", set_sync_word, 1},
    {"setPreambleLen", set_preamble_len, 1},
    {"setCR", set_cr, 1},
    {"setBW", set_bandwidth, 1},
    {"setSF", set_sf, 1},
    {"setFrequency", set_freq, 1},
    {"loraSleep", sleep_set, 0},
    {"loraIdle", idle_set, 0},
    {"loraReceive", recv_enable, 0},
    {"setHopping", set_hopping, 2},
    {"sendPacket", send_packet, 1},
    {NULL, NULL, 0}};

int lora_main_register(duk_context *ctx)
{
    duk_push_global_object(ctx);
    duk_push_object(ctx);

    duk_put_function_list(ctx, -1, lora_funcs);
    duk_put_prop_string(ctx, -2, "LoRa");
    duk_pop(ctx);

    return 1;
}

int lora_main_start()
{
    board_config_t *board = get_board_config();
#ifdef LORE_MAIN_DEBUG
    logprintf("%s: LoRa config: %s\n", __func__, board->boardname);
#endif
    lora_config(board->lora_cs_gpio,
                board->lora_rst_gpio,
                board->lora_miso_gpio,
                board->lora_mosi_gpio,
                board->lora_sck_gpio);
    lora_config_dio(
        board->lora_dio0_gpio,
        board->lora_dio1_gpio,
        board->lora_dio2_gpio);

    if (!lora_init())
    {
        logprintf("%s\n\n------------\nLoRa init failed\n------------\n\n", __func__);
    }
    lora_mode = LORA_SLEEP;

    isr_recv_queue = xQueueCreate(10, sizeof(uint32_t));
    if (xTaskCreate(&isr_recv_task, "lora_isr_recv_task", 2048, NULL, 10, NULL) != 1)
    {
        logprintf("%s: xTaskCreate ERROR\n", __func__);
        return 0;
    }

    return 1;
}
