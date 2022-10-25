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

#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

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
    
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);

    if(mutex_lock_interruptible(&(aesd_device.lock)))
        return -ERESTARTSYS;

    offsetEntry = aesd_circular_buffer_find_entry_offset_for_fpos(
                                                    &aesd_device.dev_cb_fifo,
                                                    *f_pos,
                                                    &offsetEntry_ind);

    if(!offsetEntry)
    {
        goto out;
    }

    if(count>(offsetEntry->size - offsetEntry_ind))
    {
        newCount = offsetEntry->size - offsetEntry_ind;
    }
    else
    {
        newCount = count;
    }

    bytesLeft=newCount;

    PDEBUG("Copying %u bytes from %p",bytesLeft,
                                &(offsetEntry->buffptr[offsetEntry_ind]));

    if(copy_to_user(buf, &(offsetEntry->buffptr[offsetEntry_ind]),bytesLeft))
    {
        retval = -EFAULT;
        goto out;
    }   
    *f_pos += bytesLeft;
    retval = bytesLeft;
    //Read out count number of bytes to buf (can implement partial-read
    //rule, which means that only the remainder of the identified entry
    //will be returned)
    //To do this, return value must be remainder bytes number and
    //fpos should be updated

    out:
        mutex_unlock(&(aesd_device.lock));
        PDEBUG("Returning %zu for read", count);
        return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    char* newLimboString = NULL;
    uint32_t entrySize = 0;
    uint32_t bytesLeft;
    bool nlFlag = false;
    struct aesd_circular_buffer *cb_fifo = NULL;
    struct aesd_buffer_entry newEntry;    

    char *placeholder = NULL;

    ssize_t retval = -ENOMEM;

    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);

    cb_fifo = &(aesd_device.dev_cb_fifo);

    if(mutex_lock_interruptible(&(aesd_device.lock)))
        return -ERESTARTSYS;

    placeholder = kmalloc(sizeof(char)*count, GFP_KERNEL);

    if(!placeholder)
    {
        retval = -ENOMEM;
        goto out;
    }

    bytesLeft=count;
    do{
        bytesLeft = copy_from_user(placeholder+count-bytesLeft,
                                            buf+count-bytesLeft,
                                            bytesLeft);

    } while(bytesLeft != 0);

    while(entrySize < count && !nlFlag)
    {
        if(placeholder[entrySize]=='\n')
        {
            nlFlag = true;
            break;
        }

        entrySize++;
    }

    if(aesd_device.inLimbo)
    {
            newLimboString = kmalloc(sizeof(char)*(aesd_device.limboLength\
                                                     + entrySize),
                                        GFP_KERNEL);

            if(!newLimboString)
            {
                retval = -ENOMEM;
                goto out;
            }

            memcpy(newLimboString, aesd_device.limboString, aesd_device.limboLength);
            memcpy(newLimboString + aesd_device.limboLength,
                    placeholder,
                    entrySize);

            kfree(placeholder);

            if(nlFlag)
            {
                //Add new entry to CB
                newEntry.buffptr = newLimboString;
                newEntry.size = aesd_device.limboLength + entrySize;

                if(cb_fifo->full)
                {
                    kfree(cb_fifo->entry[cb_fifo->out_offs].buffptr);
                }
                aesd_circular_buffer_add_entry(cb_fifo, &newEntry);

                //Get device out of packet limbo
                aesd_device.inLimbo = false;
                aesd_device.limboLength = 0;
                kfree(aesd_device.limboString);
            }
            else
            {
                //Update limbo string
                kfree(aesd_device.limboString);
                aesd_device.limboString = newLimboString;
                aesd_device.limboString += entrySize;  
            }
    }
    else
    {
        if(nlFlag)
        {
            //Clean packet entry scenario
            newEntry.buffptr = placeholder;
            newEntry.size = entrySize;

            if(cb_fifo->full)
            {
                kfree(cb_fifo->entry[cb_fifo->out_offs].buffptr);
            }
            aesd_circular_buffer_add_entry(cb_fifo, &newEntry);
        }
        else
        {
            //Enter limbo
            aesd_device.inLimbo = true;
            aesd_device.limboString = placeholder;
            aesd_device.limboLength = entrySize;
        }
    }
    retval=entrySize+1;
    
    out:
        mutex_unlock(&(aesd_device.lock));
        return retval;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
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
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    aesd_device.limboString = NULL;

    //Maybe do some other initialization here. Don't even
    //have to initialize cb fifo bc above statement does
    //what we need
    mutex_init(&aesd_device.lock);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
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

    //Delete semaphore needed?
    if(aesd_device.inLimbo)
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
