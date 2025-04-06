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
#include <linux/slab.h>  // For kmalloc, krealloc, kfree
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Rajkumar Saravanakumar"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    
    filp->private_data = container_of(inode->i_cdev, struct aesd_dev, cdev);
     
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    
    struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry * entry;
    size_t entry_offset;
    
    if(!dev || !buf) return -EINVAL;
    
    mutex_lock(&dev->lock);
    
    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->circular_buffer, *f_pos, &entry_offset);
    if(!entry) {
    	mutex_unlock(&dev->lock);
    	return 0;
    }
    
    size_t available = entry->size - entry_offset;
    size_t read_size = min(count, available);
    if (copy_to_user(buf, entry->buffptr + entry_offset, read_size)) {
        mutex_unlock(&dev->lock);
        return -EFAULT;
    }
    
    *f_pos += read_size;
    retval = read_size;

    mutex_unlock(&dev->lock);
        
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    
    struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry new_entry;
    char *temp_buf;

    if (!dev || !buf)
        return -EINVAL;

    temp_buf = kmalloc(count, GFP_KERNEL);
    if (!temp_buf)
        return -ENOMEM;

    if (copy_from_user(temp_buf, buf, count)) {
        kfree(temp_buf);
        return -EFAULT;
    }
    
    mutex_lock(&dev->lock); 
    
    if (dev->partial_write) {
        size_t new_size = dev->partial_write_size + count;
        char *new_buf = krealloc(dev->partial_write, new_size, GFP_KERNEL);
        if (!new_buf) {
            mutex_unlock(&dev->lock);
            kfree(temp_buf);
            return -ENOMEM;
        }
        memcpy(new_buf + dev->partial_write_size, temp_buf, count);
        kfree(temp_buf);
        dev->partial_write = new_buf;
        dev->partial_write_size = new_size;

        if (new_buf[new_size - 1] != '\n') {
            mutex_unlock(&dev->lock);
            return count; // write on circular buffer only when terminated with \n
        }

        new_entry.buffptr = new_buf;
        new_entry.size = new_size;
        dev->partial_write = NULL;
        dev->partial_write_size = 0;
    } else {
        if (temp_buf[count - 1] != '\n') {
            dev->partial_write = temp_buf;
            dev->partial_write_size = count;
            mutex_unlock(&dev->lock);
            return count;
        }
        new_entry.buffptr = temp_buf;
        new_entry.size = count;
    }

     if (dev->circular_buffer.full)
        kfree(dev->circular_buffer.entry[dev->circular_buffer.out_offs].buffptr);
	// clear the buffptr

     aesd_circular_buffer_add_entry(&dev->circular_buffer, &new_entry);
     retval = count;

     mutex_unlock(&dev->lock);
    
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

    // cdev init
    cdev_init(&dev->cdev, &aesd_fops); //aesdchar instance and aesd_fops
    
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    
    // cdev add
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

    // asks the kernel to reserve a region of device numbers
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
            
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    
    // initializes aesdc_device structrue declared b4
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    
    mutex_init(&aesd_device.lock);
    aesd_circular_buffer_init(&aesd_device.circular_buffer);
    
    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    mutex_destroy(&aesd_device.lock);
      

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
