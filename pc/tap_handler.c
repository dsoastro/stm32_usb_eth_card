/**************************************************************************
 * Based on https://backreference.org/2010/03/26/tuntap-interface-tutorial/
 **************************************************************************/



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <stdarg.h>
#include <termios.h>
#include <time.h>

#define BUFSIZE 2000


#define be16toword(a) ((((a)>>8)&0xff)|(((a)<<8)&0xff00))

uint16_t plength;
uint32_t PACKET_START_SIGN = 0xAABBCCDD;

/**************************************************************************
 * tun_alloc: allocates or reconnects to a tun/tap device. The caller     *
 *            must reserve enough space in *dev.                          *
 **************************************************************************/
int tun_alloc(char *dev, int flags) {

	struct ifreq ifr;
	int fd, err;
	char *clonedev = "/dev/net/tun";

	if( (fd = open(clonedev , O_RDWR)) < 0 ) {
		perror("Opening /dev/net/tun");
		return fd;
	}
	//printf("fd=%d\n",fd);
	memset(&ifr, 0, sizeof(ifr));

	ifr.ifr_flags = flags;

	if (*dev) {
		strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	}

	if( (err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0 ) {
		perror("ioctl(TUNSETIFF)");
		close(fd);
		return err;
	}

	strcpy(dev, ifr.ifr_name);

	return fd;
}

/**************************************************************************
 * cwrite: write routine that checks for errors and exits if an error is  *
 *         returned.                                                      *
 **************************************************************************/
int cwrite(int fd, char *buf, int n){

	int nwrite;

	if((nwrite=write(fd, buf, n)) < 0){
		perror("Writing data");
		exit(1);
	}
	return nwrite;
}
/**************************************************************************
 * cread: read routine that checks for errors and exits if an error is    *
 *        returned.                                                       *
 **************************************************************************/
int cread(int fd, char *buf, int n){

	int nread;

	if((nread=read(fd, buf, n)) < 0){
		perror("Reading data");
		exit(1);
	}
	return nread;
}
/**************************************************************************
 * read_n: ensures we read exactly n bytes, and puts them into "buf".     *
 *         (unless EOF, of course)                                        *
 **************************************************************************/
int read_n(int fd, char *buf, int n) {

	int nread, left = n;

	while(left > 0) {
		if ((nread = cread(fd, buf, left)) == 0){
			return 0 ;
		}else {
			left -= nread;
			buf += nread;
		}
	}
	return n;
}
void delay(int milliseconds)
{
	long pause;
	clock_t now,then;

	pause = milliseconds*(CLOCKS_PER_SEC/1000);
	now = then = clock();
	while( (now-then) < pause )
		now = clock();
}
void delay_micro(int microseconds)
{
	long pause;
	clock_t now,then;

	pause = microseconds*(CLOCKS_PER_SEC/1000000);
	now = then = clock();
	while( (now-then) < pause )
		now = clock();
}
char tun_name[IFNAMSIZ];
char buffer[BUFSIZE];

int main(){
	int maxfd;
	char cdc_name[20]="/dev/ttyACM0";

	strcpy(tun_name, "tap0");
	int tap_fd = tun_alloc(tun_name, IFF_TAP | IFF_NO_PI);  /* tap interface */

	if(tap_fd < 0){
		perror("Allocating interface");
		exit(1);
	}

	int tty_fd = open(cdc_name, O_RDWR | O_NOCTTY); //O_NOCTTY added


	struct termios portSettings;
	tcgetattr(tty_fd, &portSettings);
	cfmakeraw(&portSettings);
	tcsetattr(tty_fd, TCSANOW, &portSettings);
	tcflush(tty_fd, TCOFLUSH);

	if(tty_fd < 0){
		perror("Open tty");
		exit(1);
	}
	maxfd = (tap_fd > tty_fd)?tap_fd:tty_fd;
	int flag = 1; // first read
	int delay_m = 300;
	while(1) {
		int ret;
		fd_set rd_set;

		FD_ZERO(&rd_set);
		FD_SET(tap_fd, &rd_set);
		FD_SET(tty_fd, &rd_set);

		ret = select(maxfd + 1, &rd_set, NULL, NULL, NULL);

		if (ret < 0 && errno == EINTR){
			continue;
		}
		if (ret < 0) {
			perror("select()");
			exit(1);
		}
		if(FD_ISSET(tap_fd, &rd_set)) {

			uint16_t nread = cread(tap_fd, buffer, BUFSIZE);
			uint8_t buf[6];
			*(uint32_t *)buf = PACKET_START_SIGN;
			*(uint16_t *)(buf+4) = nread;
			cwrite(tty_fd,(char *)buf,6);
			cwrite(tty_fd, buffer, nread);
			delay_micro(delay_m);
		}
		if(FD_ISSET(tty_fd, &rd_set)) {
			uint32_t sign;
			int nread = read_n(tty_fd, (char *)&sign, sizeof(sign));

			if(nread == 0) {
				break;
			}
			if(sign != PACKET_START_SIGN){
				continue;
			}

			nread = read_n(tty_fd, (char *)&plength, 2);
			if(nread == 0) {
				break;
			}
			if (nread != 2){
				continue;
			}

			if(flag){
				flag = 0;
				nread = cread(tty_fd, buffer, sizeof(buffer));
				if(nread != 6){
					continue;
				}
			}

			if(plength > BUFSIZE){
				break;
			}

			nread = read_n(tty_fd, buffer, plength);
			if (nread != 0){
				cwrite(tap_fd, buffer, nread);
				delay_micro(delay_m);
			}
		}
	}

}
