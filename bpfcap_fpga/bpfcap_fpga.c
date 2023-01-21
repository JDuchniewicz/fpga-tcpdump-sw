#include "bpfcap_fpga.h"


struct bpfcap_fpga_dev {
    struct miscdevice miscdev;
    void __iomem *regs;

    u32     write_value;
    u32     read_value;
};

static struct of_device_id bpfcap_fpga_dt_ids[] = {
    {
        .compatible = "dev,bpfcap_fpga"
    },
    { /* end of table */ }
};

MODULE_DEVICE_TABLE(of, bpfcap_fpga_dt_ids);

static struct platform_driver bpfcap_fpga_platform = {
    .probe = bpfcap_fpga_probe,
    .remove = bpfcap_fpga_remove,
    .driver = {
        .name = "tcpdump on FPGA with BPF Driver",
        .owner = THIS_MODULE,
        .of_match_table = bpfcap_fpga_dt_ids
    }
};

static struct file_operations bpfcap_fpga_fops = {
    .owner = THIS_MODULE,
    .mmap  = bpfcap_fpga_mmap,
    .write = bpfcap_fpga_write,
    .read  = bpfcap_fpga_read,
}; // TODO: add the page_fault method?

static int bpfcap_fpga_probe(struct platform_device *pdev)
{
    int ret_val = -EBUSY;
    struct bpfcap_fpga_dev *dev;
    struct resource *r = NULL;

    pr_info("bpfcap_fpga_probe enter \n");

    r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (r == NULL) {
        pr_err("IORESOURCE_MEM (register space) does not exist!\n");
        goto bad_exit_return;
    }

    dev = devm_kzalloc(&pdev->dev, sizeof(struct bpfcap_fpga_dev), GFP_KERNEL);
    bpfcap_fpga_device = dev;

    dev->regs = devm_ioremap_resource(&pdev->dev, r);
    if (IS_ERR(dev->regs))
        goto bad_ioremap;

    dev->write_value = 0x0;
    iowrite32(dev->write_value, dev->regs + BPFCAP_FPGA_CTRL);
    //dev->write_value = 0xdeadbeef;
    iowrite32(dev->write_value, dev->regs + BPFCAP_FPGA_PKT_BEGIN);
    //dev->write_value = 0x0000ffff;
    iowrite32(dev->write_value, dev->regs + BPFCAP_FPGA_PKT_END);
    iowrite32(dev->write_value, dev->regs + BPFCAP_FPGA_WRITE_ADDR);

    dev->miscdev.minor = MISC_DYNAMIC_MINOR;
    dev->miscdev.name = "bpfcap_fpga";
    dev->miscdev.fops = &bpfcap_fpga_fops;

    ret_val = misc_register(&dev->miscdev);
    if (ret_val != 0) {
        pr_info("Could not register misc device\n");
        goto bad_exit_return;
    }

    platform_set_drvdata(pdev, (void*)dev);

    pr_info("bpfcap_fpga_probe exit\n");

    return 0;
bad_ioremap:
    ret_val = PTR_ERR(dev->regs);
bad_exit_return:
    pr_info("bpfcap_fpga_probe bad exit\n");
    return ret_val;
}

static int bpfcap_fpga_remove(struct platform_device *pdev)
{
    struct bpfcap_fpga_dev *dev = (struct bpfcap_fpga_dev*)platform_get_drvdata(pdev);
    pr_info("bpfcap_fpga_remove enter\n");

    // write stop to the capture program? TODO:
    bpfcap_fpga_device = NULL;

    misc_deregister(&dev->miscdev);
    pr_info("bpfcap_fpga_remove exit\n");
    return 0;
}

static ssize_t bpfcap_fpga_write(struct file *file, const char *buffer, size_t len, loff_t *offset)
{
    int success = 0;
    struct bpfcap_fpga_dev *dev = container_of(file->private_data, struct bpfcap_fpga_dev, miscdev);

    pr_info("Writing to the device\n");

    success = copy_from_user(&dev->write_value, buffer, sizeof(dev->write_value));
    if (success != 0) {
        pr_info("Failed to copy the register data\n");
    }

    return len;
}

static ssize_t bpfcap_fpga_read(struct file *file, char *buffer, size_t len, loff_t *offset)
{
    return 1;
}

// will it expose proper registers? probably yes, avalon will expose the addressing range TODO:
static int bpfcap_fpga_mmap(struct file *file, struct vm_area_struct *vma)
{
    unsigned long off = vma->vm_pgoff << PAGE_SHIFT;
    unsigned long physical = BPFCAP_FPGA_REG_START + off; // TODO: should it be remapped using this address or the bridge one?
    unsigned long vsize = vma->vm_end - vma->vm_start;
    unsigned long psize = 0x10; // FIXME:
    pr_info("bpfcap_fpga_mmap off: %lx, physical: %lx, vsize: %lx, psize: %lx\n",
            off, physical, vsize, psize);

    //if (vsize > psize)
    //    return -EINVAL; // too high

    if (remap_pfn_range(vma, vma->vm_start, physical, psize, vma->vm_page_prot))
        return -EAGAIN;

    pr_info("bpfcap_fpga_mmap exit\n");
    return 0;
}

static int bpfcap_fpga_init(void)
{
    int ret_val = 0;
    pr_info("Initializing the FPGA accelerated tcpdump driver module.\n");

    ret_val = platform_driver_register(&bpfcap_fpga_platform);
    if (ret_val != 0) {
        pr_err("platform_driver_register returned %d\n", ret_val);
        return ret_val;
    }

    pr_info("FPGA accelerated tcpdump driver module properly initialized!\n");
    return 0;
}

static void bpfcap_fpga_exit(void)
{
    pr_info("FPGA accelerated tcpdump driver module exit.\n");

    platform_driver_unregister(&bpfcap_fpga_platform);

    pr_info("FPGA accelerated tcpdump driver module successfully unregistered.\n");
}

int bpfcap_fpga_write_regs(u32 ctrl, u32 pkt_begin, u32 pkt_end, u32 write_addr)
{
    if (!bpfcap_fpga_device)
    {
        pr_err("Please call insmod bpfcap_fpga.ko first.\n");
        return 1;
    }

    iowrite32(ctrl, bpfcap_fpga_device->regs + BPFCAP_FPGA_CTRL);
    iowrite32(pkt_begin, bpfcap_fpga_device->regs + BPFCAP_FPGA_PKT_BEGIN);
    iowrite32(pkt_end, bpfcap_fpga_device->regs + BPFCAP_FPGA_PKT_END);
    iowrite32(write_addr, bpfcap_fpga_device->regs + BPFCAP_FPGA_WRITE_ADDR);
    return 0;
}
EXPORT_SYMBOL(bpfcap_fpga_write_regs);

module_init(bpfcap_fpga_init);
module_exit(bpfcap_fpga_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jakub Duchniewicz, j.duchniewicz@gmail.com");
MODULE_DESCRIPTION("FPGA accelerated tcpdump with eBPF driver.");
MODULE_VERSION("1.0");
