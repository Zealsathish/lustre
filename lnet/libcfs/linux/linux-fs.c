# define DEBUG_SUBSYSTEM S_LNET

#include <linux/fs.h>
#include <linux/ctype.h>
#include <asm/uaccess.h>

#include <libcfs/libcfs.h>

cfs_file_t *
cfs_filp_open (const char *name, int flags, int mode, int *err)
{
	/* XXX
	 * Maybe we need to handle flags and mode in the future
	 */
	cfs_file_t	*filp = NULL;

	filp = filp_open(name, flags, mode); 
	if (IS_ERR(filp)) { 
		int rc;

		rc = PTR_ERR(filp); 
		printk(KERN_ERR "LustreError: can't open %s file: err %d\n", 
				name, rc);
		if (err)
			*err = rc;
		filp = NULL;
	}
	return filp;
}

ssize_t
cfs_user_write (cfs_file_t *filp, const char *buf, size_t count, loff_t *offset)
{
        mm_segment_t fs;
        ssize_t size;

        fs = get_fs();
        set_fs(KERNEL_DS);
        size = filp->f_op->write(filp, (char *)buf, count, offset);
        set_fs(fs);

        return size;
}

EXPORT_SYMBOL(cfs_filp_open);
EXPORT_SYMBOL(cfs_user_write);
