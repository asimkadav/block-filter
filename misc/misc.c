/*
*
* Copyright (c) 2013, Asim Kadav, asimkadav@gmail.com
* 
*  A very simple block filter driver
*  
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*  
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include <linux/pci.h>
#include <linux/bug.h>
#include <linux/kallsyms.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/rtnetlink.h>
#include <linux/lockdep.h>
#include <linux/slab.h>
#include <linux/highmem.h>
#include <linux/swap.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/blkdev.h>

#include "misc.h"

MODULE_AUTHOR("Asim Kadav");
MODULE_LICENSE("GPL");

static struct miscdevice misc_help;
static struct block_device *blkdev;
static void (*original_request_fn) (struct request_queue*, struct bio*);


/* Sample ioctl code - not used. Can be used to trigger on/off filtering. */
static long mischelp_ioctl(/*struct inode *inode,*/ struct file *fp,
                unsigned int cmd, unsigned long arg) {

        if (cmd == MISC_GET)    {
                printk ("Can perform get ops %d.\n", (int) arg);
        }

        if (cmd == MISC_PUT)    {
                printk ("Can perform put ops %d.\n", (int) arg);
        }

        return 0;
}


struct file_operations misc_fops = {
        .unlocked_ioctl = mischelp_ioctl,
        .owner = THIS_MODULE,
        .mmap = NULL,
};

void misc_request_fn(struct request_queue *q, struct bio *bio) {
        printk ("we are passing bios.\n");
        // here is where we trace requests...
        original_request_fn (q, bio);
        return;
}


void register_block_device(char *path)  {

        struct request_queue *blkdev_queue = NULL;

        if (path == NULL)       {
                printk ("Block device empty.\n");
                return;
        }

        printk ("Will open %s.\n", path);

        blkdev = lookup_bdev(path);

        if (IS_ERR(blkdev))     {
                printk ("No such block device.\n");
                return;
        }

        printk ("Found block device %p with bs %d.\n", blkdev, blkdev->bd_block_size);
        blkdev_queue = bdev_get_queue(blkdev);
        original_request_fn = blkdev_queue->request_fn;
        blkdev_queue->request_fn = misc_request_fn;
}

void unregister_block_device(void)      {
        struct request_queue *blkdev_queue = NULL;

        blkdev_queue = bdev_get_queue(blkdev);

        if ((blkdev_queue->request_fn != NULL) &&
                        (original_request_fn != NULL))  {

                blkdev_queue->request_fn = original_request_fn;
                printk ("Successfully unregistered block device.\n");
        }
}



int init_module(void)   {
        int retval = 0;
        static char *mischelp_name = "mischelp";

        misc_help.minor = MISC_MINOR;
        misc_help.name = mischelp_name;
        misc_help.fops = &misc_fops;
        retval = misc_register(&misc_help);

        if (retval)
                return retval;

        register_block_device("/dev/sda");

        printk ("block tracer initialized successfully.\n");
        return 0;
}

void cleanup_module(void){
        int number = 0;

        unregister_block_device();

        number = misc_deregister(&misc_help);
        if (number < 0) {
                printk ("misc_deregister failed. %d\n", number);
        }

        printk ("It's over for block tracer.. \n");
}


