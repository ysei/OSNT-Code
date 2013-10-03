#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <signal.h>
#include <stdlib.h>

#define PAGE_SIZE 4096

#define NF10_IOCTL_CMD_READ_STAT (SIOCDEVPRIVATE+0)
#define NF10_IOCTL_CMD_WRITE_REG (SIOCDEVPRIVATE+1)
#define NF10_IOCTL_CMD_READ_REG (SIOCDEVPRIVATE+2)
#define NF10_IOCTL_CMD_RESET_DMA (SIOCDEVPRIVATE+3)
#define NF10_IOCTL_CMD_SET_RX_DNE_HEAD (SIOCDEVPRIVATE+4)
#define NF10_IOCTL_CMD_SET_RX_BUFF_HEAD (SIOCDEVPRIVATE+5)
#define NF10_IOCTL_CMD_SET_RX_PKT_HEAD (SIOCDEVPRIVATE+6)
#define NF10_IOCTL_CMD_START_DMA (SIOCDEVPRIVATE+7)
#define NF10_IOCTL_CMD_STOP_DMA (SIOCDEVPRIVATE+8)

int cmd_file;
int rx_dne_file;
int rx_buff_file;
FILE *pcap;

uint64_t rx_dne_head = 0;
uint64_t rx_pkt_head = 0;
uint64_t rx_buff_head = 0;

char *rx_dne = NULL;
char *rx_buff = NULL;

void sig_handler(int signo)
{
    uint64_t v;

    if (signo == SIGINT){

        ioctl(cmd_file, NF10_IOCTL_CMD_STOP_DMA, v);
        close(cmd_file);
        close(rx_dne_file);
        close(rx_buff_file);
        fclose(pcap);

        exit(0);
    }
}

int push_section_header_block()
{
    uint32_t block_type = 0x0a0d0d0a;
    uint32_t block_len = 28;
    uint32_t magic = 0x1a2b3c4d;
    uint16_t major = 1;
    uint16_t minor = 0;
    uint64_t section_len = 0xffffffffffffffffULL;

    if(pcap == NULL)
    {
        perror("pcap file not open");
        return -1;
    }

    if(fwrite(&block_type, 4, 1, pcap) != 1)
    {
        perror("pcap write error");
        return -1;
    }

    if(fwrite(&block_len, 4, 1, pcap) != 1)
    {
        perror("pcap write error");
        return -1;
    }

    if(fwrite(&magic, 4, 1, pcap) != 1)
    {
        perror("pcap write error");
        return -1;
    }

    if(fwrite(&major, 2, 1, pcap) != 1)
    {
        perror("pcap write error");
        return -1;
    }

    if(fwrite(&minor, 2, 1, pcap) != 1)
    {
        perror("pcap write error");
        return -1;
    }

    if(fwrite(&section_len, 8, 1, pcap) != 1)
    {
        perror("pcap write error");
        return -1;
    }

    if(fwrite(&block_len, 4, 1, pcap) != 1)
    {
        perror("pcap write error");
        return -1;
    }

    return 0;
}

int push_interface_description_block(char *name)
{
    uint32_t block_type = 1;
    uint32_t block_len;
    uint16_t link_type = 0;
    uint16_t reserved = 0;
    uint32_t snap_len = 65535;
    uint16_t opt_code_name = 2;
    uint16_t opt_len_name;
    uint16_t opt_code_tsresol = 9;
    uint16_t opt_len_tsresol = 1;
    uint32_t opt_tsresol = 9;
    uint16_t opt_code_end = 0;
    uint16_t opt_len_end = 0;

    uint32_t padding_len;
    uint32_t padding = 0;

    opt_len_name = strlen(name) + 1;
    block_len = 36 + ((opt_len_name - 1)/4 + 1)*4;
    padding_len = ((opt_len_name - 1)/4 + 1)*4 - opt_len_name;

    if(pcap == NULL)
    {
        perror("pcap file not open");
        return -1;
    }

    if(fwrite(&block_type, 4, 1, pcap) != 1)
    {
        perror("pcap write error");
        return -1;
    }

    if(fwrite(&block_len, 4, 1, pcap) != 1)
    {
        perror("pcap write error");
        return -1;
    }

    if(fwrite(&link_type, 2, 1, pcap) != 1)
    {
        perror("pcap write error");
        return -1;
    }

    if(fwrite(&reserved, 2, 1, pcap) != 1)
    {
        perror("pcap write error");
        return -1;
    }

    if(fwrite(&snap_len, 4, 1, pcap) != 1)
    {
        perror("pcap write error");
        return -1;
    }

    if(fwrite(&opt_code_name, 2, 1, pcap) != 1)
    {
        perror("pcap write error");
        return -1;
    }

    if(fwrite(&opt_len_name, 2, 1, pcap) != 1)
    {
        perror("pcap write error");
        return -1;
    }

    if(fwrite(name, opt_len_name, 1, pcap) != 1)
    {
        perror("pcap write error");
        return -1;
    }

    if(padding_len != 0)
    {
        if(fwrite(&padding, padding_len, 1, pcap) != 1)
        {
            perror("pcap write error");
            return -1;
        }
    }

    if(fwrite(&opt_code_tsresol, 2, 1, pcap) != 1)
    {
        perror("pcap write error");
        return -1;
    }

    if(fwrite(&opt_len_tsresol, 2, 1, pcap) != 1)
    {
        perror("pcap write error");
        return -1;
    }

    if(fwrite(&opt_tsresol, 4, 1, pcap) != 1)
    {
        perror("pcap write error");
        return -1;
    }

    if(fwrite(&opt_code_end, 2, 1, pcap) != 1)
    {
        perror("pcap write error");
        return -1;
    }

    if(fwrite(&opt_len_end, 2, 1, pcap) != 1)
    {
        perror("pcap write error");
        return -1;
    }

    if(fwrite(&block_len, 4, 1, pcap) != 1)
    {
        perror("pcap write error");
        return -1;
    }

    return 0;
}

int push_enhanced_packet_block(uint64_t port_encoded, uint64_t len, uint64_t timestamp)
{
    uint32_t block_type = 6;
    uint32_t block_len;
    uint32_t interface_id;
    uint32_t timestamp_high;
    uint32_t timestamp_low;
    uint32_t captured_len = (uint32_t)len;
    uint32_t packet_len = (uint32_t)len;
    uint32_t padding_len = ((len-1)/4 + 1)*4 - len;
    uint64_t padding = 0;

    block_len = 32 + ((len-1)/4 + 1)*4;

    if(port_encoded & 0x0200)
        interface_id = 0;
    else if(port_encoded & 0x0800)
        interface_id = 1;
    else if(port_encoded & 0x2000)
        interface_id = 2;
    else if(port_encoded & 0x8000)
        interface_id = 3;
    else 
        interface_id = 4;

    //timestamp = ((timestamp>>32)&0xffffffff)*1000000000 + (((timestamp&0xffffffff)*1000000000)>>32);
    timestamp = timestamp*25/4;
    timestamp_high = (uint32_t)((timestamp>>32)&0xffffffff);
    timestamp_low = (uint32_t)(timestamp&0xffffffff);

    if(pcap == NULL)
    {
        perror("pcap file not open");
        return -1;
    }

    if(fwrite(&block_type, 4, 1, pcap) != 1)
    {
        perror("pcap write error");
        return -1;
    }

    if(fwrite(&block_len, 4, 1, pcap) != 1)
    {
        perror("pcap write error");
        return -1;
    }

    if(fwrite(&interface_id, 4, 1, pcap) != 1)
    {
        perror("pcap write error");
        return -1;
    }

    if(fwrite(&timestamp_high, 4, 1, pcap) != 1)
    {
        perror("pcap write error");
        return -1;
    }

    if(fwrite(&timestamp_low, 4, 1, pcap) != 1)
    {
        perror("pcap write error");
        return -1;
    }

    if(fwrite(&captured_len, 4, 1, pcap) != 1)
    {
        perror("pcap write error");
        return -1;
    }

    if(fwrite(&packet_len, 4, 1, pcap) != 1)
    {
        perror("pcap write error");
        return -1;
    }

    if(fwrite(rx_buff+rx_buff_head, len, 1, pcap) != 1)
    {
        perror("pcap write error");
        return -1;
    }

    if(padding_len != 0)
    {
        if(fwrite(&padding, padding_len, 1, pcap) != 1)
        {
            perror("pcap write error");
            return -1;
        }
    }

    if(fwrite(&block_len, 4, 1, pcap) != 1)
    {
        perror("pcap write error");
        return -1;
    }

    return 0;
}

int main ( int argc, char **argv )
{

    uint64_t v;
    uint64_t rx_int;
    uint64_t addr;
    uint64_t len;
    uint64_t port_encoded;
    uint64_t timestamp;

    int pkt_count = 0;

    uint64_t rx_dne_mask = 0x00000fffULL;
    uint64_t rx_pkt_mask = 0x0000ffffULL;
    uint64_t rx_buff_mask = 0xffffULL;

    signal(SIGINT, sig_handler);

    cmd_file = open("/dev/nf10", O_RDWR);
    if(cmd_file < 0){
        perror("/dev/nf10");
        goto error_out;
    }

    rx_dne_file = open("/sys/kernel/debug/nf10_rx_dne_mmap", O_RDWR);
    if(rx_dne_file < 0) {
        perror("nf10_rx_dne_mmap");
        goto error_out;
    }

    rx_buff_file = open("/sys/kernel/debug/nf10_rx_buff_mmap", O_RDWR);
    if(rx_buff_file < 0) {
        perror("nf10_rx_buff_mmap");
        goto error_out;
    }

    pcap = fopen("packets.pcapng", "wb");
    if(pcap == NULL) {
        perror("cannot open packets.pcapng");
        goto error_out;
    }

    rx_dne = mmap(NULL, rx_dne_mask+1, PROT_READ|PROT_WRITE, MAP_SHARED, rx_dne_file, 0);
    if (rx_dne == MAP_FAILED) {
        perror("mmap rx_dne error");
        goto error_out;
    }

    rx_buff = mmap(NULL, rx_buff_mask+1+PAGE_SIZE, PROT_READ, MAP_SHARED, rx_buff_file, 0);
    if (rx_buff == MAP_FAILED) {
        perror("mmap rx_buff error");
        goto error_out;
    }

    if(ioctl(cmd_file, NF10_IOCTL_CMD_RESET_DMA, v) < 0){
        perror("nf10 reset dma failed");
        goto error_out;
    }

    if(ioctl(cmd_file, NF10_IOCTL_CMD_START_DMA, v) < 0){
        perror("nf10 start dma failed");
        goto error_out;
    }

    if(push_section_header_block() < 0){
        goto error_out;
    }

    if(push_interface_description_block("nf0") < 0){
        goto error_out;
    }

    if(push_interface_description_block("nf1") < 0){
        goto error_out;
    }

    if(push_interface_description_block("nf2") < 0){
        goto error_out;
    }

    if(push_interface_description_block("nf3") < 0){
        goto error_out;
    }

    if(push_interface_description_block("unknown") < 0){
        goto error_out;
    }

    while(1){
        rx_int = *(((uint64_t*)rx_dne) + rx_dne_head/8);
        if( ((rx_int >> 48) & 0xffff) != 0xffff ){

            pkt_count++;
            printf("%d packets received\n", pkt_count);

            timestamp = *(((uint64_t*)rx_dne) + (rx_dne_head)/8 + 1);
            len = rx_int & 0xffff;
            port_encoded = (rx_int >> 16) & 0xffff;

            *(((uint64_t*)rx_dne) + rx_dne_head/8) = 0xffffffffffffffffULL;

            if(push_enhanced_packet_block(port_encoded, len, timestamp)<0)
            {
                goto error_out;
            }

            rx_dne_head = ((rx_dne_head + 64) & rx_dne_mask);
            rx_buff_head = ((rx_buff_head + ((len-1)/64 + 1)*64) & rx_buff_mask);
            rx_pkt_head = ((rx_pkt_head + ((len-1)/64 + 1)*64) & rx_pkt_mask);

            if(ioctl(cmd_file, NF10_IOCTL_CMD_SET_RX_DNE_HEAD, rx_dne_head) < 0){
                perror("nf10 set rx dne head failed");
                goto error_out;
            }

            if(ioctl(cmd_file, NF10_IOCTL_CMD_SET_RX_BUFF_HEAD, rx_buff_head) < 0){
                perror("nf10 set rx buff head failed");
                goto error_out;
            }

            if(ioctl(cmd_file, NF10_IOCTL_CMD_SET_RX_PKT_HEAD, rx_pkt_head) < 0){
                perror("nf10 set rx pkt head failed");
                goto error_out;
            }
            
        }

    }

error_out:
    ioctl(cmd_file, NF10_IOCTL_CMD_STOP_DMA, v);
    close(cmd_file);
    close(rx_dne_file);
    close(rx_buff_file);
    fclose(pcap);	
    return -1;
}
