#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#define HPS_TO_FPGA_BASE_ADDR       0xc0010000 // TODO: this does not work properly ask Sebastian how to address in kernel/userspace
#define MEM_BUFFER_PHY_ADDR         0x3fff0000
#define MEM_BUFFER_LEN              0xffff
#define MEM_BUFFER_PKT_SRC_OFF      0x0

#define BPFCAP_FPGA_REG_SIZE        0x10 // TODO: how can we get it from the driver?
#define BPFCAP_DEV_FILE             "/dev/bpfcap_fpga"
#define MEM_FILE                    "/dev/mem"

#define PCAP_HEADER_SIZE            24 // in bytes

static volatile sig_atomic_t stop = 0;

static void fatal(const char* msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);
    fputc('\n', stderr);
}

static void sighandler(int sig)
{
    stop = 1;
}

void set_fpga_regs(uint32_t* fpga_regs, uint32_t ctrl, uint32_t pkt_start, uint32_t pkt_end, uint32_t write_addr)
{
    // pointer is 4 bytes
    *fpga_regs = ctrl;
    *(fpga_regs + 0x1) = pkt_start;
    *(fpga_regs + 0x2) = pkt_end;
    *(fpga_regs + 0x3) = write_addr;
}

void print_fpga_regs(uint32_t* fpga_regs)
{
    printf("\n\nPRINTING FPGA REGS:\n");
    printf("ctrl: %x, pkt_start: %x, pkt_end: %x, write_addr: %x",
            *fpga_regs, *(fpga_regs + 0x1), *(fpga_regs + 0x2), *(fpga_regs + 0x3));
}

void print_memory(char* mem, uint32_t start_addr, uint32_t end_addr)
{
    const uint32_t len = end_addr - start_addr; // TODO: add printing the addresses?
    const uint32_t offset = start_addr - MEM_BUFFER_PHY_ADDR;
    printf("len: %d - %x\n", len, len);
    for (uint32_t i = 0; i < len; ++i)
    {
        printf("%0x ", *(mem + offset + i));
        if (i > 1 && (i + 1) % 8 == 0)
        {
            printf("\n");
        }
    }
}

void write_pcap_header

//void print_help()
//{
//    print
//}

int main(int argc, char *agv[])
{
    int mem_fd = open(MEM_FILE, O_RDWR | O_SYNC);
    if (mem_fd < 0)
    {
        fatal("Cannot open the mem_fd device", errno);
        goto fail;
    }

    uint32_t* bpfcap_fpga_regs = mmap(NULL, BPFCAP_FPGA_REG_SIZE, PROT_WRITE | PROT_READ, MAP_SHARED, mem_fd, HPS_TO_FPGA_BASE_ADDR);
    if (bpfcap_fpga_regs == MAP_FAILED)
    {
        fatal("Cannot mmap the bpfcap_fpga_regs", errno);
        goto fail_unmap;
    }

    char* packet_dump_mem = mmap(NULL, MEM_BUFFER_LEN, PROT_WRITE | PROT_READ, MAP_SHARED, mem_fd, MEM_BUFFER_PHY_ADDR);
    if (packet_dump_mem == MAP_FAILED)
    {
        fatal("Cannot mmap the reserved_memory_range", errno);
        goto fail_unmap;
    }

    signal(SIGINT, sighandler);
    signal(SIGSTOP, sighandler);

    // this program needs:
    // input management (filename, buffer capture size, constant wraparound buffer run?)
    // open and dump the memory contents (byte by byte or memcpy?)
    // add pcap header to the dump
    // debug printing?
    // checking if fpga is still working?

    // for now just read whole memory to the buffer
    int pcap_fd = open("dump.pcap", O_RDWR | O_CREAT, PROT_WRITE | PROT_READ);
    if (pcap_fd < 0)
    {
        fatal("Cannot open the dump file", errno);
        goto fail_pcap;
    }

    // wait until FPGA is not BUSY anymore and is DONE
    uint32_t fpga_ctrl = (uint32_t)*bpfcap_fpga_regs;
    if (fpga_ctrl & (0x1 << 31))
    {
        fatal("FPGA still busy! Exiting");
        goto fail_pcap;
    }

    // get last write address and last len - this will be the end we need to dump
    uint32_t out_buffer_pos = *(bpfcap_fpga_regs + 0xC);
    uint32_t last_tx_len = *(bpfcap_fpga_regs + 0x8) - *(bpfcap_fpga_regs + 0x4);
    out_buffer_pos += last_tx_len;

    if (out_buffer_pos < MEM_BUFFER_PHY_ADDR || out_buffer_pos >=  0x3fffffff)
    {
        fatal("The output buffer either wrapped or is wrong, out_buffer_pos: ", out_buffer_pos);
        goto fail;
    }

    uint32_t filesize = out_buffer_pos - MEM_BUFFER_PHY_ADDR;
    char* pcap_buf = mmap(NULL, filesize + PCAP_HEADER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, pcap_fd, 0);
    if (pcap_buf == MAP_FAILED)
    {
        fatal("Canot map the dump buffer for writing", errno);
        goto fail_pcap;
    }
    write_pcap_header(pcap_buf);

    memcpy(pcap_buf, packet_dump_mem, filesize);
/*
    // randomize the memory in the buffer? so we can check continously
    memset(packet_dump_mem, 0xFF, MEM_BUFFER_LEN);
    // print_all_memory
    //printf("\n\nPRINTING ALL MEMORY \n\n");
    //print_memory(packet_dump_mem, MEM_BUFFER_PHY_ADDR, MEM_BUFFER_PHY_ADDR + MEM_BUFFER_LEN / 8);

    // clear registers of FPGA
    set_fpga_regs(bpfcap_fpga_regs, 0x0, 0x0, 0x0, 0x0);

    // read the register contents just to be sure
    print_fpga_regs(bpfcap_fpga_regs);

    // read from the 0th mem buffer
    printf("\n\nMEMORY BEFORE PKT WRITING addr: %x end: %x\n\n", MEM_BUFFER_PHY_ADDR,
            MEM_BUFFER_PHY_ADDR + TEMP_BUFFER_SIZE);
    print_memory(packet_dump_mem, MEM_BUFFER_PHY_ADDR,
            MEM_BUFFER_PHY_ADDR + TEMP_BUFFER_SIZE);
    // write first time to clear registers
    for (int i = 0; i < 0x1111 / 32; ++i) // TODO: add printing, probably wrong addressing here
    {
        *(packet_dump_mem + MEM_BUFFER_PKT_SRC_OFF + i) = i % 0xCC;
    }

    //printf("\n\nPRINTING ALL MEMORY AFTER PKT WRITING\n\n");
    //print_memory(packet_dump_mem, MEM_BUFFER_PHY_ADDR, MEM_BUFFER_PHY_ADDR + MEM_BUFFER_LEN / 8);

    printf("\n\nMEMORY AFTER PKT WRITING addr: %x end: %x\n\n", MEM_BUFFER_PHY_ADDR,
            MEM_BUFFER_PHY_ADDR + TEMP_BUFFER_SIZE);
    print_memory(packet_dump_mem, MEM_BUFFER_PHY_ADDR,
            MEM_BUFFER_PHY_ADDR + TEMP_BUFFER_SIZE);
    printf("\n\nMEMORY AFTER PKT WRITING FOR NEXT RANGE addr: %x end: %x\n\n", MEM_BUFFER_PHY_ADDR + MEM_BUFFER_PKT_DST_OFF(1),
            MEM_BUFFER_PHY_ADDR + MEM_BUFFER_PKT_DST_OFF(1) + TEMP_BUFFER_SIZE);
    print_memory(packet_dump_mem, MEM_BUFFER_PHY_ADDR + MEM_BUFFER_PKT_DST_OFF(1),
            MEM_BUFFER_PHY_ADDR + MEM_BUFFER_PKT_DST_OFF(1) + TEMP_BUFFER_SIZE);
    printf("\n\nMEMORY AFTER PKT WRITING FOR NEXT RANGE addr: %x end: %x\n\n", MEM_BUFFER_PHY_ADDR + MEM_BUFFER_PKT_DST_OFF(2),
            MEM_BUFFER_PHY_ADDR + MEM_BUFFER_PKT_DST_OFF(2) + TEMP_BUFFER_SIZE);
    print_memory(packet_dump_mem, MEM_BUFFER_PHY_ADDR + MEM_BUFFER_PKT_DST_OFF(2),
            MEM_BUFFER_PHY_ADDR + MEM_BUFFER_PKT_DST_OFF(2) + TEMP_BUFFER_SIZE);

    */

    munmap(bpfcap_fpga_regs, BPFCAP_FPGA_REG_SIZE);
    munmap(packet_dump_mem, MEM_BUFFER_LEN);
    munmap(pcap_buf, filesize + PCAP_HEADER_SIZE);
    close(mem_fd);
    close(pcap_fd);
    return 0;

fail_pcap:
    munmap(pcap_buf, filesize + PCAP_HEADER_SIZE);
    close(pcap_fd);
fail_unmap:
    munmap(bpfcap_fpga_regs, BPFCAP_FPGA_REG_SIZE);
    munmap(packet_dump_mem, MEM_BUFFER_LEN);
fail:
    close(mem_fd);
    exit(EXIT_FAILURE);
}
