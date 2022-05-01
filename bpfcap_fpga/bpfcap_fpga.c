#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/fs.h>
#include <linux/io.h>

#define BPFCAP_FPGA_CTRL        0x00 /* Read / Write */
#define BPFCAP_FPGA_PKT_BEGIN   0x04 /* Write */
#define BPFCAP_FPGA_PKT_END     0x08 /* Write */

static int bpfcap_fpga_probe(struct platform_device *pdev);
static int bpfcap_fpga_remove(struct platform_device *pdev);
static ssize_t bpfcap_fpga_write(struct file *file, const char *buffer, size_t len, loff_t *offset);
static ssize_t bpfcap_fpga_read(struct file *file, char *buffer, size_t len, loff_t *offset);

struct bpfcap_fpga_dev {
    struct miscdevice miscdev;
    void __iomem *regs;

    u32     write_value;
    u32     read_value;
};

static struct of_device_id bpfcap_fpga_dt_ids[] = {
    {
        .compatible = "dev, bpfcap_fpga"
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
    .write = bpfcap_fpga_write,
    .read  = bpfcap_fpga_read,
};

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

    if (IS_ERR(dev->regs))
        goto bad_ioremap;

    dev->write_value = 0x0;
    iowrite32(dev->write_value, dev->regs + BPFCAP_FPGA_CTRL);
    iowrite32(dev->write_value, dev->regs + BPFCAP_FPGA_PKT_BEGIN);
    iowrite32(dev->write_value, dev->regs + BPFCAP_FPGA_PKT_END);

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

}

static ssize_t bpfcap_fpga_write(struct file *file, const char *buffer, size_t len, loff_t *offset)
{

}

static ssize_t bpfcap_fpga_read(struct file *file, char *buffer, size_t len, loff_t *offset)
{

}

