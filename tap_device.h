/* This file was automatically generated.  Do not edit! */
int write_tap_dev(char *buffer,int len);
int read_tap_dev(char *buffer,int max_len,char *dst_mac,char *is_broadcast);
int setNonblocking(int fd);
int init_tap_device(void);
extern uint8_t ofdm0_mac[6];
extern const uint8_t mac_all_one[6];
extern const uint8_t mac_all_zero[6];
