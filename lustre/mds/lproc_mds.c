/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 *  Copyright (C) 2002 Cluster File Systems, Inc.
 *
 *   This file is part of Lustre, http://www.lustre.org.
 *
 *   Lustre is free software; you can redistribute it and/or
 *   modify it under the terms of version 2 of the GNU General Public
 *   License as published by the Free Software Foundation.
 *
 *   Lustre is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Lustre; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#define DEBUG_SUBSYSTEM S_CLASS

#include <linux/lustre_lite.h>
#include <linux/lprocfs_status.h>


int rd_uuid(char* page, char **start, off_t off,
               int count, int *eof, void *data)
{
        struct obd_device* temp=(struct obd_device*)data;
        int len=0;
        len+=snprintf(page, count, "%s\n", temp->obd_uuid); 
        return len;  
}
int rd_blksize(char* page, char **start, off_t off,
               int count, int *eof, void *data)
{
        struct obd_device* temp=(struct obd_device*)data;
        struct mds_obd *mds=&temp->u.mds;
        struct statfs mystats;
        int rc, len=0;
        
        rc = vfs_statfs(mds->mds_sb, &mystats);
        if (rc) {
                CERROR("mds: statfs failed: rc %d\n", rc);
                return 0;
        }
        len+=snprintf(page, count, LPU64"\n", (__u64)(mystats.f_bsize)); 
        return len;

}
int rd_blktotal(char* page, char **start, off_t off,
                int count, int *eof, void *data)
{
       struct obd_device* temp=(struct obd_device*)data;
        struct mds_obd *mds=&temp->u.mds;
        struct statfs mystats;
        int rc, len=0;
        
        rc = vfs_statfs(mds->mds_sb, &mystats);
        if (rc) {
                CERROR("mds: statfs failed: rc %d\n", rc);
                return 0;
        }
        len+=snprintf(page, count, LPU64"\n", (__u64)(mystats.f_blocks)); 
        return len;  
        
}

int rd_blkfree(char* page, char **start, off_t off,
               int count, int *eof, void *data)
{
        struct obd_device* temp=(struct obd_device*)data;
        struct mds_obd *mds=&temp->u.mds;
        struct statfs mystats;
        int rc, len=0;
        
        rc = vfs_statfs(mds->mds_sb, &mystats);
        if (rc) {
                CERROR("mds: statfs failed: rc %d\n", rc);
                return 0;
        }
        len+=snprintf(page, count, LPU64"\n", (__u64)(mystats.f_bfree)); 
        return len;  
        
}

int rd_kbfree(char* page, char **start, off_t off,
              int count, int *eof, void *data)
{
        struct obd_device* temp=(struct obd_device*)data;
        struct mds_obd *mds=&temp->u.mds;
        struct statfs mystats;
        int rc, blk_size, len=0;
        
        rc = vfs_statfs(mds->mds_sb, &mystats);
        if (rc) {
                CERROR("mds: statfs failed: rc %d\n", rc);
                return 0;
        }
        blk_size=mystats.f_bsize;
        len+=snprintf(page, count, LPU64"\n", 
                      (__u64)((mystats.f_bfree)/(blk_size*1024))); 
        return len;  
       
}

int rd_fstype(char* page, char **start, off_t off,
                  int count, int *eof, void *data)
{               
        struct obd_device* temp=(struct obd_device*)data;
        struct mds_obd *mds=&temp->u.mds;
        int len=0;
        len+=snprintf(page, count, "%s\n", mds->mds_fstype); 
        return len;  
 
}

int rd_ffiles(char* page, char **start, off_t off,
               int count, int *eof, void *data)
{
        struct obd_device* temp=(struct obd_device*)data;
        struct mds_obd *mds=&temp->u.mds;
        struct statfs mystats;
        int rc, len=0;
        
        rc = vfs_statfs(mds->mds_sb, &mystats);
        if (rc) {
                CERROR("mds: statfs failed: rc %d\n", rc);
                return 0;
        }
        
        len+=snprintf(page, count, LPU64"\n", (__u64)(mystats.f_files));
        return len;  

        
}

int rd_inodesfree(char* page, char **start, off_t off,
                 int count, int *eof, void *data)
{
        struct obd_device* temp=(struct obd_device*)data;
        struct mds_obd *mds=&temp->u.mds;
        struct statfs mystats;
        int rc, len=0;
        
        rc = vfs_statfs(mds->mds_sb, &mystats);
        if (rc) {
                CERROR("mds: statfs failed: rc %d\n", rc);
                return 0;
        }
        
        len+=snprintf(page, count, LPU64"\n", (__u64)(mystats.f_ffree));
        return len; 
}

int rd_filesets(char* page, char **start, off_t off,
                 int count, int *eof, void *data)
{
        return 0;
}
struct lprocfs_vars status_var_nm_1[]={
        {"status/uuid", rd_uuid, 0},
        {"status/f_blocksize",rd_blksize, 0},
        {"status/f_blockstotal",rd_blktotal, 0},
        {"status/f_blocksfree",rd_blkfree, 0},
        {"status/f_kbytesfree", rd_kbfree, 0},
        {"status/f_fstype", rd_fstype, 0},
        {"status/f_files", rd_ffiles, 0},
        {"status/f_inodesfree", rd_inodesfree, 0},
        {"status/f_filesets", rd_filesets, 0},
        {0}
};
int rd_numdevices(char* page, char **start, off_t off,
                  int count, int *eof, void *data)
{
        struct obd_type* class=(struct obd_type*)data;
        int len=0;
        len+=snprintf(page, count, "%d\n", class->typ_refcnt);
        return len;
}

struct lprocfs_vars status_class_var[]={
        {"status/num_devices", rd_numdevices, 0},
        {0}
};
