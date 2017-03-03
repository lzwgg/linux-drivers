
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/slab.h> // contains of kmalloc()
#include <asm/io.h>
//#include <asm/system.h>
#include <asm/uaccess.h>

#define VIRTUALDISK_SIZE 0x2000
#define MEM_CLEAR 0x1
#define PORT1_SET 0x2
#define PORT2_SET 0x3
#define VIRTUALDISK_MAJOR 200

static int VirtualDisk_major = VIRTUALDISK_MAJOR;

/*
VitualDisk struct
*/

struct VirtualDisk
{
    struct cdev cdev;
    unsigned char mem[VIRTUALDISK_SIZE];
    int port1;
    long port2;
    long count;
} *Virtualdisk_devp;

static loff_t VirtualDisk_llseek(struct file *filp, loff_t offset, int orig);
static ssize_t VirtualDisk_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos);
static ssize_t VirtualDisk_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos);
static long VirtualDisk_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
int VirtualDisk_open(struct inode *inode, struct file *filp);
static int VirtualDisk_release(struct inode *inode, struct file *filp);

static const struct file_operations VirtualDisk_fops = 
{
    .owner = THIS_MODULE,
    .llseek = VirtualDisk_llseek,
    .read = VirtualDisk_read,
    .write = VirtualDisk_write,
    .unlocked_ioctl = VirtualDisk_ioctl,
    .open = VirtualDisk_open,
    .release = VirtualDisk_release
};

static void VirtualDisk_setup_cdev(struct VirtualDisk *dev, int minor)
{
    dev_t devno;
    int err;
    devno = MKDEV(VirtualDisk_major, minor);
    cdev_init(&dev->cdev, &VirtualDisk_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &VirtualDisk_fops;
    err = cdev_add(&dev->cdev, devno, 1);
    if (err)
        printk(KERN_NOTICE "Error in cdev_add() \n");
}

int VirtualDisk_init(void)
{
    int ret;
    dev_t devno = MKDEV(VirtualDisk_major, 0);
    if (VirtualDisk_major)
        ret = register_chrdev_region(devno, 1, "VirtualDisk");
    else
    {
        ret = alloc_chrdev_region(&devno, 0, 1, "VirtualDisk");
        VirtualDisk_major = MAJOR(devno);
    }
    if (ret < 0)
        return ret;
    Virtualdisk_devp = kmalloc(sizeof(struct VirtualDisk), GFP_KERNEL);
    if (!Virtualdisk_devp)
    {
        ret = - ENOMEM;
        goto fail_kmalloc;
    }
    memset(Virtualdisk_devp, 0, sizeof(struct VirtualDisk));

    VirtualDisk_setup_cdev(Virtualdisk_devp, 0);
    return 0;
    fail_kmalloc:
        unregister_chrdev_region(devno, 1);
    return ret;
}

void VirtualDisk_exit(void)
{
    cdev_del(&Virtualdisk_devp->cdev);
    kfree(Virtualdisk_devp);
    unregister_chrdev_region(MKDEV(VirtualDisk_major, 0), 1);
}

int VirtualDisk_open(struct inode *inode, struct file *filp)
{
    struct VirtualDisk *devp;
    filp->private_data = Virtualdisk_devp;
    devp = filp->private_data;
    devp->count++;
    return 0;
}

static int VirtualDisk_release(struct inode *inode, struct file *filp)
{
    struct VirtualDisk *devp = filp->private_data;
    devp->count--;
    return 0;
}

static ssize_t VirtualDisk_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
    unsigned long p = *ppos;
    unsigned int count = size;
    int ret = 0;
    struct VirtualDisk *devp = filp->private_data;
    if (p >= VIRTUALDISK_SIZE)
        return count ? - ENXIO : 0;
    if (count > VIRTUALDISK_SIZE - p)
        count = VIRTUALDISK_SIZE - p;
    if (copy_to_user(buf, (void*)(devp->mem + p), count))
    {
        ret = - EFAULT;
    }
    else
    {
        *ppos += count;
        ret = count;
        printk(KERN_INFO "read %d bytes(s) from %d\n", count, p);
    }
    return ret;
}

static ssize_t VirtualDisk_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos)
{
    unsigned long p = *ppos;
    unsigned int count = size;
    int ret = 0;
    struct VirtualDisk *devp = filp->private_data;
    if (p >= VIRTUALDISK_SIZE)
        return count ? - ENXIO : 0;
    if (count > VIRTUALDISK_SIZE - p)
        count = VIRTUALDISK_SIZE - p;
    if (copy_from_user(devp->mem + p, buf, count))
    {
        ret = - EFAULT;
    }
    else
    {
        *ppos += count;
        ret = count;
        printk(KERN_INFO "written %d bytes(s) from %d\n", count, p);
    }
    return ret;
}

static loff_t VirtualDisk_llseek(struct file *filp, loff_t offset, int orig)
{
    loff_t ret = 0;
    switch (orig)
    {
        case SEEK_SET:
            if (offset < 0)
            {
                ret = -EINVAL;
                break;
            }
            if ((unsigned int)offset > VIRTUALDISK_SIZE)
            {
                ret = -EINVAL;
                break;
            }
            filp->f_pos = (unsigned int)offset;
            ret = filp->f_pos;
            break;
        case SEEK_CUR:
            if ((filp->f_pos + offset) > VIRTUALDISK_SIZE)
            {
                ret = -EINVAL;
                break;
            }
            if ((filp->f_pos + offset) < 0)
            {
                ret = -EINVAL;
                break;
            }
            filp->f_pos += offset;
            ret = filp->f_pos;
            break;
        default:
            ret = -EINVAL;
            break;
    }
    return ret;
}

static long VirtualDisk_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct VirtualDisk *devp = filp->private_data;
    switch (cmd)
    {
        case MEM_CLEAR:
            memset(devp->mem, 0, VIRTUALDISK_SIZE);
            printk(KERN_INFO "VirtualDisk is set to zero\n");
            break;
        case PORT1_SET:
            devp->port1 = 0;
            break;
        case PORT2_SET:
            devp->port2 = 0;
            break;
        default:
            return -EINVAL;
    }
    return 0;
}
