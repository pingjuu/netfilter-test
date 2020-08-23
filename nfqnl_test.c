#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <linux/types.h>
#include <linux/netfilter.h>		/* for NF_ACCEPT */
#include <errno.h>
#include <string.h>
#include <stdbool.h>
//#include <netinet/ip.h>
//#include <netinet/tcp.h>

#include "headers.h"
#include <libnetfilter_queue/libnetfilter_queue.h>
void dump(unsigned char* buf, int size) {
	int i;
	for (i = 0; i < size; i++) {
		if (i % 16 == 0)
			printf("\n");
		printf("%02x ", buf[i]);
	}
}
int ipv4(const u_char *temp)
{
    struct libnet_ipv4_hdr *ip_packet = (struct libnet_ipv4_hdr*)temp;
    //ip_packet->ip_src = inet_ntoa(ip_packet->ip_src);
    if (ip_packet->ip_v == 4){
		//printf("this is ip packet v4\n");
		//printf("ip protocol %x\n",ip_packet->ip_p);
	
	    if(ip_packet->ip_p == 0x06){
    	    //printf("lenght: %x\n",ip_packet->ip_hl );
        	return ip_packet->ip_hl<<2;
    	}else return 0;
	}else return 0;
}

int hostcheck(char* http_packet)
{
	char *ptr = strtok(http_packet,"\n");
	bool exitwhile = false;
	while(ptr != NULL)
	{
		printf("%s\n",ptr);
		char *ptr2 = strtok(ptr,":");
		while(ptr2 != NULL){
			if(strcmp(ptr2,"test.gilgil.net")==0){
				printf("%s",ptr2);
				exitwhile =true;
				return -1;
			}
		}
		if(exitwhile==true) break;
		ptr = strtok(NULL, "\n");
	}
	return 0;
}




/* returns packet id */
static u_int32_t print_pkt (struct nfq_data *tb, int *verdict_id)
{
	int id = 0;
	struct nfqnl_msg_packet_hdr *ph;
	struct nfqnl_msg_packet_hw *hwph;
	u_int32_t mark,ifi;
	int ret;
	unsigned char *data;

	ph = nfq_get_msg_packet_hdr(tb);
	if (ph) {
		id = ntohl(ph->packet_id);
		printf("hw_protocol=0x%04x hook=%u id=%u ",
			ntohs(ph->hw_protocol), ph->hook, id);
	}

	hwph = nfq_get_packet_hw(tb);
	if (hwph) {
		int i, hlen = ntohs(hwph->hw_addrlen);

		printf("hw_src_addr=");
		for (i = 0; i < hlen-1; i++)
			printf("%02x:", hwph->hw_addr[i]);
		printf("%02x ", hwph->hw_addr[hlen-1]);
	}

	mark = nfq_get_nfmark(tb);
	if (mark)
		printf("mark=%u ", mark);

	ifi = nfq_get_indev(tb);
	if (ifi)
		printf("indev=%u ", ifi);

	ifi = nfq_get_outdev(tb);
	if (ifi)
		printf("outdev=%u ", ifi);
	ifi = nfq_get_physindev(tb);
	if (ifi)
		printf("physindev=%u ", ifi);

	ifi = nfq_get_physoutdev(tb);
	if (ifi)
		printf("physoutdev=%u ", ifi);

	ret = nfq_get_payload(tb, &data);
	if (ret >= 0)
	{
		
		//dump(data, ret);
		//**code make**
		int next = ipv4(data);
		if(next == 20){
			//struct libnet_ipv4_hdr *ip_packet = (struct libnet_ipv4_hdr*)data;
			//printf("\nip packet ok\n but version:? : %x\n",ip_packet->ip_v);
					//printf("ip length :%d\n",ip_offset);
					//ip_packet = ip_packet+ip_offset;
			struct libnet_tcp_hdr *tcp_packet =(struct libnet_tcp_hdr *)(data+next);

			if(ntohs(tcp_packet->th_dport) == 80){
				printf("payload_len=%d ", ret);
				printf("tcp dest port : %d\n", ntohs(tcp_packet->th_dport));
				int headerlen=tcp_packet->th_off<<2;
				printf("headerlen %d\n", headerlen);
				u_char *http_packet = (u_char *)(tcp_packet)+(tcp_packet->th_off<<2);
						//http_packet +=10;
				printf("****dump****\n");
				dump(data, ret);
				printf("\nhttp packet :\n");
				printf("%s",http_packet);
				printf("\n------------------------------------------\n");
				//int http_packetlen = sizeof(http_packet);
				//for(int i=0; i<http_packetlen; i++){
				//	printf("%c",http_packet[i]);
				//}

				if (hostcheck(http_packet)==-1);
					*verdict_id=-1;
				printf("\n");
			}
		}
	}

	fputc('\n', stdout);

	return id;
}


static int cb(struct nfq_q_handle *qh, struct nfgenmsg *nfmsg,
	      struct nfq_data *nfa, void *data)
{
	int verdict_id=0;
	u_int32_t id = print_pkt(nfa,&verdict_id);
	printf("entering callback\n");
	if(verdict_id == -1){
		return nfq_set_verdict(qh, id, NF_DROP, 0, NULL);
		}
	else 
		return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
}

int main(int argc, char **argv)
{
	struct nfq_handle *h;
	struct nfq_q_handle *qh;
	struct nfnl_handle *nh;
	int fd;
	int rv;
	char buf[4096] __attribute__ ((aligned));

	printf("opening library handle\n");
	h = nfq_open();
	if (!h) {
		fprintf(stderr, "error during nfq_open()\n");
		exit(1);
	}

	printf("unbinding existing nf_queue handler for AF_INET (if any)\n");
	if (nfq_unbind_pf(h, AF_INET) < 0) {
		fprintf(stderr, "error during nfq_unbind_pf()\n");
		exit(1);
	}

	printf("binding nfnetlink_queue as nf_queue handler for AF_INET\n");
	if (nfq_bind_pf(h, AF_INET) < 0) {
		fprintf(stderr, "error during nfq_bind_pf()\n");
		exit(1);
	}

	printf("binding this socket to queue '0'\n");
	qh = nfq_create_queue(h,  0, &cb, NULL);
	if (!qh) {
		fprintf(stderr, "error during nfq_create_queue()\n");
		exit(1);
	}

	printf("setting copy_packet mode\n");
	if (nfq_set_mode(qh, NFQNL_COPY_PACKET, 0xffff) < 0) {
		fprintf(stderr, "can't set packet_copy mode\n");
		exit(1);
	}

	fd = nfq_fd(h);

	for (;;) {
		if ((rv = recv(fd, buf, sizeof(buf), 0)) >= 0) {
			printf("pkt received\n");
			nfq_handle_packet(h, buf, rv);
			continue;
		}
		/* if your application is too slow to digest the packets that
		 * are sent from kernel-space, the socket buffer that we use
		 * to enqueue packets may fill up returning ENOBUFS. Depending
		 * on your application, this error may be ignored. nfq_nlmsg_verdict_putPlease, see
		 * the doxygen documentation of this library on how to improve
		 * this situation.
		 */
		if (rv < 0 && errno == ENOBUFS) {
			printf("losing packets!\n");
			continue;
		}
		perror("recv failed");
		break;
	}

	printf("unbinding from queue 0\n");
	nfq_destroy_queue(qh);

#ifdef INSANE
	/* normally, applications SHOULD NOT issue this command, since
	 * it detaches other programs/sockets from AF_INET, too ! */
	printf("unbinding from AF_INET\n");
	nfq_unbind_pf(h, AF_INET);
#endif

	printf("closing library handle\n");
	nfq_close(h);

	exit(0);

}
