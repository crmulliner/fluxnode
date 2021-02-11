/*
 * Improvements Copyright: Collin Mulliner
 *
 * based on: https://github.com/Inteform/esp32-lora-library
 * and since that is based https://github.com/sandeepmistry/arduino-LoRa
 * the license is: MIT
 */

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"

#include "lora.h"

//#define LORA_DEBUG 1

// Registers
#define REG_FIFO 0x00
#define REG_OP_MODE 0x01
#define REG_FRF_MSB 0x06
#define REG_FRF_MID 0x07
#define REG_FRF_LSB 0x08
#define REG_PA_CONFIG 0x09
#define REG_PA_RAMP 0x0A
#define REG_LNA 0x0c
#define REG_FIFO_ADDR_PTR 0x0d
#define REG_FIFO_TX_BASE_ADDR 0x0e
#define REG_FIFO_RX_BASE_ADDR 0x0f
#define REG_FIFO_RX_CURRENT_ADDR 0x10
#define REG_IRQ_FLAGS 0x12
#define REG_RX_NB_BYTES 0x13
#define REG_MODEM_STAT 0x18
#define REG_PKT_SNR_VALUE 0x19
#define REG_PKT_RSSI_VALUE 0x1a
#define REG_RSSI_VALUE 0x1b
#define REG_HOP_CHANNEL 0x1c
#define REG_MODEM_CONFIG_1 0x1d
#define REG_MODEM_CONFIG_2 0x1e
#define REG_RX_TIMEOUT 0x1f
#define REG_PREAMBLE_MSB 0x20
#define REG_PREAMBLE_LSB 0x21
#define REG_PAYLOAD_LENGTH 0x22
#define REG_HOP_PERIOD 0x24
#define REG_MODEM_CONFIG_3 0x26
#define REG_RSSI_WIDEBAND 0x2c
#define REG_DETECTION_OPTIMIZE 0x31
#define REG_INVERT_IQ_1 0x33
#define REG_HIGH_BW_OPTIMIZE_1 0x36
#define REG_DETECTION_THRESHOLD 0x37
#define REG_SYNC_WORD 0x39
#define REG_HIGH_BW_OPTIMIZE_2 0x3A
#define REG_INVERT_IQ_2 0x3B
#define REG_DIO_MAPPING_1 0x40
#define REG_VERSION 0x42
#define REG_TCXO 0x4B
#define REG_PA_DAC 0x4D

// Transceiver modes
#define MODE_LONG_RANGE_MODE 0x80
#define MODE_SLEEP 0x00
#define MODE_STDBY 0x01
#define MODE_TX 0x03
#define MODE_RX_CONTINUOUS 0x05
#define MODE_RX_SINGLE 0x06
// fake state for internal usage
#define MODE_RX_READ_BUF 0x07

#define RF_IMAGECAL_IMAGECAL_MASK 0xBF
#define RF_IMAGECAL_IMAGECAL_START 0x40
#define RF_IMAGECAL_IMAGECAL_RUNNING 0x20

// PA configuration
#define PA_BOOST 0x80

// IRQ masks
#define IRQ_TX_DONE_MASK 0x08
#define IRQ_PAYLOAD_CRC_ERROR_MASK 0x20
#define IRQ_RX_DONE_MASK 0x40
#define IRQ_FHSS_CHANGE_CHANNEL 0x02
#define IRQ_RX_TIMEOUT 0x80
#define IRQ_VALID_HDR_MASK 0x10
#define IRQ_CAD_DONE_MASK 0x04
#define IRQ_CAD_DETECT_MASK 0x01

#define PA_OUTPUT_RFO_PIN 0
#define PA_OUTPUT_PA_BOOST_PIN 1

#define TIMEOUT_RESET 100

struct lora_config_t
{
  int gpio_cs;
  int gpio_rst;
  int gpio_miso;
  int gpio_mosi;
  int gpio_sck;
  int gpio_dio0;
  int gpio_dio1;
  int gpio_dio2;
};

static spi_device_handle_t __spi;
static int __implicit;
static long __frequency;
static int _modem_state;
// gpio config
static struct lora_config_t config;

/**
 * Config GPIOs
 * @param gpio_cs chip select
 * @param gpio_rst reset
 * @param gpio_miso MISO
 * @param gpio_mosi MOSI
 * @param gpio_sck Clock
 */
void lora_config(const int gpio_cs, const int gpio_rst, const int gpio_miso, const int gpio_mosi, const int gpio_sck)
{
  config.gpio_cs = gpio_cs;
  config.gpio_rst = gpio_rst;
  config.gpio_mosi = gpio_mosi;
  config.gpio_miso = gpio_miso;
  config.gpio_sck = gpio_sck;
}

/**
 * Config DIO GPIOs
 * @param dio0 Dio0
 * @param dio1 Dio0
 * @param dio2 Dio0
 */
void lora_config_dio(const int gpio_dio0, const int gpio_dio1, const int gpio_dio2)
{
  config.gpio_dio0 = gpio_dio0;
  config.gpio_dio1 = gpio_dio1;
  config.gpio_dio2 = gpio_dio2;
}

/**
 * Write a value to a register.
 * @param reg Register index.
 * @param val Value to write.
 */
void lora_write_reg(int reg, int val)
{
  uint8_t out[2] = {0x80 | reg, val};
  uint8_t in[2];

  spi_transaction_t t = {
      .flags = 0,
      .length = 8 * sizeof(out),
      .tx_buffer = out,
      .rx_buffer = in};

  gpio_set_level(config.gpio_cs, 0);
  spi_device_transmit(__spi, &t);
  gpio_set_level(config.gpio_cs, 1);
}

/**
 * Read the current value of a register.
 * @param reg Register index.
 * @return Value of the register.
 */
int lora_read_reg(int reg)
{
  uint8_t out[2] = {reg, 0xff};
  uint8_t in[2];

  spi_transaction_t t = {
      .flags = 0,
      .length = 8 * sizeof(out),
      .tx_buffer = out,
      .rx_buffer = in};

  gpio_set_level(config.gpio_cs, 0);
  spi_device_transmit(__spi, &t);
  gpio_set_level(config.gpio_cs, 1);
  return in[1];
}

/**
 * Perform physical reset on the Lora chip
 */
void lora_reset(void)
{
  gpio_set_level(config.gpio_rst, 0);
  vTaskDelay(pdMS_TO_TICKS(1));
  gpio_set_level(config.gpio_rst, 1);
  vTaskDelay(pdMS_TO_TICKS(5));
}

/**
 * Configure explicit header mode.
 * Packet size will be included in the frame.
 */
void lora_explicit_header_mode(void)
{
  __implicit = 0;
  lora_write_reg(REG_MODEM_CONFIG_1, lora_read_reg(REG_MODEM_CONFIG_1) & 0xfe);
}

/**
 * Configure implicit header mode.
 * All packets will have a predefined size.
 * @param size Size of the packets.
 */
void lora_implicit_header_mode(const int size)
{
  __implicit = 1;
  lora_write_reg(REG_MODEM_CONFIG_1, lora_read_reg(REG_MODEM_CONFIG_1) | 0x01);
  lora_write_reg(REG_PAYLOAD_LENGTH, size);
}

/**
 * Sets the radio transceiver in idle mode.
 * Must be used to change registers and access the FIFO.
 */
void lora_idle(void)
{
  _modem_state = MODE_STDBY;
  lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
}

/**
 * Sets the radio transceiver in sleep mode.
 * Low power consumption and FIFO is lost.
 */
void lora_sleep(void)
{
  _modem_state = MODE_SLEEP;
  lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_SLEEP);
}

/**
 * Sets the radio transceiver in receive mode.
 * Incoming packets will be received.
 */
void lora_receive(void)
{
  _modem_state = MODE_RX_CONTINUOUS;
  lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_RX_CONTINUOUS);
}

/**
 * Configure power level for transmission
 * @param level 2-17, from least to most power
 */
void lora_set_tx_power(int level)
{
  // RF9x module uses PA_BOOST pin
  if (level < 2)
    level = 2;
  else if (level > 17)
    level = 17;
  lora_write_reg(REG_PA_CONFIG, PA_BOOST | (level - 2));
}

/**
 * Calculate frequency.
 * @param freq frequency in Mhz e.g. 902.12
 * @return register values for given frequency
 */
static unsigned int freq_regs(const double freq)
{
  double fr = (freq / 0.61035) * 1000;
  int add = 0;
  unsigned long long ax = fr * 100;
  unsigned long long ay = ((unsigned long long)fr) * 100;
  if (((ax - ay) % 10) >= 5)
  {
    add = 1;
  }
  double ff = fr * 10;
  return ff + add;
}

/**
 * Set carrier frequency.
 * @param frequency Frequency in Mhz (e.g. 902.3)
 */
void lora_set_frequency(const double frequency)
{
  __frequency = frequency;

  unsigned long frf = freq_regs(frequency);
  //printf("0x%.2x 0x%.2x 0x%.2x\n", (uint8_t)(frf >> 16), (uint8_t)(frf >> 8), (uint8_t)(frf >> 0));

  lora_write_reg(REG_FRF_MSB, (uint8_t)(frf >> 16));
  lora_write_reg(REG_FRF_MID, (uint8_t)(frf >> 8));
  lora_write_reg(REG_FRF_LSB, (uint8_t)(frf >> 0));
}

/**
 * Set spreading factor.
 * @param sf 6-12, Spreading factor to use.
 */
void lora_set_spreading_factor(int sf)
{
  if (sf < 6)
    sf = 6;
  else if (sf > 12)
    sf = 12;

  if (sf == 6)
  {
    lora_write_reg(REG_DETECTION_OPTIMIZE, 0xc5);
    lora_write_reg(REG_DETECTION_THRESHOLD, 0x0c);
  }
  else
  {
    lora_write_reg(REG_DETECTION_OPTIMIZE, 0xc3);
    lora_write_reg(REG_DETECTION_THRESHOLD, 0x0a);
  }

  lora_write_reg(REG_MODEM_CONFIG_2, (lora_read_reg(REG_MODEM_CONFIG_2) & 0x0f) | ((sf << 4) & 0xf0));
}

/**
 * Set bandwidth (bit rate)
 * @param sbw Bandwidth in Hz (up to 500000)
 */
void lora_set_bandwidth(long sbw)
{
  int bw;

  if (sbw <= 7.8E3)
    bw = 0;
  else if (sbw <= 10.4E3)
    bw = 1;
  else if (sbw <= 15.6E3)
    bw = 2;
  else if (sbw <= 20.8E3)
    bw = 3;
  else if (sbw <= 31.25E3)
    bw = 4;
  else if (sbw <= 41.7E3)
    bw = 5;
  else if (sbw <= 62.5E3)
    bw = 6;
  else if (sbw <= 125E3)
    bw = 7;
  else if (sbw <= 250E3)
    bw = 8;
  else
    bw = 9;
  lora_write_reg(REG_MODEM_CONFIG_1, (lora_read_reg(REG_MODEM_CONFIG_1) & 0x0f) | (bw << 4));
}

/**
 * Get Settings.
 * @param bw Bandwidth
 * @param cr coding rate
 * @param sf spreading factor
 */
void lora_get_settings(int *bw, int *cr, int *sf)
{
  *sf = (lora_read_reg(REG_MODEM_CONFIG_2) & 0xf0) >> 4;
  *bw = (lora_read_reg(REG_MODEM_CONFIG_1) & 0xf0) >> 4;
  *cr = (lora_read_reg(REG_MODEM_CONFIG_1) & 0x0f) >> 1;
}

/**
 * Set coding rate 
 * @param denominator 5-8, Denominator for the coding rate 4/x
 */
void lora_set_coding_rate(int denominator)
{
  if (denominator < 5)
    denominator = 5;
  else if (denominator > 8)
    denominator = 8;

  int cr = denominator - 4;
  lora_write_reg(REG_MODEM_CONFIG_1, (lora_read_reg(REG_MODEM_CONFIG_1) & 0xf1) | (cr << 1));
}

/**
 * Set the size of preamble.
 * @param length Preamble length in symbols.
 */
void lora_set_preamble_length(const long length)
{
  lora_write_reg(REG_PREAMBLE_MSB, (uint8_t)(length >> 8));
  lora_write_reg(REG_PREAMBLE_LSB, (uint8_t)(length >> 0));
}

/**
 * Change radio sync word.
 * @param sw New sync word to use.
 */
void lora_set_sync_word(const int sw)
{
  lora_write_reg(REG_SYNC_WORD, sw);
}

/**
 * Enable appending/verifying packet CRC.
 */
void lora_enable_crc(void)
{
  lora_write_reg(REG_MODEM_CONFIG_2, lora_read_reg(REG_MODEM_CONFIG_2) | 0x04);
}

/**
 * Disable appending/verifying packet CRC.
 */
void lora_disable_crc(void)
{
  lora_write_reg(REG_MODEM_CONFIG_2, lora_read_reg(REG_MODEM_CONFIG_2) & 0xfb);
}

/**
 *  Calibrate
 */
// fix me and test
void lora_calibrate(void)
{
  lora_write_reg(REG_OP_MODE, MODE_SLEEP);
  lora_write_reg(REG_PA_CONFIG, 0);

  // set frequency before calibrating

  lora_write_reg(REG_INVERT_IQ_2, (lora_read_reg(REG_INVERT_IQ_2) & RF_IMAGECAL_IMAGECAL_MASK) | RF_IMAGECAL_IMAGECAL_START);
  while ((lora_read_reg(REG_INVERT_IQ_2) & RF_IMAGECAL_IMAGECAL_RUNNING) == RF_IMAGECAL_IMAGECAL_RUNNING)
    ;
}

/**
 *  Disbale Invert IQ
 */
void lora_disable_invert_iq(void)
{
  lora_write_reg(REG_INVERT_IQ_1, lora_read_reg(REG_INVERT_IQ_1) & ~(1 << 6));
  // what is this for ?
  //lora_write_reg(REG_INVERT_IQ_2, 0x1D);
}

/**
 *  Enable Invert IQ
 */
void lora_enable_invert_iq(void)
{
  lora_write_reg(REG_INVERT_IQ_1, lora_read_reg(REG_INVERT_IQ_1) | (1 << 6));
  // what is this for?
  //lora_write_reg(REG_INVERT_IQ_2, 0x19);
}

/**
 * Perform hardware initialization.
 * @return 1 if version is 0x12
 */
int lora_init(void)
{
  esp_err_t ret;

  // Configure CPU hardware to communicate with the radio chip
  gpio_pad_select_gpio(config.gpio_rst);
  gpio_set_direction(config.gpio_rst, GPIO_MODE_OUTPUT);
  gpio_pad_select_gpio(config.gpio_cs);
  gpio_set_direction(config.gpio_cs, GPIO_MODE_OUTPUT);

  spi_bus_config_t bus = {
      .miso_io_num = config.gpio_miso,
      .mosi_io_num = config.gpio_mosi,
      .sclk_io_num = config.gpio_sck,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 0};

  ret = spi_bus_initialize(VSPI_HOST, &bus, 0);
  assert(ret == ESP_OK);

  spi_device_interface_config_t dev = {
      .clock_speed_hz = 9000000,
      .mode = 0,
      .spics_io_num = -1,
      .queue_size = 1,
      .flags = 0,
      .pre_cb = NULL};
  ret = spi_bus_add_device(VSPI_HOST, &dev, &__spi);
  assert(ret == ESP_OK);

  uint8_t version;

  // Perform hardware reset.
  lora_reset();

  // Check version.
  uint8_t i = 0;
  while (i++ < TIMEOUT_RESET)
  {
    version = lora_read_reg(REG_VERSION);
#ifdef LORA_DEBUG
    printf("lora version: %x\n", version);
#endif
    if (version == 0x12)
    {
      break;
    }
    vTaskDelay(2);
  }
  // at the end of the loop above, the max value i can reach is TIMEOUT_RESET + 1
  if (i > (TIMEOUT_RESET + 1))
  {
    return 0;
  }

  // Default configuration.
  lora_sleep();
  lora_write_reg(REG_FIFO_RX_BASE_ADDR, 0);
  lora_write_reg(REG_FIFO_TX_BASE_ADDR, 0);
  // set LNA boost
  lora_write_reg(REG_LNA, lora_read_reg(REG_LNA) | 0x03);
  // AGC auto
  lora_write_reg(REG_MODEM_CONFIG_3, 0x04);
  lora_set_tx_power(2);

  return version == 0x12;
}

/**
 * Send a packet.
 * @param buf Data to be sent
 * @param size Size of data.
 */
void lora_send_packet(uint8_t *buf, const int size)
{
  // Transfer data to radio.
  lora_idle();
  _modem_state = MODE_TX;
  lora_write_reg(REG_FIFO_ADDR_PTR, 0);

  for (int i = 0; i < size; i++)
    lora_write_reg(REG_FIFO, *buf++);

  lora_write_reg(REG_PAYLOAD_LENGTH, size);

  // Start transmission and wait for conclusion.
  lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_TX);
  while ((lora_read_reg(REG_IRQ_FLAGS) & IRQ_TX_DONE_MASK) == 0)
    vTaskDelay(2);

  lora_write_reg(REG_IRQ_FLAGS, IRQ_TX_DONE_MASK);
  _modem_state = MODE_STDBY;
}

/**
 * Send a packet with disableding recv irq first and ensure the modem is not receiving
 * @param buf Data to be sent
 * @param size Size of data.
 */
void lora_send_packet_irq(uint8_t *buf, const int size)
{
  while (_modem_state == MODE_RX_READ_BUF)
  {
#ifdef LORA_DEBUG
    printf("%s modem state == RX_READ_BUF\n", __func__);
#endif
    vTaskDelay(1);
  }
  _modem_state = MODE_TX;
  lora_enable_irq_recv(LORA_IRQ_DISABLE);
  lora_send_packet(buf, size);
  lora_enable_irq_recv(LORA_IRQ_ENABLE);
}

/**
 * Set FHSS hops
 * @param hops Number of hops (0 == disable hopping)
 */
void lora_fhss_sethops(const int hops)
{
  lora_write_reg(REG_HOP_PERIOD, hops);
#ifdef LORA_DEBUG
  printf("hops %d\n", lora_read_reg(REG_HOP_PERIOD));
#endif
}

/**
 * Handle hopping
 * This will change the channel if the IRQ indicates channel change
 * @param fhtable FHSS table
 * @param fhtable_size number of entries in the table
 * @return 1 if channel change irq was handled and 0 for anything else
 */
int lora_fhss_handle(const double *fhtable, const int fhtable_size)
{
  int irq = lora_read_reg(REG_IRQ_FLAGS);

#if 0
  if ((irq & IRQ_RX_TIMEOUT) != 0) {
#ifdef LORA_DEBUG
    printf("%s: rx to\n", __func__);
#endif
    lora_write_reg(REG_IRQ_FLAGS, IRQ_RX_TIMEOUT);
  }
  if ((irq & IRQ_VALID_HDR_MASK) != 0) {
#ifdef LORA_DEBUG
    printf("valid hdr\n");
#endif
    lora_write_reg(REG_IRQ_FLAGS, IRQ_VALID_HDR_MASK);
  }
  if ((irq & IRQ_CAD_DETECT_MASK) != 0) {
#ifdef LORA_DEBUG
    printf("CAD detect\n");
#endif
    lora_write_reg(REG_IRQ_FLAGS, IRQ_CAD_DETECT_MASK);
  }
  if ((irq & IRQ_PAYLOAD_CRC_ERROR_MASK) != 0) {
#ifdef LORA_DEBUG
    printf("CRC ERROR\n");
#endif
    lora_write_reg(REG_IRQ_FLAGS, IRQ_PAYLOAD_CRC_ERROR_MASK);
  }
  if ((irq & IRQ_CAD_DONE_MASK) != 0) {
#ifdef LORA_DEBUG
    printf("CAD done\n");
#endif
    lora_write_reg(REG_IRQ_FLAGS, IRQ_CAD_DONE_MASK);
  }
#endif

  // not change channel
  if ((irq & IRQ_FHSS_CHANGE_CHANNEL) == 0)
  {
#ifdef LORA_DEBUG
    printf("%s: not channel change\n", __func__);
#endif
    return 0;
  }

  int hop = lora_read_reg(REG_HOP_CHANNEL);
  hop = (hop & 0x3F) % fhtable_size;
  lora_set_frequency(fhtable[hop]);

  // ONLY clear channel change IRQ
  lora_write_reg(REG_IRQ_FLAGS, IRQ_FHSS_CHANGE_CHANNEL);

#ifdef LORA_DEBUG
  printf("%s: hop %d\n", __func__, hop);
#endif
  return 1;
}

/**
 * Read a received packet.
 * @param buf Buffer for the data.
 * @param size Available size in buffer (bytes).
 * @return Number of bytes received (zero if no packet available).
 */
int lora_receive_packet(uint8_t *buf, const int size)
{
  int len = 0;

  // Check interrupts
  int irq = lora_read_reg(REG_IRQ_FLAGS);
  lora_write_reg(REG_IRQ_FLAGS, irq);
  if ((irq & IRQ_RX_DONE_MASK) == 0)
    return 0;
  if (irq & IRQ_PAYLOAD_CRC_ERROR_MASK)
    return 0;

  _modem_state = MODE_RX_READ_BUF;

  // Find packet size.
  if (__implicit)
    len = lora_read_reg(REG_PAYLOAD_LENGTH);
  else
    len = lora_read_reg(REG_RX_NB_BYTES);

  // Transfer data from radio.
  lora_idle();
  _modem_state = MODE_RX_READ_BUF;
  lora_write_reg(REG_FIFO_ADDR_PTR, lora_read_reg(REG_FIFO_RX_CURRENT_ADDR));
  if (len > size)
    len = size;
  for (int i = 0; i < len; i++)
    *buf++ = lora_read_reg(REG_FIFO);

  _modem_state = MODE_STDBY;
  return len;
}

/** 
 * Install the RECV IRQ handler
 * call gpio_install_isr_service() before using this
 * @param isr_handler RECV IRQ handler
 * @returns result of gpio_isr_handler_add
 */
int lora_install_irq_recv(lora_isr_t isr_handler)
{
  gpio_pad_select_gpio(config.gpio_dio0);
  gpio_set_direction(config.gpio_dio0, GPIO_MODE_INPUT);
  gpio_set_intr_type(config.gpio_dio0, GPIO_INTR_POSEDGE);
  // start disabled
  gpio_intr_disable(config.gpio_dio0);

  lora_write_reg(REG_DIO_MAPPING_1, 0x00);

  return gpio_isr_handler_add(config.gpio_dio0, isr_handler, (void *)0);
}

/**
 * Uninstall RECV IRQ handler
 * @returns result of gpio_isr_handler_remove
 */
int lora_uninstall_irq_recv(void)
{
  gpio_intr_disable(config.gpio_dio0);
  return gpio_isr_handler_remove(config.gpio_dio0);
}

/**
 * enable / disable irq recv (e.g. during tx)
 * @param enable enable (1) or disable (0)
 * @return result of gpio_intr_disable
 */
int lora_enable_irq_recv(const int enable)
{
  if (enable == LORA_IRQ_ENABLE)
  {
    return gpio_intr_enable(config.gpio_dio0);
  }
  return gpio_intr_disable(config.gpio_dio0);
}

/**
 * Install FHSS IRQ handler
 * call gpio_install_isr_service() before using this
 * @param isr_handler FHSS irq handler
 * @returns result of gpio_isr_handler_add
 */
int lora_install_irq_fhss(lora_isr_t isr_handler)
{
  gpio_pad_select_gpio(config.gpio_dio2);
  gpio_set_direction(config.gpio_dio2, GPIO_MODE_INPUT);
  gpio_set_intr_type(config.gpio_dio2, GPIO_INTR_POSEDGE);
  // start disabled
  gpio_intr_disable(config.gpio_dio2);

  lora_write_reg(REG_DIO_MAPPING_1, 0x00);

  return gpio_isr_handler_add(config.gpio_dio2, isr_handler, (void *)2);
}

/**
 * Uninstall FHSS IRQ handler
 * @returns result of gpio_isr_handler_remove
 */
int lora_uninstall_irq_fhss(void)
{
  gpio_intr_disable(config.gpio_dio2);
  return gpio_isr_handler_remove(config.gpio_dio2);
}

/**
 * enable / disable irq FHSS IRQ
 * @param enable enable (1) or disable (0)
 * @return result of gpio_intr_disable
 */
int lora_enable_irq_fhss(const int enable)
{
  if (enable == LORA_IRQ_ENABLE)
  {
    return gpio_intr_enable(config.gpio_dio2);
  }
  return gpio_intr_disable(config.gpio_dio2);
}

/**
 * Returns non-zero if there is data to read (packet received).
 */
int lora_received(void)
{
  if (lora_read_reg(REG_IRQ_FLAGS) & IRQ_RX_DONE_MASK)
    return 1;
  return 0;
}

/**
 * Return last packet's RSSI.
 * @return rssi
 */
int lora_packet_rssi(void)
{
  return (lora_read_reg(REG_PKT_RSSI_VALUE) - (__frequency < 868E6 ? 164 : 157));
}

void lora_set_gain(uint8_t gain)
{
  // check allowed range
  if (gain > 6)
  {
    gain = 6;
  }

  // set to standby
  lora_idle();

  // set gain
  if (gain == 0)
  {
    // if gain = 0, enable AGC
    lora_write_reg(REG_MODEM_CONFIG_3, 0x04);
  }
  else
  {
    // disable AGC
    lora_write_reg(REG_MODEM_CONFIG_3, 0x00);

    // clear Gain and set LNA boost
    lora_write_reg(REG_LNA, 0x03);

    // set gain
    lora_write_reg(REG_LNA, lora_read_reg(REG_LNA) | (gain << 5));
  }
}

/**
 * Return last packet's SNR (signal to noise ratio).
 * @return snr
 */
float lora_packet_snr(void)
{
  return ((int8_t)lora_read_reg(REG_PKT_SNR_VALUE)) * 0.25;
}

/**
 * Dump registers
 */
void lora_dump_registers(void)
{
  int i;
  printf("00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n");
  for (i = 0; i < 0x40; i++)
  {
    printf("%02X ", lora_read_reg(i));
    if ((i & 0x0f) == 0x0f)
      printf("\n");
  }
  printf("\n");
}
