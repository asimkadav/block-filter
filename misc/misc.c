/*
*
* Copyright (c) 2013, Asim Kadav, asimkadav@gmail.com
* Updated 2025 for modern Linux kernels (5.0+)
*
*  A simple block filter driver for modern Linux kernels
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/genhd.h>
#include <linux/fs.h>
#include <linux/bio.h>
#include <linux/mutex.h>

#include "misc.h"

MODULE_AUTHOR("Asim Kadav");
MODULE_DESCRIPTION("Simple block filter driver for modern Linux kernels");
MODULE_LICENSE("GPL");
MODULE_VERSION("2.0");

/* Module parameters */
static char *target_device = "/dev/sda1";
module_param(target_device, charp, 0444);
MODULE_PARM_DESC(target_device, "Target block device to filter (default: /dev/sda1)");

/* Driver state */
static struct miscdevice misc_help;
static struct gendisk *filter_disk;
static struct block_device *target_bdev;
static struct mutex filter_lock;
static bool filter_active;

/* Statistics counters */
static atomic64_t bio_count = ATOMIC64_INIT(0);

/* Sample ioctl code - can be used to trigger on/off filtering or get stats */
static long mischelp_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	switch (cmd) {
	case MISC_GET:
		pr_info("block_filter: GET operation requested, arg=%lu\n", arg);
		/* Could return statistics here */
		ret = atomic64_read(&bio_count);
		break;

	case MISC_PUT:
		pr_info("block_filter: PUT operation requested, arg=%lu\n", arg);
		/* Could perform control operations here */
		break;

	default:
		pr_warn("block_filter: Unknown ioctl command 0x%x\n", cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct file_operations misc_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = mischelp_ioctl,
	.llseek = no_llseek,
};

/*
 * Bio submission callback - this is where we intercept I/O requests
 * Modern kernels use submit_bio instead of request_fn
 */
static void filter_submit_bio(struct bio *bio)
{
	struct bio *cloned_bio;

	if (!filter_active || !target_bdev) {
		bio->bi_status = BLK_STS_IOERR;
		bio_endio(bio);
		return;
	}

	/* Increment statistics */
	atomic64_inc(&bio_count);

	/* Log the I/O operation */
	pr_debug("block_filter: Intercepted bio: sector=%llu, size=%u, op=%s\n",
		 (unsigned long long)bio->bi_iter.bi_sector,
		 bio->bi_iter.bi_size,
		 bio_op(bio) == REQ_OP_READ ? "READ" : "WRITE");

	/*
	 * Clone the bio and redirect it to the target device
	 * This allows us to inspect/modify the I/O before forwarding
	 */
	cloned_bio = bio_clone_fast(bio, GFP_NOIO, NULL);
	if (!cloned_bio) {
		pr_err("block_filter: Failed to clone bio\n");
		bio->bi_status = BLK_STS_RESOURCE;
		bio_endio(bio);
		return;
	}

	/* Redirect to target device */
	bio_set_dev(cloned_bio, target_bdev);

	/* Set up completion to handle original bio */
	cloned_bio->bi_private = bio;
	cloned_bio->bi_end_io = bio->bi_end_io;

	/* Submit to underlying device */
	submit_bio_noacct(cloned_bio);
}

static const struct block_device_operations filter_fops = {
	.owner = THIS_MODULE,
	.submit_bio = filter_submit_bio,
};

/*
 * Register and set up the filter block device
 */
static int register_block_device(const char *path)
{
	struct request_queue *q;
	sector_t size;
	int ret;
	fmode_t mode = FMODE_READ | FMODE_WRITE;

	if (!path || strlen(path) == 0) {
		pr_err("block_filter: Block device path is empty\n");
		return -EINVAL;
	}

	pr_info("block_filter: Opening target device %s\n", path);

	/* Open the target block device with proper reference counting */
	target_bdev = blkdev_get_by_path(path, mode, THIS_MODULE);
	if (IS_ERR(target_bdev)) {
		ret = PTR_ERR(target_bdev);
		pr_err("block_filter: Failed to open block device %s: %d\n", path, ret);
		target_bdev = NULL;
		return ret;
	}

	/* Validate the device */
	if (!target_bdev->bd_disk) {
		pr_err("block_filter: Target device has no gendisk\n");
		ret = -EINVAL;
		goto err_put_bdev;
	}

	pr_info("block_filter: Found block device, size=%llu bytes, block_size=%u\n",
		(unsigned long long)bdev_nr_bytes(target_bdev),
		bdev_logical_block_size(target_bdev));

	/* Allocate a new disk for our filter device */
	filter_disk = blk_alloc_disk(NUMA_NO_NODE);
	if (!filter_disk) {
		pr_err("block_filter: Failed to allocate disk\n");
		ret = -ENOMEM;
		goto err_put_bdev;
	}

	/* Configure the filter disk */
	filter_disk->major = 0; /* Dynamically allocated */
	filter_disk->first_minor = 0;
	filter_disk->minors = 1;
	filter_disk->fops = &filter_fops;
	filter_disk->private_data = target_bdev;
	snprintf(filter_disk->disk_name, DISK_NAME_LEN, "blockfilter");

	/* Set the capacity to match the target device */
	size = bdev_nr_sectors(target_bdev);
	set_capacity(filter_disk, size);

	/* Configure queue properties to match target */
	q = filter_disk->queue;
	blk_queue_logical_block_size(q, bdev_logical_block_size(target_bdev));
	blk_queue_physical_block_size(q, bdev_physical_block_size(target_bdev));

	/* Add the disk - makes it visible to the system */
	ret = add_disk(filter_disk);
	if (ret) {
		pr_err("block_filter: Failed to add disk: %d\n", ret);
		goto err_cleanup_disk;
	}

	filter_active = true;
	pr_info("block_filter: Block device filter registered successfully\n");
	pr_info("block_filter: Filter device available as /dev/%s\n",
		filter_disk->disk_name);

	return 0;

err_cleanup_disk:
	blk_cleanup_disk(filter_disk);
	filter_disk = NULL;
err_put_bdev:
	if (target_bdev) {
		blkdev_put(target_bdev, mode);
		target_bdev = NULL;
	}
	return ret;
}

/*
 * Unregister the filter device and release resources
 */
static void unregister_block_device(void)
{
	fmode_t mode = FMODE_READ | FMODE_WRITE;

	mutex_lock(&filter_lock);
	filter_active = false;
	mutex_unlock(&filter_lock);

	/* Remove and cleanup the filter disk */
	if (filter_disk) {
		del_gendisk(filter_disk);
		blk_cleanup_disk(filter_disk);
		filter_disk = NULL;
		pr_info("block_filter: Filter disk removed\n");
	}

	/* Release the target block device */
	if (target_bdev) {
		blkdev_put(target_bdev, mode);
		target_bdev = NULL;
		pr_info("block_filter: Target block device released\n");
	}

	pr_info("block_filter: Total BIOs processed: %lld\n",
		atomic64_read(&bio_count));
}

static int __init block_filter_init(void)
{
	int ret;
	static char *mischelp_name = "blockfilter";

	pr_info("block_filter: Initializing block filter driver\n");

	/* Initialize mutex for synchronization */
	mutex_init(&filter_lock);
	filter_active = false;

	/* Register misc device for control interface */
	misc_help.minor = MISC_DYNAMIC_MINOR; /* Use dynamic minor number */
	misc_help.name = mischelp_name;
	misc_help.fops = &misc_fops;
	misc_help.mode = 0444; /* Read-only for non-root */

	ret = misc_register(&misc_help);
	if (ret) {
		pr_err("block_filter: Failed to register misc device: %d\n", ret);
		return ret;
	}

	pr_info("block_filter: Misc device registered at /dev/%s\n", mischelp_name);

	/* Register the block device filter */
	ret = register_block_device(target_device);
	if (ret) {
		pr_err("block_filter: Failed to register block device: %d\n", ret);
		misc_deregister(&misc_help);
		return ret;
	}

	pr_info("block_filter: Block filter initialized successfully\n");
	return 0;
}

static void __exit block_filter_exit(void)
{
	pr_info("block_filter: Shutting down block filter driver\n");

	/* Unregister the block device filter */
	unregister_block_device();

	/* Unregister misc device */
	misc_deregister(&misc_help);

	pr_info("block_filter: Block filter driver unloaded\n");
}

module_init(block_filter_init);
module_exit(block_filter_exit);
