/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) 2002 Cluster File Systems, Inc.
 *
 * This code is issued under the GNU General Public License.
 * See the file COPYING in this distribution
 *
 * by Cluster File Systems, Inc.
 * authors, Peter Braam <braam@clusterfs.com> & 
 * Phil Schwan <phil@clusterfs.com>
 */

#define EXPORT_SYMTAB

#include <linux/version.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <asm/unistd.h>

#define DEBUG_SUBSYSTEM S_LDLM

#include <linux/obd_support.h>
#include <linux/obd_class.h>

#include <linux/lustre_dlm.h>

extern kmem_cache_t *ldlm_lock_slab;

static struct ldlm_lock *ldlm_lock_new(struct ldlm_lock *parent,
                                       struct ldlm_resource *resource,
                                       ldlm_mode_t mode)
{
        struct ldlm_lock *lock;

        lock = kmem_cache_alloc(ldlm_lock_slab, SLAB_KERNEL);
        if (lock == NULL)
                BUG();

        memset(lock, 0, sizeof(*lock));
        lock->l_resource = resource;
        lock->l_req_mode = mode;
        INIT_LIST_HEAD(&lock->l_children);

        if (parent != NULL) {
                lock->l_parent = parent;
                list_add(&lock->l_childof, &parent->l_children);
        }

        return lock;
}

static int ldlm_notify_incompatible(struct list_head *list,
                                    struct ldlm_lock *new)
{
        struct list_head *tmp;
        int rc = 0;

        list_for_each(tmp, list) {
                struct ldlm_lock *lock;
                lock = list_entry(tmp, struct ldlm_lock, l_res_link);
                if (lockmode_compat(lock->l_req_mode, new->l_req_mode))
                        continue;

                rc = 1;

                if (lock->l_resource->lr_blocking != NULL)
                        lock->l_resource->lr_blocking(lock, new);
        }

        return rc;
}


static int ldlm_reprocess_queue(struct list_head *queue, 
                                struct list_head *granted_list)
{
        struct list_head *tmp1, *tmp2;
        struct ldlm_resource *res;
        int rc = 0;

        list_for_each(tmp1, queue) { 
                struct ldlm_lock *pending;
                rc = 0; 
                pending = list_entry(tmp1, struct ldlm_lock, l_res_link);

                /* check if pending can go in ... */ 
                list_for_each(tmp2, granted_list) {
                        struct ldlm_lock *lock;
                        lock = list_entry(tmp2, struct ldlm_lock, l_res_link);
                        if (lockmode_compat(lock->l_granted_mode, 
                                            pending->l_req_mode))
                                continue;
                        else { 
                                /* no, we are done */
                                rc = 1;
                                break;
                        }
                }

                if (rc) { 
                        /* no - we are done */
                        break;
                }

                res = pending->l_resource;
                list_del(&pending->l_res_link); 
                list_add(&pending->l_res_link, &res->lr_granted);
                pending->l_granted_mode = pending->l_req_mode;

                if (pending->l_granted_mode < res->lr_most_restr)
                        res->lr_most_restr = pending->l_granted_mode;

                /* XXX call completion here */ 
                

        }

        return rc;
}

ldlm_error_t ldlm_local_lock_enqueue(struct obd_device *obddev, __u32 ns_id,
                                     struct ldlm_resource *parent_res,
                                     struct ldlm_lock *parent_lock,
                                     __u32 *res_id, ldlm_mode_t mode, 
                                     struct ldlm_handle *lockh)
{
        struct ldlm_namespace *ns;
        struct ldlm_resource *res;
        struct ldlm_lock *lock;
        int incompat, rc;

        ENTRY;

	ns = ldlm_namespace_find(obddev, ns_id);
	if (ns == NULL || ns->ns_hash == NULL)
		BUG();

        res = ldlm_resource_get(ns, parent_res, res_id, 1);
        if (res == NULL)
                BUG();

        lock = ldlm_lock_new(parent_lock, res, mode);
        if (lock == NULL)
                BUG();

        lockh->addr = (__u64)(unsigned long)lock;
        spin_lock(&res->lr_lock);

        /* FIXME: We may want to optimize by checking lr_most_restr */

        if (!list_empty(&res->lr_converting)) {
                list_add(&lock->l_res_link, res->lr_waiting.prev);
                rc = ELDLM_BLOCK_CONV;
                GOTO(out, rc);
        }
        if (!list_empty(&res->lr_waiting)) {
                list_add(&lock->l_res_link, res->lr_waiting.prev);
                rc = ELDLM_BLOCK_WAIT;
                GOTO(out, rc);
        }
        incompat = ldlm_notify_incompatible(&res->lr_granted, lock);
        if (incompat) {
                list_add(&lock->l_res_link, res->lr_waiting.prev);
                rc = ELDLM_BLOCK_GRANTED;
                GOTO(out, rc);
        }

        list_add(&lock->l_res_link, &res->lr_granted);
        lock->l_granted_mode = mode;
        if (mode < res->lr_most_restr)
                res->lr_most_restr = mode;

        /* XXX call the completion call back function */ 

        rc = ELDLM_OK;
        GOTO(out, rc);

 out:
        spin_unlock(&res->lr_lock);
        return rc;
}

ldlm_error_t ldlm_local_lock_cancel(struct obd_device *obddev, 
                                     struct ldlm_handle *lockh)
{
        struct ldlm_lock *lock;
        struct ldlm_resource *res = lock->l_resource;
        ENTRY;

        lock = (struct ldlm_lock *)(unsigned long)lockh->addr;
        list_del(&lock->l_res_link); 

        kmem_cache_free(ldlm_lock_slab, lock); 
        if (ldlm_resource_put(lock->l_resource)) {
                EXIT;
                return 0;
        }
        
        ldlm_reprocess_queue(&res->lr_converting, &res->lr_granted); 
        if (list_empty(&res->lr_converting))
                ldlm_reprocess_queue(&res->lr_waiting, &res->lr_granted); 
        
        return 0;
}

void ldlm_lock_dump(struct ldlm_lock *lock)
{
        char ver[128];

        if (RES_VERSION_SIZE != 4)
                BUG();

        snprintf(ver, sizeof(ver), "%x %x %x %x",
                 lock->l_version[0], lock->l_version[1],
                 lock->l_version[2], lock->l_version[3]);

        CDEBUG(D_OTHER, "  -- Lock dump: %p (%s)\n", lock, ver);
        CDEBUG(D_OTHER, "  Parent: %p\n", lock->l_parent);
        CDEBUG(D_OTHER, "  Requested mode: %d, granted mode: %d\n",
               (int)lock->l_req_mode, (int)lock->l_granted_mode);
}
