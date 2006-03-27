/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */

#ifndef _MGS_INTERNAL_H
#define _MGS_INTERNAL_H

#ifdef __KERNEL__
# include <linux/fs.h>
# include <linux/dcache.h>
#endif
#include <linux/lustre_handles.h>
#include <libcfs/kp30.h>
#include <linux/lustre_idl.h>
#include <linux/lustre_lib.h>
#include <linux/lustre_dlm.h>
#include <linux/lustre_log.h>
#include <linux/lustre_export.h>


/* MDS has o_t * 1000 */
#define MGS_SERVICE_WATCHDOG_TIMEOUT (obd_timeout * 10)

/* mgs_llog.c */
#define FSDB_EMPTY 0x0001

struct fs_db {
        char              fsdb_name[8];
        struct list_head  fsdb_list;
        struct semaphore  fsdb_sem;
        void*             fsdb_ost_index_map;
        void*             fsdb_mdt_index_map;
        __u32             fsdb_flags;
        __u32             fsdb_gen;
};

int mgs_init_fsdb_list(struct obd_device *obd);
int mgs_cleanup_fsdb_list(struct obd_device *obd);
int mgs_check_index(struct obd_device *obd, struct mgs_target_info *mti);
int mgs_check_failnid(struct obd_device *obd, struct mgs_target_info *mti);
int mgs_write_log_target(struct obd_device *obd, struct mgs_target_info *mti);
int mgs_upgrade_sv_14(struct obd_device *obd, struct mgs_target_info *mti);
int mgs_erase_logs(struct obd_device *obd, char *fsname);
int mgs_setparam(struct obd_device *obd, char *fsname, struct lustre_cfg *lcfg);

/* mgs_fs.c */
int mgs_fs_setup(struct obd_device *obd, struct vfsmount *mnt);
int mgs_fs_cleanup(struct obd_device *obddev);


#endif
