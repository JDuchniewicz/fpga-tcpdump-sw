#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdint.h>

#define MEM_BUFFER_PHY_ADDR         0x3fff0000
#define MEM_BUFFER_LEN              0xffff
#define MEM_BUFFER_PKT_SRC_OFF      0x0
#define MEM_BUFFER_PKT_DST_OFF(i)  (0x1111 * (i))
// where we put the packet for reading? probably at the beginning, then we write with consecutive offsets that we can check in memory
// manually

#define BPFCAP_FPGA_REG_SIZE        0xC // TODO: how can we get it from the driver?
#define BPFCAP_DEV_FILE             "/dev/bpfcap_fpga"
#define MEM_FILE                    "/dev/mem"

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

void set_fpga_regs(char* fpga_regs, uint32_t ctrl, uint32_t pkt_start, uint32_t pkt_end, uint32_t write_addr)
{
    *fpga_regs = ctrl;
    *(fpga_regs + 0x4) = pkt_start;
    *(fpga_regs + 0x8) = pkt_end;
    *(fpga_regs + 0xC) = write_addr;
}

void print_memory(char* mem, uint32_t start_addr, uint32_t end_addr)
{
    const uint32_t len = end_addr - start_addr; // TODO: add printing the addresses?
    printf("len: %d - %x\n", len, len);
    for (uint32_t i = 0; i < len; ++i)
    {
        printf("%0x ", *(mem + i));
        if (i > 1 && (i - 1) % 8 == 0)
        {
            printf("\n");
        }
    }
}

int main(int argc, char *agv[])
{
    int bpfcap_fpga_fd = open(BPFCAP_DEV_FILE, O_RDWR | O_SYNC);
    if (bpfcap_fpga_fd < 0)
    {
        fatal("Cannot open the bpfcap_fpga_fd device", errno);
        goto fail;
    }

    int mem_fd = open(MEM_FILE, O_RDWR | O_SYNC);
    if (mem_fd < 0)
    {
        fatal("Cannot open the mem_fd device", errno);
        goto fail;
    }

    char* bpfcap_fpga_regs = mmap(NULL, BPFCAP_FPGA_REG_SIZE, PROT_WRITE | PROT_READ, MAP_SHARED, bpfcap_fpga_fd, 0);
    if (bpfcap_fpga_regs == MAP_FAILED)
    {
        fatal("Cannot mmap the bpfcap_fpga_regs", errno);
        goto fail;
    }

    char* packet_dump_mem = mmap(NULL, MEM_BUFFER_LEN, PROT_WRITE | PROT_READ, MAP_SHARED, mem_fd, MEM_BUFFER_PHY_ADDR);
    if (packet_dump_mem == MAP_FAILED)
    {
        fatal("Cannot mmap the reserved_memory_range", errno);
        goto fail;
    }

    signal(SIGINT, sighandler);
    signal(SIGSTOP, sighandler);

    // first open the device and mmap the memory for writing to registers
    // Every time we want to capture a packet we need to write to mmaped memory and submit the writing memory addr
    // probably even no need to wait (we will initally write just a single packet several times (maybe later allowing for loading
    // the packets from memory)

    // randomize the memory in the buffer? so we can check continously

    // clear registers of FPGA
    set_fpga_regs(bpfcap_fpga_regs, 0x0, 0x0, 0x0, 0x0);

    printf("\n\nMEMORY BEFORE PKT WRITING\n\n");
    print_memory(packet_dump_mem, MEM_BUFFER_PHY_ADDR, MEM_BUFFER_PHY_ADDR + MEM_BUFFER_PKT_DST_OFF(1));
    // write first time to clear registers
    for (int i = 0; i < 0x1111 / 2; ++i) // TODO: add printing, probably wrong addressing here
    {
        *(packet_dump_mem + MEM_BUFFER_PKT_SRC_OFF + i) = 0xCC;
    }

    printf("\n\nMEMORY AFTER PKT WRITING\n\n");
    print_memory(packet_dump_mem, MEM_BUFFER_PHY_ADDR, MEM_BUFFER_PHY_ADDR + MEM_BUFFER_PKT_DST_OFF(1) / 2);

    // loop 15 times
    for (int i = 1; i < 3; ++i) // TODO: how many packets process?
    {
        // set write addr
        printf("Looping i: %d!\n", i);
        printf("\n\nMEMORY BEFORE FPGA PROCESSING\n\n");
        print_memory(packet_dump_mem, MEM_BUFFER_PHY_ADDR + MEM_BUFFER_PKT_DST_OFF(i),
                MEM_BUFFER_PHY_ADDR + MEM_BUFFER_PKT_DST_OFF(i) + MEM_BUFFER_PKT_DST_OFF(1) / 2);

        if (i < 2) // ignore the last one
        {
            set_fpga_regs(bpfcap_fpga_regs, 0x0, MEM_BUFFER_PHY_ADDR + MEM_BUFFER_PKT_DST_OFF(i),
                    MEM_BUFFER_PHY_ADDR + MEM_BUFFER_PKT_DST_OFF(i) + MEM_BUFFER_PKT_DST_OFF(1) / 2,
                    MEM_BUFFER_PHY_ADDR + MEM_BUFFER_PKT_DST_OFF(i));
        }

        printf("\n\nMEMORY AFTER FPGA PROCESSING\n\n");
        print_memory(packet_dump_mem, MEM_BUFFER_PHY_ADDR + MEM_BUFFER_PKT_DST_OFF(i),
                MEM_BUFFER_PHY_ADDR + MEM_BUFFER_PKT_DST_OFF(i) + MEM_BUFFER_PKT_DST_OFF(1) / 2);
        sleep(10);
    }

    // print the mapped memory contents
    printf("\n\nDONE PROCESSING\n\n");

    munmap(bpfcap_fpga_regs, BPFCAP_FPGA_REG_SIZE);
    munmap(packet_dump_mem, MEM_BUFFER_LEN);
    close(bpfcap_fpga_fd);
    close(mem_fd);
    return 0;

fail_unmap:
    munmap(bpfcap_fpga_regs, BPFCAP_FPGA_REG_SIZE);
    munmap(packet_dump_mem, MEM_BUFFER_LEN);
fail:
    close(bpfcap_fpga_fd);
    close(mem_fd);
    exit(EXIT_FAILURE);
}
