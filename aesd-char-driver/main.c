/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations

#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0)
#define access_ok_wrapper(type, arg, cmd) \
    access_ok(type, arg, cmd)
#else
#define access_ok_wrapper(type, arg, cmd) \
    access_ok(arg, cmd)
#endif

#include "aesdchar.h"
int aesd_major = 0; // use dynamic major
int aesd_minor = 0;

#include "aesd_ioctl.h"

MODULE_AUTHOR("Erich Clever");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");

    filp->private_data = &aesd_device;

    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                  loff_t *f_pos)
{
    struct aesd_buffer_entry *offsetEntry;
    size_t offsetEntry_ind;

    uint32_t bytesLeft, newCount;

    ssize_t retval = 0;

    PDEBUG("read %zu bytes with offset %lld", count, *f_pos);

    if (mutex_lock_interruptible(&(aesd_device.lock)))
        return -ERESTARTSYS;

    offsetEntry = aesd_circular_buffer_find_entry_offset_for_fpos(
        &aesd_device.dev_cb_fifo,
        *f_pos,
        &offsetEntry_ind);

    if (!offsetEntry)
    {
        PDEBUG("At cbfifo's end!");
        retval = 0;
        goto out;
    }

    if (count > (offsetEntry->size - offsetEntry_ind))
    {
        newCount = offsetEntry->size - offsetEntry_ind;
    }
    else
    {
        newCount = count;
    }

    bytesLeft = newCount;

    PDEBUG("Copying %u bytes from %p", bytesLeft,
           &(offsetEntry->buffptr[offsetEntry_ind]));

    if (copy_to_user(buf, &(offsetEntry->buffptr[offsetEntry_ind]), bytesLeft))
    {
        retval = -EFAULT;
        goto out;
    }
    *f_pos += bytesLeft;
    retval = bytesLeft;
    // Read out count number of bytes to buf (can implement partial-read
    // rule, which means that only the remainder of the identified entry
    // will be returned)
    // To do this, return value must be remainder bytes number and
    // fpos should be updated

out:
    mutex_unlock(&(aesd_device.lock));
    PDEBUG("Returning %zu bytes for read", count);
    PDEBUG("Actual return value: %zu", retval);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                   loff_t *f_pos)
{
    char *newLimboString = NULL;
    uint32_t entrySize = 0;
    uint32_t bytesLeft;
    bool nlFlag = false;
    struct aesd_circular_buffer *cb_fifo = NULL;
    struct aesd_buffer_entry newEntry;

    char *placeholder = NULL;

    ssize_t retval = -ENOMEM;

    PDEBUG("write %zu bytes with offset %lld", count, *f_pos);

    cb_fifo = &(aesd_device.dev_cb_fifo);

    if (mutex_lock_interruptible(&(aesd_device.lock)))
        return -ERESTARTSYS;

    placeholder = kmalloc(sizeof(char) * count, GFP_KERNEL);

    if (!placeholder)
    {
        retval = -ENOMEM;
        goto out;
    }

    bytesLeft = count;
    do
    {
        bytesLeft = copy_from_user(placeholder + count - bytesLeft,
                                   buf + count - bytesLeft,
                                   bytesLeft);

    } while (bytesLeft != 0);

    while (entrySize < count && !nlFlag)
    {
        if (placeholder[entrySize] == '\n')
        {
            nlFlag = true;
        }

        entrySize++;
    }

    if (aesd_device.inLimbo)
    {
        newLimboString = kmalloc(sizeof(char) * (aesd_device.limboLength + entrySize),
                                 GFP_KERNEL);

        if (!newLimboString)
        {
            retval = -ENOMEM;
            goto out;
        }

        memcpy(newLimboString, aesd_device.limboString, aesd_device.limboLength);
        memcpy(newLimboString + aesd_device.limboLength,
               placeholder,
               entrySize);

        kfree(placeholder);

        if (nlFlag)
        {
            // Add new entry to CB
            newEntry.buffptr = newLimboString;
            newEntry.size = aesd_device.limboLength + entrySize;

            if (cb_fifo->full)
            {
                kfree(cb_fifo->entry[cb_fifo->out_offs].buffptr);
            }
            aesd_circular_buffer_add_entry(cb_fifo, &newEntry);

            PDEBUG("%zu bytes at %p in cbfifo", newEntry.size, newEntry.buffptr);

            // Get device out of packet limbo
            aesd_device.inLimbo = false;
            aesd_device.limboLength = 0;
            kfree(aesd_device.limboString);
        }
        else
        {
            // Update limbo string
            kfree(aesd_device.limboString);
            aesd_device.limboString = newLimboString;
            aesd_device.limboLength += entrySize;
        }
    }
    else
    {
        if (nlFlag)
        {
            // Clean packet entry scenario
            newEntry.buffptr = placeholder;
            newEntry.size = entrySize;

            if (cb_fifo->full)
            {
                kfree(cb_fifo->entry[cb_fifo->out_offs].buffptr);
            }
            aesd_circular_buffer_add_entry(cb_fifo, &newEntry);
        }
        else
        {
            // Enter limbo
            aesd_device.inLimbo = true;
            aesd_device.limboString = placeholder;
            aesd_device.limboLength = entrySize;
        }
    }
    retval = entrySize;

out:
    mutex_unlock(&(aesd_device.lock));
    return retval;
}

loff_t aesd_llseek(struct file *filp, loff_t off, int whence)
{
    loff_t newpos;
    struct aesd_buffer_entry *entryPtr = NULL;
    struct aesd_circular_buffer *devBuffPtr = &aesd_device.dev_cb_fifo;
    size_t entryPtrInd;

    uint64_t devBuffSize;

    switch (whence)
    {
    case 0: /* SEEK_SET */
        entryPtr = aesd_circular_buffer_find_entry_offset_for_fpos(
            devBuffPtr, off, &entryPtrInd);
        if (entryPtr)
        {
            newpos = off;
        }
        break;

    case 1: /* SEEK_CUR */
        entryPtr = aesd_circular_buffer_find_entry_offset_for_fpos(
            devBuffPtr, filp->f_pos + off, &entryPtrInd);
        if (entryPtr)
        {
            newpos = filp->f_pos + off;
        }
        break;

    case 2: /* SEEK_END */
        bufferSize(devBuffPtr, &devBuffSize);

        entryPtr = aesd_circular_buffer_find_entry_offset_for_fpos(
            devBuffPtr, devBuffSize + off, &entryPtrInd);

        if (entryPtr)
        {
            newpos = devBuffSize + off;
        }
        break;

    default: /* can't happen */
        return -EINVAL;
    }
    if (!entryPtr)
        return -EINVAL;
    filp->f_pos = newpos;
    return newpos;
}

long aesd_ioctl(struct file *filp, unsigned int cmd, long unsigned int seekParams_usr_addr)
{
    // int err = 0, tmp;
    int retval = 0;
    struct aesd_circular_buffer *devBuffPtr = &aesd_device.dev_cb_fifo;
    loff_t newpos = 0;
    int index, currOffset = 0;
    struct aesd_seekto seekParams;
    unsigned long retVal=0;

    /*
     * extract the type and number bitfields, and don't decode
     * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
     */
    if (_IOC_TYPE(cmd) != AESD_IOC_MAGIC)
        return -ENOTTY;
    if (_IOC_NR(cmd) > AESDCHAR_IOC_MAXNR)
        return -ENOTTY;

    /*
     * the direction is a bitmask, and VERIFY_WRITE catches R/W
     * transfers. `Type' is user-oriented, while
     * access_ok is kernel-oriented, so the concept of "read" and
     * "write" is reversed
     */
    // if (_IOC_DIR(cmd) & _IOC_READ)
    // 	err = !access_ok_wrapper(VERIFY_WRITE, (void __user *)seekParams, _IOC_SIZE(cmd));
    // else if (_IOC_DIR(cmd) & _IOC_WRITE)
    // 	err =  !access_ok_wrapper(VERIFY_READ, (void __user *)seekParams, _IOC_SIZE(cmd));
    // if (err) return -EFAULT;

    switch (cmd)
    {

    case AESDCHAR_IOCSEEKTO:

        retVal = copy_from_user(&seekParams,
                                (void *)seekParams_usr_addr,
                                sizeof(struct aesd_seekto));

        if(retVal)
        {
            return -EFAULT;
        }

        // seekParams = (struct aesd_seekto*)(seekParams_usr_addr);

        PDEBUG("K-space ArgX: %u\r\nK-Space ArgY: %u",seekParams.write_cmd,seekParams.write_cmd_offset);

        // Range-checking
        if (seekParams.write_cmd > (bufferLength(devBuffPtr) - 1))
        {
            return -EINVAL;
        }

        // Wrapping search of cb fifo
        if (devBuffPtr->full || (devBuffPtr->out_offs > devBuffPtr->in_offs))
        {
            for (index = devBuffPtr->out_offs; index < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED &&
                                               currOffset <= seekParams.write_cmd;
                 index++)
            {
                if (currOffset == seekParams.write_cmd)
                {
                    if (seekParams.write_cmd_offset >= devBuffPtr->entry[index].size)
                    {
                        return -EINVAL;
                    }
                    else
                    {
                        filp->f_pos = (newpos + seekParams.write_cmd_offset);
                        return 0;
                    }
                }
                newpos += devBuffPtr->entry[index].size;
                currOffset++;
            }
            for (index = 0; index <= seekParams.write_cmd; index++)
            {
                if (currOffset == seekParams.write_cmd)
                {
                    if (seekParams.write_cmd_offset >= devBuffPtr->entry[index].size)
                    {
                        return -EINVAL;
                    }
                    else
                    {
                        filp->f_pos = (newpos + seekParams.write_cmd_offset);
                        return 0;
                    }
                }
                newpos += devBuffPtr->entry[index].size;
                currOffset++;
            }
        }
        else if (devBuffPtr->out_offs < devBuffPtr->in_offs)
        {
            for (index = devBuffPtr->out_offs; currOffset <= seekParams.write_cmd; index++)
            {
                if (currOffset == seekParams.write_cmd)
                {
                    if (seekParams.write_cmd_offset >= devBuffPtr->entry[index].size)
                    {
                        return -EINVAL;
                    }
                    else
                    {
                        filp->f_pos = (newpos + seekParams.write_cmd_offset);
                        return 0;
                    }
                }
                newpos += devBuffPtr->entry[index].size;
                currOffset++;
            }
        }
        else
        {
            // Buffer empty
            filp->f_pos = 0;
            return 0;
        }
        return -EINVAL;

    default: /* redundant, as cmd was checked against MAXNR */
        return -ENOTTY;
    }
    return retval;
}

struct file_operations aesd_fops = {
    .owner = THIS_MODULE,
    .read = aesd_read,
    .write = aesd_write,
    .open = aesd_open,
    .release = aesd_release,
    .llseek = aesd_llseek,
    .unlocked_ioctl = aesd_ioctl,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add(&dev->cdev, devno, 1);
    if (err)
    {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}

int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
                                 "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0)
    {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device, 0, sizeof(struct aesd_dev));

    aesd_device.limboString = NULL;

    // Maybe do some other initialization here. Don't even
    // have to initialize cb fifo bc above statement does
    // what we need
    mutex_init(&aesd_device.lock);

    result = aesd_setup_cdev(&aesd_device);

    if (result)
    {
        unregister_chrdev_region(dev, 1);
    }
    return result;
}

void aesd_cleanup_module(void)
{
    struct aesd_buffer_entry *entryptr = NULL;
    int i;

    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    // Delete semaphore needed?
    if (aesd_device.inLimbo)
    {
        kfree(aesd_device.limboString);
    }

    AESD_CIRCULAR_BUFFER_FOREACH(entryptr, &aesd_device.dev_cb_fifo, i)
    {
        kfree(entryptr->buffptr);
    }

    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
