/*
 * Improvements Copyright: Collin Mulliner
 *
 * based on: https://github.com/Inteform/esp32-lora-library
 * and since that is based https://github.com/sandeepmistry/arduino-LoRa
 * the license is: MIT
 */

#ifndef __LORA_H__
#define __LORA_H__

#define LORA_IRQ_ENABLE 1
#define LORA_IRQ_DISABLE 0

#define LORA_MSG_MAX_SIZE 255

typedef void (*lora_isr_t)(void *);

void lora_config_dio(const int gpio_dio0, const int gpio_dio1, const int gpio_dio2);
void lora_config(const int gpio_cs, const int gpio_rst, const int gpio_miso, const int gpio_mosi, const int gpio_sck);
int lora_init(void);
void lora_reset(void);

void lora_explicit_header_mode(void);
void lora_implicit_header_mode(int size);

void lora_idle(void);
void lora_sleep(void); 
void lora_receive(void);

void lora_set_tx_power(int level);
void lora_set_frequency(const double frequency);
void lora_set_spreading_factor(int sf);
void lora_set_bandwidth(long sbw);
void lora_set_coding_rate(int denominator);
void lora_set_preamble_length(long length);
void lora_set_sync_word(int sw);
void lora_enable_crc(void);
void lora_disable_crc(void);
void lora_send_packet(uint8_t *buf, const int size);
int lora_receive_packet(uint8_t *buf, const int size);
int lora_received(void);
int lora_packet_rssi(void);
float lora_packet_snr(void);
void lora_dump_registers(void);
void lora_get_settings(int *bw, int *cr, int *sf);
void lora_set_gain(uint8_t gain);

void lora_disable_invert_iq();
void lora_enable_invert_iq();
void lora_calibrate();

int lora_install_irq_recv(lora_isr_t isr_handler);
int lora_uninstall_irq_recv();
int lora_enable_irq_recv(int enable);
void lora_send_packet_irq(uint8_t *buf, int size);

int lora_install_irq_fhss(lora_isr_t isr_handler);
int lora_uninstall_irq_fhss();
int lora_enable_irq_fhss(int enable);
void lora_fhss_sethops(const int hops);
int lora_fhss_handle(const double *fhtable, const int fhtable_size);

#endif
