/*
 * aesdchar.h
 *
 *  Created on: Oct 23, 2019
 *      Author: Dan Walkes
 */

#ifndef AESD_CHAR_DRIVER_AESDCHAR_H_
#define AESD_CHAR_DRIVER_AESDCHAR_H_

#include <linux/cdev.h>
#include <linux/mutex.h>
#include "aesd-circular-buffer.h"

#define AESD_DEBUG 1  //Remove comment on this line to enable debug

#undef PDEBUG             /* undef it, just in case */
#ifdef AESD_DEBUG
#  ifdef __KERNEL__
     /* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_DEBUG "aesdchar: " fmt, ## args)
#  else
     /* This one for user space */
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif
#else
#  define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif

struct aesd_dev
{
    struct cdev cdev;                     /* Char device structure */
    struct aesd_circular_buffer circular_buffer;   /* Circular buffer for storing write operations */
    struct mutex lock;                     /* Mutex for thread safety */
    char *partial_write;                   /* Buffer for incomplete write operations */
    size_t partial_write_size;                   /* Size of the partial write */
};

int aesd_open(struct inode *, struct file *);
int aesd_release(struct inode *, struct file *);
ssize_t aesd_read(struct file *, char __user *, size_t, loff_t *);
ssize_t aesd_write(struct file *, const char __user *, size_t, loff_t *);
int aesd_init_module(void);
void aesd_cleanup_module(void);




#endif /* AESD_CHAR_DRIVER_AESDCHAR_H_ */
