#ifndef __NET_H
#define __NET_H
//--------------------------------------------------
#include "stm32f1xx_hal.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "enc28j60.h"

typedef struct enc28j60_frame{
  uint8_t addr_dest[6];
  uint8_t addr_src[6];
  uint16_t type;
  uint8_t data[];
} enc28j60_frame_ptr;

typedef struct arp_msg{
	uint16_t  net_tp;
	uint16_t  proto_tp;
	uint8_t macaddr_len;
	uint8_t ipaddr_len;
	uint16_t  op;
	uint8_t macaddr_src[6];
	uint8_t ipaddr_src[4];
	uint8_t macaddr_dst[6];
	uint8_t ipaddr_dst[4];
} arp_msg_ptr;

#define be16toword(a) ((((a)>>8)&0xff)|(((a)<<8)&0xff00))
#define ETH_ARP be16toword(0x0806)
#define ETH_IP be16toword(0x0800)
//--------------------------------------------------
#define ARP_ETH	be16toword(0x0001)
#define ARP_IP	be16toword(0x0800)
#define ARP_REQUEST	be16toword(1)
#define ARP_REPLY	be16toword(2)
#define USB_BUFSIZE 8000
#define USB_POINTERS_ARRAY_SIZE 256
#define PACKET_START_SIGN 0xAABBCCDD

void net_poll(void);
void net_ini(void);
void eth_read(enc28j60_frame_ptr *frame, uint16_t len);

#endif /* __NET_H */
