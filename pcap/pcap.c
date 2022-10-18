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

#define BPFCAP_FPGA_REG_SIZE        0x1C // TODO: how can we get it from the driver?
#define BPFCAP_DEV_FILE             "/dev/bpfcap_fpga"
#define MEM_FILE                    "/dev/mem"

#define PCAP_HEADER_SIZE            24 // in bytes

static volatile sig_atomic_t stop = 0;

static char *magic_nr = "0xA1B23C4D";
char *NAME = "dump.pcap";
int SIZE = MEM_BUFFER_LEN;

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

void set_fpga_regs(uint32_t* fpga_regs, uint32_t ctrl, uint32_t pkt_start, uint32_t pkt_end,
                   uint32_t capt_buf_start, uint32_t capt_buf_size)
{
    // pointer is 4 bytes
    *fpga_regs = ctrl;
    *(fpga_regs + 0x1) = pkt_start;
    *(fpga_regs + 0x2) = pkt_end;
    *(fpga_regs + 0x3) = capt_buf_start;
    *(fpga_regs + 0x4) = capt_buf_size;
}

uint32_t get_fpga_reg(uint32_t fpga_regs, uint32_t reg_nr)
{
    if (reg_nr == 0)
        return fpga_regs;
    else if (reg_nr == 1)
        return (fpga_regs + 0x1);
    else if (reg_nr == 2)
        return (fpga_regs + 0x2);
    else if (reg_nr == 3)
        return (fpga_regs + 0x3);
    else if (reg_nr == 4)
        return (fpga_regs + 0x4);
    else if (reg_nr == 5)
        return (fpga_regs + 0x5);
    else
    {
        printf("Invalid register number %d, allowed values 0 - 5\n", reg_nr);
        return 0;
    }
}

void print_fpga_regs(uint32_t* fpga_regs)
{
    printf("\n\nPRINTING FPGA REGS:\n");
    printf("ctrl: %x, pkt_start: %x, pkt_end: %x, capt_buf_start: %x, capt_buf_size: %x, last_write_addr: %x",
            *fpga_regs, *(fpga_regs + 0x1), *(fpga_regs + 0x2), *(fpga_regs + 0x3),
            *(fpga_regs + 0x4), *(fpga_regs + 0x5));
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

void write_pcap_header(char* pcap_buf)
{
    // magic
    // memcpy(pcap_buf, magic_nr, 4); TODO: somehow this does not work
    *pcap_buf = 0x4D;
    pcap_buf += 1;
    *pcap_buf = 0x3C;
    pcap_buf += 1;
    *pcap_buf = 0xB2;
    pcap_buf += 1;
    *pcap_buf = 0xA1;
    pcap_buf += 1;
    printf("contents 0x%08x\n", *pcap_buf);
    //pcap_buf += 4;
    // major
    *pcap_buf = 2;
    printf("contents 0x%08x\n", *pcap_buf);
    pcap_buf += 2;
    // minor
    *pcap_buf = 4;
    printf("contents 0x%08x\n", *pcap_buf);
    pcap_buf += 2;
    // reserved x2
    memset(pcap_buf, 0, 8);
    printf("contents 0x%08x\n", *pcap_buf);
    pcap_buf += 8;
    // snaplen - 1500 currently used MTU?
    // TODO: fix snaplen
    //*pcap_buf = 0x05;
    *pcap_buf = 0xff;
    printf("contents 0x%08x\n", *pcap_buf);
    pcap_buf += 1;
    //*pcap_buf = 0xdc;
    *pcap_buf = 0xff; // TODO: fix it, somehow wrong also endianness of time etc is wrong, need to switched in the RTL
    printf("contents 0x%08x\n", *pcap_buf);
    pcap_buf += 3;
    // FCS + reserved
    // link type LINKTYPE_IPV4?
    *pcap_buf = 0xe4;
}

void parse_args(int argc, char** argv)
{
    int opt;

    while ((opt = getopt(argc, argv, "s:n:")) != -1)
    {
        switch (opt)
        {
            case 's':
                SIZE = strtol(optarg, NULL, 16);
                if (SIZE > MEM_BUFFER_LEN)
                {
                    printf("Size cannot be greater than %d\n", MEM_BUFFER_LEN);
                    SIZE = MEM_BUFFER_LEN;
                }
                break;

            case 'n':
                NAME = optarg;
                break;

            default:
                printf("Usage: %s [-s] [size] [-n] [pcap file name]", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char** argv)
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
    // input management (filename, buffer capture size, constant wraparound buffer run?) (this freerun buffer might be not possible)

    parse_args(argc, argv);

    // for now just read whole memory to the buffer
    int pcap_fd = open(NAME, O_RDWR | O_CREAT, PROT_WRITE | PROT_READ);
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

    // TODO: debug, seems to not work properly
    //if (out_buffer_pos < MEM_BUFFER_PHY_ADDR || out_buffer_pos >=  0x3fffffff)
    //{
    //    fatal("The output buffer either wrapped or is wrong, out_buffer_pos: 0x%08x", out_buffer_pos);
    //    goto fail;
    //}

    //uint32_t filesize = out_buffer_pos - MEM_BUFFER_PHY_ADDR;
    uint32_t filesize = 0x10000;
    char* pcap_buf = mmap(NULL, SIZE + PCAP_HEADER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, pcap_fd, 0);
    if (pcap_buf == MAP_FAILED)
    {
        fatal("Canot map the dump buffer for writing", errno);
        goto fail_pcap;
    }
    ftruncate(pcap_fd, SIZE + PCAP_HEADER_SIZE);
    printf("pcap_buf: 0x%08x\n", (uint32_t)pcap_buf);
    write_pcap_header(pcap_buf);
    pcap_buf += 24;
    printf("pcap_buf: 0x%08x\n", (uint32_t)pcap_buf);

    memcpy(pcap_buf, packet_dump_mem, SIZE);

    munmap(bpfcap_fpga_regs, BPFCAP_FPGA_REG_SIZE);
    munmap(packet_dump_mem, MEM_BUFFER_LEN);
    munmap(pcap_buf, SIZE + PCAP_HEADER_SIZE);
    close(mem_fd);
    close(pcap_fd);
    return 0;

fail_pcap:
    munmap(pcap_buf, SIZE + PCAP_HEADER_SIZE);
    close(pcap_fd);
fail_unmap:
    munmap(bpfcap_fpga_regs, BPFCAP_FPGA_REG_SIZE);
    munmap(packet_dump_mem, MEM_BUFFER_LEN);
fail:
    close(mem_fd);
    exit(EXIT_FAILURE);
}
