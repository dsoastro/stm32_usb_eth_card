#include "net.h"
#include "enc28j60.h"

#define IP_ADDR {192,168,2,197}
uint8_t ipaddr[4]=IP_ADDR;

char str1[60]={0};
extern UART_HandleTypeDef huart1;
extern uint8_t macaddr[6];
void log(char* msg);
//--------------------------------------------------
void net_ini(void)
{
 enc28j60_ini();
}
//--------------------------------------------------

void eth_send(enc28j60_frame_ptr *frame, uint16_t len)
{
	memcpy(frame->addr_dest,frame->addr_src,6);
	memcpy(frame->addr_src,macaddr,6);
	enc28j60_packetSend((void*)frame,len+sizeof(enc28j60_frame_ptr));
}
void arp_send(enc28j60_frame_ptr *frame)
{
	arp_msg_ptr *msg=(void*)(frame->data);
	msg->op = ARP_REPLY;
	memcpy(msg->macaddr_dst,msg->macaddr_src,6);
	memcpy(msg->macaddr_src,macaddr,6);
	memcpy(msg->ipaddr_dst,msg->ipaddr_src,4);
	memcpy(msg->ipaddr_src,ipaddr,4);
	eth_send(frame,sizeof(arp_msg_ptr));
}
//-----------------------------------------------
//-----------------------------------------------
uint8_t arp_read(enc28j60_frame_ptr *frame, uint16_t len)
{
	uint8_t res=0;
	arp_msg_ptr *msg=(void*)(frame->data);
	if(len>=sizeof(arp_msg_ptr)) //my amendments >= instead of >
	{
		if((msg->net_tp==ARP_ETH)&&(msg->proto_tp==ARP_IP))
		{
			if((msg->op==ARP_REQUEST)) //&&(!memcmp(msg->ipaddr_dst,ipaddr,4))
			{
				sprintf(str1,"len=%d\r\n",len);
				log(&str1);
				sprintf(str1,"\r\nrequest\r\nmac_src %02X:%02X:%02X:%02X:%02X:%02X\r\n",
					msg->macaddr_src[0],msg->macaddr_src[1],msg->macaddr_src[2],
				  msg->macaddr_src[3],msg->macaddr_src[4],msg->macaddr_src[5]);
				HAL_UART_Transmit(&huart1,(uint8_t*)str1,strlen(str1),0x1000);
				sprintf(str1,"ip_src %d.%d.%d.%d\r\n",
					msg->ipaddr_src[0],msg->ipaddr_src[1],msg->ipaddr_src[2],msg->ipaddr_src[3]);
				HAL_UART_Transmit(&huart1,(uint8_t*)str1,strlen(str1),0x1000);
				sprintf(str1,"mac_dst %02X:%02X:%02X:%02X:%02X:%02X\r\n",
					msg->macaddr_dst[0],msg->macaddr_dst[1],msg->macaddr_dst[2],
				  msg->macaddr_dst[3],msg->macaddr_dst[4],msg->macaddr_dst[5]);
				HAL_UART_Transmit(&huart1,(uint8_t*)str1,strlen(str1),0x1000);
				sprintf(str1,"ip_dst %d.%d.%d.%d\r\n",
					msg->ipaddr_dst[0],msg->ipaddr_dst[1],msg->ipaddr_dst[2],msg->ipaddr_dst[3]);
				HAL_UART_Transmit(&huart1,(uint8_t*)str1,strlen(str1),0x1000);
				res=1;
			}
		}
	}
	return res;
}

void eth_read(enc28j60_frame_ptr *frame, uint16_t len)
{
	if(frame->type==ETH_ARP)
	{
		sprintf(str1,"%02X:%02X:%02X:%02X:%02X:%02X-%02X:%02X:%02X:%02X:%02X:%02X; %d; arprn\r\n",
				frame->addr_src[0],frame->addr_src[1],frame->addr_src[2],frame->addr_src[3],frame->addr_src[4],frame->addr_src[5],
				frame->addr_dest[0],frame->addr_dest[1],frame->addr_dest[2],frame->addr_dest[3],frame->addr_dest[4],frame->addr_dest[5],
				len);
		HAL_UART_Transmit(&huart1,(uint8_t*)str1,strlen(str1),0x1000);
		if(arp_read(frame,len-sizeof(enc28j60_frame_ptr))){
			//arp_send(frame);
		}
	}
	else if(frame->type==ETH_IP)
	{
		sprintf(str1,"%02X:%02X:%02X:%02X:%02X:%02X-%02X:%02X:%02X:%02X:%02X:%02X; %d; iprn\r\n",
				frame->addr_src[0],frame->addr_src[1],frame->addr_src[2],frame->addr_src[3],frame->addr_src[4],frame->addr_src[5],
				frame->addr_dest[0],frame->addr_dest[1],frame->addr_dest[2],frame->addr_dest[3],frame->addr_dest[4],frame->addr_dest[5],
				len);
		HAL_UART_Transmit(&huart1,(uint8_t*)str1,strlen(str1),0x1000);
	}
}




