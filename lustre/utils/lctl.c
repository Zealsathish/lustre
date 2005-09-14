/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 *  Copyright (C) 2002 Cluster File Systems, Inc.
 *   Author: Peter J. Braam <braam@clusterfs.com>
 *   Author: Phil Schwan <phil@clusterfs.com>
 *   Author: Robert Read <rread@clusterfs.com>
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



#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <portals/ptlctl.h>
#include "obdctl.h"
#include "parser.h"

static int jt_quit(int argc, char **argv) {
        Parser_quit(argc, argv);
        return 0;
}

static int jt_noop(int argc, char **argv) {
        return 0;
}

static int jt_opt_ignore_errors(int argc, char **argv) {
        Parser_ignore_errors(1);
        return 0;
}

#include <linux/lustre_audit.h>

static int parse_audit_ops(char * str, __u64 * mask)
{
        char * op = strtok(str, ",");
        int rc = 0;
        
        while (op) {
                if (!strcmp(op, "all")) {
                        *mask |= AUDIT_ALL_OPS;
                        break;
                }
                else if (!strcmp(op,"none")) {
                        *mask &= ~AUDIT_ALL_OPS;
                }
                else if (!strcmp(op,"create"))
                        SET_AUDIT_OP((*mask), AUDIT_CREATE);
                else if (!strcmp(op,"link"))
                        SET_AUDIT_OP((*mask), AUDIT_LINK);
                else if (!strcmp(op,"delete"))
                        SET_AUDIT_OP((*mask), AUDIT_UNLINK);
                else if (!strcmp(op,"rename"))
                        SET_AUDIT_OP((*mask), AUDIT_RENAME);
                else if (!strcmp(op,"read")) {
                        SET_AUDIT_OP((*mask), AUDIT_READ);
                        SET_AUDIT_OP((*mask), AUDIT_MMAP);
                }
                else if (!strcmp(op,"write"))
                        SET_AUDIT_OP((*mask), AUDIT_WRITE);
                else if (!strcmp(op,"open"))
                        SET_AUDIT_OP((*mask), AUDIT_OPEN);
                else if (!strcmp(op,"stat"))
                        SET_AUDIT_OP((*mask), AUDIT_STAT);
                else if (!strcmp(op,"readdir"))
                        SET_AUDIT_OP((*mask), AUDIT_READDIR);
                else if (!strcmp(op,"readlink"))
                        SET_AUDIT_OP((*mask), AUDIT_READLINK);
                else if (!strcmp(op,"setattr"))
                        SET_AUDIT_OP((*mask), AUDIT_SETATTR);
                else {
                        fprintf(stderr, "invalid audit operation '%s'\n", op);
                }       

                op = strtok(NULL, ",");
        }
        
        return rc;
}

#include <unistd.h>
#include <dirent.h>
#include <linux/unistd.h>

static int set_dir_audit(char * dir, __u64 mask)
{
        int rc = 0;
        char * buf;
        DIR * sdr;
        struct dirent * ent = (void*)buf;
        
        buf = malloc(strlen(dir) + NAME_MAX + 1);
        
        sdr = opendir(dir);
        if (!sdr) {
                fprintf(stderr, "cannot open dir %s\n", dir);
                free(buf);
                return -1;
        }
        
        while ((ent = readdir(sdr)) != NULL) {
                int fd;
                
                if (!strcmp(ent->d_name, ".") ||
                    !strcmp(ent->d_name, ".."))
                        continue;
                
                sprintf(buf, "%s/%s", dir, ent->d_name);
                
                fd = open(buf, O_RDONLY);
                if (fd < 0) {
                        fprintf(stderr, "can't open: %s: %s\n", buf,
                                strerror(rc = errno));
                        continue;
                }
                rc = ioctl(fd, LL_IOC_AUDIT, mask);
                close(fd);
        }
        closedir(sdr);
        free(buf);
        return rc;
}

static int set_audit(int argc, char **argv, int fs)
{
        __u64 mask = 0;
        struct stat st;
        int rc, fd;

        if (argc != 4)
                return CMD_HELP;
        
        /* audit can be for all operations or for failed only */
        if (!strcmp(argv[1], "fail"))
                SET_AUDIT_OP(mask, AUDIT_FAIL);
        else if (strcmp(argv[1], "all")) {
                fprintf(stderr, "%s: invalid audit type %s\n",
                        jt_cmdname(argv[0]), argv[1]);
                return -EINVAL;
        }
        
        rc = parse_audit_ops(argv[2], &mask);
        if (rc < 0)
                return -EINVAL;
        
        //open file/dir
        fd = open(argv[3], O_RDONLY);
        if (fd < 0) {
                fprintf(stderr, "can't open: %s: %s\n", argv[3],
                        strerror(rc = errno));
                return -1;
        }
        
        rc = fstat(fd, &st);
        if (rc) {
                close(fd);
                return rc;
        }
        //audit for fs?
        if (fs)
                SET_AUDIT_OP(mask, AUDIT_FS);
        else {
                //if dir then set audit for childs also
                if (S_ISDIR(st.st_mode)) {
                        //rc = set_dir_audit(argv[3], mask | AUD_BIT(AUDIT_DIR));
                }
        }
        //set audit for file/dir itself
        rc = ioctl(fd, LL_IOC_AUDIT, mask);
        close(fd);
        return rc;
}

static int jt_set_audit(int argc, char **argv)
{
        return set_audit(argc, argv, 0);
}

static int jt_set_fsaudit(int argc, char **argv)
{
        return set_audit(argc, argv, 1);
}

static int jt_flush_audit(int argc, char **argv)
{
        __u64 mask = 0;
        int rc, fd;

        if (argc != 2)
                return CMD_HELP;
        
        //open file/dir
        fd = open(argv[1], O_RDONLY);
        if (fd < 0) {
                fprintf(stderr, "can't open: %s: %s\n", argv[1],
                        strerror(rc = errno));
                return -1;
        }
        
        SET_AUDIT_OP(mask, AUDIT_SYNC);

        rc = ioctl(fd, LL_IOC_AUDIT, mask);
        close(fd);
        return rc;
}

/*
 * XXX Should not belong to here
 */
static int flush_cred_ioctl(char *mp)
{
        int fd, rc;

        fd = open(mp, O_RDONLY);
        if (fd == -1) {
                fprintf(stderr, "flush_cred_ioctl: error open %s: %s\n",
                        mp, strerror(errno));
                return -1;
        }

        rc = ioctl(fd, LL_IOC_FLUSH_CRED);
        if (rc == -1) {
                fprintf(stderr, "flush_cred_ioctl: error ioctl %s: %s\n",
                        mp, strerror(errno));
        }

        close(fd);
        return rc;
}

static int jt_flush_cred(int argc, char **argv)
{
        FILE   *proc;
        char    procline[PATH_MAX], *line;
        int     i, rc = 0;

        /* no args means search all lustre mountpoints */
        if (argc < 2) {
                proc = fopen("/proc/mounts", "r");
                if (!proc) {
                        fprintf(stderr, "%s: can't open /proc/mounts\n",
                                jt_cmdname(argv[0]));
                        return -1;
                }

                while ((line = fgets(procline, PATH_MAX, proc)) != NULL) {
                        char dev[PATH_MAX];
                        char mp[PATH_MAX];
                        char fs[PATH_MAX];

                        if (sscanf(line, "%s %s %s", dev, mp, fs) != 3) {
                                fprintf(stderr, "%s: unexpected format in "
                                                "/proc/mounts\n",
                                        jt_cmdname(argv[0]));
                                return -1;
                        }

                        if (strcmp(fs, "lustre") &&
                            strcmp(fs, "lustre_lite"))
                                continue;

                        if (flush_cred_ioctl(mp))
                                rc = -1;
                }
        } else {
                /* follow the exact flush sequence as supplied */
                for (i = 1; i < argc; i++) {
                        if (flush_cred_ioctl(argv[i]))
                                rc = -1;
                }
        }

        return rc;
}

command_t cmdlist[] = {
        /* Metacommands */
        {"--device", jt_opt_device, 0,
         "run <command> after connecting to device <devno>\n"
         "--device <devno> <command [args ...]>"},
        {"--threads", jt_opt_threads, 0,
         "run <threads> separate instances of <command> on device <devno>\n"
         "--threads <threads> <verbose> <devno> <command [args ...]>"},
        {"--ignore_errors", jt_opt_ignore_errors, 0,
         "ignore errors that occur during script processing\n"
         "--ignore_errors"},
        {"ignore_errors", jt_opt_ignore_errors, 0,
         "ignore errors that occur during script processing\n"
         "ignore_errors"},
        {"dump", jt_ioc_dump, 0, "usage: dump file, save ioctl buffer to file"},

        /* Network configuration commands */
        {"==== network config ====", jt_noop, 0, "network config"},
        {"--net", jt_opt_net, 0, "run <command> after setting network to <net>\n"
         "usage: --net <tcp/elan/myrinet> <command>"},
        {"network", jt_ptl_network, 0, "commands that follow apply to net\n"
         "usage: network <tcp/elan/myrinet>"},
        {"interface_list", jt_ptl_print_interfaces, 0, "print interface entries\n"
         "usage: interface_list"},
        {"add_interface", jt_ptl_add_interface, 0, "add interface entry\n"
         "usage: add_interface ip [netmask]"},
        {"del_interface", jt_ptl_del_interface, 0, "del interface entry\n"
         "usage: del_interface [ip]"},
        {"peer_list", jt_ptl_print_peers, 0, "print peer entries\n"
         "usage: peer_list"},
        {"add_peer", jt_ptl_add_peer, 0, "add an peer entry\n"
         "usage: add_peer <nid> <host> <port>"},
        {"del_peer", jt_ptl_del_peer, 0, "remove an peer entry\n"
         "usage: del_peer [<nid>] [<host>] [ks]"},
        {"conn_list", jt_ptl_print_connections, 0, "print all the connected remote nid\n"
         "usage: conn_list"},
        {"connect", jt_ptl_connect, 0, "connect to a remote nid\n"
         "usage: connect <host> <port> [iIOC]"},
        {"disconnect", jt_ptl_disconnect, 0, "disconnect from a remote nid\n"
         "usage: disconnect [<nid>]"},
        {"active_tx", jt_ptl_print_active_txs, 0, "print active transmits\n"
         "usage: active_tx"},
        {"mynid", jt_ptl_mynid, 0, "inform the socknal of the local nid. "
         "The nid defaults to hostname for tcp networks and is automatically "
         "setup for elan/myrinet networks.\n"
         "usage: mynid [<nid>]"},
        {"shownid", jt_ptl_shownid, 0, "print the local NID\n"
         "usage: shownid"},
        {"add_uuid", jt_lcfg_add_uuid, 0, "associate a UUID with a nid\n"
         "usage: add_uuid <uuid> <nid> <net_type>"},
        {"close_uuid", jt_obd_close_uuid, 0, "disconnect a UUID\n"
         "usage: close_uuid <uuid> <net_type>"},
        {"del_uuid", jt_lcfg_del_uuid, 0, "delete a UUID association\n"
         "usage: del_uuid <uuid>"},
        {"add_route", jt_ptl_add_route, 0,
         "add an entry to the portals routing table\n"
         "usage: add_route <gateway> <target> [<target>]"},
        {"del_route", jt_ptl_del_route, 0,
         "delete route via gateway to targets from the portals routing table\n"
         "usage: del_route <gateway> [<target>] [<target>]"},
        {"set_route", jt_ptl_notify_router, 0,
         "enable/disable routes via gateway in the portals routing table\n"
         "usage: set_route <gateway> <up/down> [<time>]"},
        {"route_list", jt_ptl_print_routes, 0,
         "print the portals routing table, same as show_route\n"
         "usage: route_list"},
        {"show_route", jt_ptl_print_routes, 0,
         "print the portals routing table, same as route_list\n"
         "usage: show_route"},
        {"fail", jt_ptl_fail_nid, 0, "fail/restore communications.\n"
         "Omitting the count means indefinitely, 0 means restore, "
         "otherwise fail 'count' messages.\n"
         "usage: fail nid|_all_ [count]"},

        /* Device selection commands */
        {"=== device selection ===", jt_noop, 0, "device selection"},
        {"newdev", jt_lcfg_newdev, 0, "create a new device\n"
         "usage: newdev"},
        {"device", jt_obd_device, 0,
         "set current device to <%name|$name|devno>\n"
         "usage: device <%name|$name|devno>"},
        {"cfg_device", jt_lcfg_device, 0,
         "set current device being configured to <$name>\n"
         "usage: cfg_device <name>"},
        {"device_list", jt_obd_list, 0, "show all devices\n"
         "usage: device_list"},
        {"dl", jt_obd_list, 0, "show all devices\n"
         "usage: dl"},
        {"lustre_build_version", jt_get_version, 0,
         "print the build version of lustre\n"
         "usage: lustre_build_version"},

        /* Device configuration commands */
        {"==== device config =====", jt_noop, 0, "device config"},
        {"attach", jt_lcfg_attach, 0,
         "set the type of the current device (with <name> and <uuid>)\n"
         "usage: attach type [name [uuid]]"},
        {"setup", jt_lcfg_setup, 0,
         "type specific device configuration information\n"
         "usage: setup <args...>"},
        {"cleanup", jt_obd_cleanup, 0, "cleanup previously setup device\n"
         "usage: cleanup [force | failover]"},
        {"detach", jt_obd_detach, 0,
         "remove driver (and name and uuid) from current device\n"
         "usage: detach"},
        {"lov_setup", jt_lcfg_lov_setup, 0, "create a LOV device\n"
         "usage: lov_setup lov-uuid stripe-count stripe-size offset pattern"},
        {"lmv_setup", jt_lcfg_lmv_setup, 0,
         "create an LMV device\n"
         "usage: lmv_setup lmv-uuid UUID1 [UUID2 ...]"},
        {"lov_modify_tgts", jt_lcfg_lov_modify_tgts, 0,
         "add or delete an obd to/from a LOV device\n"
         "usage: lov_modify_tgts add|del <lov-name> <uuid> <index> <gen>"},
        {"lov_getconfig", jt_obd_lov_getconfig, 0,
         "read lov configuration from an mds device\n"
         "usage: lov_getconfig lov-uuid"},
        {"lmv_modify_tgts", jt_lcfg_lmv_modify_tgts, 0,
         "add an mdc to a LMV device\n"
         "usage: lmv_modify_tgts add <lmv_name> <mdc_uuid>"},
        {"record", jt_cfg_record, 0, "record commands that follow in log\n"
         "usage: record cfg-uuid-name"},
        {"endrecord", jt_cfg_endrecord, 0, "stop recording\n"
         "usage: endrecord"},
        {"parse", jt_cfg_parse, 0, "parse the log of recorded commands for this config\n"
         "usage: parse config-uuid-name"},
        {"dump_log", jt_cfg_dump_log, 0, "print log of recorded commands for this config to kernel debug log\n"
         "usage: dump_log config-uuid-name"},
        {"clear_log", jt_cfg_clear_log, 0, "delete current config log of recorded commands\n"
         "usage: clear_log config-name"},

        /* Device operations */
        {"=== device operations ==", jt_noop, 0, "device operations"},
        {"probe", jt_obd_connect, 0,
         "build a connection handle to a device.  This command is used to "
         "suspend configuration until lctl has ensured that the mds and osc "
         "services are available.  This is to avoid mount failures in a "
         "rebooting cluster.\n"
         "usage: probe [timeout]"},
        {"close", jt_obd_disconnect, 0,
         "close the connection handle\n"
         "usage: close"},
        {"getattr", jt_obd_getattr, 0,
         "get attribute for OST object <objid>\n"
         "usage: getattr <objid>"},
        {"setattr", jt_obd_setattr, 0,
         "set mode attribute for OST object <objid>\n"
         "usage: setattr <objid> <mode>"},
         {"create", jt_obd_create, 0,
         "create <num> OST objects (with <mode>)\n"
         "usage: create [num [mode [verbose [lsm data]]]]"},
        {"destroy", jt_obd_destroy, 0,
         "destroy OST object <objid> [num [verbose]]\n"
         "usage: destroy <num> objects, starting at objid <objid>"},
        {"test_getattr", jt_obd_test_getattr, 0,
         "do <num> getattrs (on OST object <objid> (objid+1 on each thread))\n"
         "usage: test_getattr <num> [verbose [[t]objid]]"},
        {"test_setattr", jt_obd_test_setattr, 0,
         "do <num> setattrs (on OST object <objid> (objid+1 on each thread))\n"
         "usage: test_setattr <num> [verbose [[t]objid]]"},
        {"test_brw", jt_obd_test_brw, 0,
         "do <num> bulk read/writes (<npages> per I/O, on OST object <objid>)\n"
         "usage: test_brw [t]<num> [write [verbose [npages [[t]objid]]]]"},
        {"get_stripe", jt_obd_get_stripe, 0,
         "show stripe info for an echo client object\n"
         "usage: get_stripe objid\n"},
        {"set_stripe", jt_obd_set_stripe, 0,
         "set stripe info for an echo client object\n"
         "usage: set_stripe objid[=width!count[@offset][:id:id...]\n"},
        {"unset_stripe", jt_obd_unset_stripe, 0,
         "unset stripe info for an echo client object\n"
         "usage: unset_stripe objid\n"},
        {"test_ldlm", jt_obd_test_ldlm, 0,
         "perform lock manager test\n"
         "usage: test_ldlm"},
        {"ldlm_regress_start", jt_obd_ldlm_regress_start, 0,
         "start lock manager stress test\n"
         "usage: ldlm_regress_start [numthreads [refheld [numres [numext]]]]"},
        {"ldlm_regress_stop", jt_obd_ldlm_regress_stop, 0,
         "stop lock manager stress test (no args)\n"},
        {"dump_ldlm", jt_obd_dump_ldlm, 0,
         "dump all lock manager state (no args)"},
        {"disable_recovery", jt_obd_disable_recovery, 0, "disable recovery on an import\n"},
        {"enable_recovery", jt_obd_enable_recovery, 0, "enable recovery on an import\n"},
        {"activate", jt_obd_activate, 0, "activate an import\n"},
        {"deactivate", jt_obd_deactivate, 0, "deactivate an import\n"},
        {"recover", jt_obd_recover, 0, "usage: recover [<connection UUID>]"},
        {"lookup", jt_obd_mdc_lookup, 0, "usage: lookup <directory> <file>"},
        {"notransno", jt_obd_no_transno, 0,
         "disable sending of committed-transno updates\n"},
        {"readonly", jt_obd_set_readonly, 0,
         "disable writes to the underlying device\n"},
        {"abort_recovery", jt_obd_abort_recovery, 0,
         "abort recovery on MDS device\n"},
        {"root_squash", jt_obd_root_squash, 0,
         "squash root to 'uid:gid' except client 'nid'\n"
         "usage: root_squash [uid:gid [nid]]\n"},
        {"start", jt_obd_start, 0,
         "setup mds/ost from the llog file\n"
         "usage: start <profilename>"},
        {"mount_option", jt_lcfg_mount_option, 0, 
         "usage: mount_option profile osc_name [mdc_name] [gsc_name] \n"},
        {"del_mount_option", jt_lcfg_del_mount_option, 0,
         "usage: del_mount_option profile\n"},
        {"set_timeout", jt_lcfg_set_timeout, 0,
         "usage: set_timeout <secs>\n"},
        {"set_lustre_upcall", jt_lcfg_set_lustre_upcall, 0,
         "usage: set_lustre_upcall </full/path/to/upcall> \n"},
        {"add_conn ", jt_lcfg_add_conn, 0,
         "usage: add_conn <conn_uuid> [priority]\n"},
        {"del_conn ", jt_lcfg_del_conn, 0,
         "usage: del_conn <conn_uuid> \n"},
        {"set_security", jt_lcfg_set_security, 0,
         "usage: set_security key value\n"},
        {"audit", jt_set_audit, 0,
         "usage: audit type operations filename\n"},
        {"fs_audit", jt_set_fsaudit, 0,
         "usage: fs_audit type operations mountpoint\n"},
        {"flush_audit", jt_flush_audit, 0,
         "usage: flush_audit mountpoin\n"},
        {"lsync", jt_obd_reint_sync, 0,
         "usage: lsync\n"},  
        {"cache_on", jt_obd_cache_on, 0,
         "usage: lsync\n"},  
        {"cache_off", jt_obd_cache_off, 0,
         "usage: lsync\n"},  
/*
        {"obd_flush_cred", jt_obd_flush_cred, 0,
         "usage: obd_flush_cred [all]\n"},
*/
        /*snap operations*/
        {"snap_add", jt_obd_snap_add, 0, 
         "usage: snap_add <dev_name> <snap_name>\n"}, 
        /* Llog operations */ 
        {"llog_catlist", jt_llog_catlist, 0, 
         "list all catalog logs on current device.\n"
         "usage: llog_catlist"},
        {"llog_info", jt_llog_info, 0,
         "print log header information.\n"
         "usage: llog_info <$logname|#oid#ogr#ogen>\n"
         "       oid, ogr and ogen are hexadecimal."},
        {"llog_print", jt_llog_print, 0,
         "print log content information.\n"
         "usage: llog_print <$logname|#oid#ogr#ogen> [from] [to]\n"
         "       oid, ogr and ogen are hexadecimal.\n"
         "       print all records from index 1 by default."},
        {"llog_check", jt_llog_check, 0,
         "print log content information.\n"
         "usage: llog_check <$logname|#oid#ogr#ogen> [from] [to]\n"
         "       oid, ogr and ogen are hexadecimal.\n"
         "       check all records from index 1 by default."},
         {"llog_cancel", jt_llog_cancel, 0,
         "cancel one record in log.\n"
         "usage: llog_cancel <catalog id|catalog name> <log id> <index>"},
        {"llog_remove", jt_llog_remove, 0,
         "remove one log from catalog, erase it from disk.\n"
         "usage: llog_remove <catalog id|catalog name> <log id>"},
        
        /* Misc commands */
        {"flush_cred", jt_flush_cred, 0,
         "flush the client side credential.\n"
         "usage: flush_cred [mountpoint]..."},
        {"set_crypt", jt_set_lkey_type, 0,
         "set lustre key management type, only support gks and mks\n"
         "usage: set_crypt [dir] [gks|mks]...."},
        
        /* Debug commands */
        {"======== debug =========", jt_noop, 0, "debug"},
        {"debug_daemon", jt_dbg_debug_daemon, 0,
         "debug daemon control and dump to a file\n"
         "usage: debug_daemon {start file [#MB]|stop}"},
        {"debug_kernel", jt_dbg_debug_kernel, 0,
         "get debug buffer and dump to a file, same as dk\n"
         "usage: debug_kernel [file] [raw]"},
        {"dk", jt_dbg_debug_kernel, 0,
         "get debug buffer and dump to a file, same as debug_kernel\n"
         "usage: dk [file] [raw]"},
        {"debug_file", jt_dbg_debug_file, 0,
         "read debug buffer from input and dump to output, same as df\n"
         "usage: debug_file <input> [output] [raw]"},
        {"df", jt_dbg_debug_file, 0,
         "read debug buffer from input and dump to output, same as debug_file\n"
         "usage: df <input> [output] [raw]"},
        {"clear", jt_dbg_clear_debug_buf, 0, "clear kernel debug buffer\n"
         "usage: clear"},
        {"mark", jt_dbg_mark_debug_buf, 0,"insert marker text in kernel debug buffer\n"
         "usage: mark <text>"},
        {"filter", jt_dbg_filter, 0, "filter message type\n"
         "usage: filter <subsystem id/debug mask>"},
        {"show", jt_dbg_show, 0, "Show specific type of messages\n"
         "usage: show <subsystem id/debug mask>"},
        {"debug_list", jt_dbg_list, 0, "list subsystem and debug types\n"
         "usage: debug_list <subs/types>"},
        {"modules", jt_dbg_modules, 0,
         "provide gdb-friendly module information\n"
         "usage: modules <path>"},
        {"panic", jt_dbg_panic, 0, "force the kernel to panic\n"
         "usage: panic"},
        {"lwt", jt_ptl_lwt, 0,
         "light-weight tracing\n"
         "usage: lwt start\n"
         "       lwt stop [file]"},
        {"memhog", jt_ptl_memhog, 0,
         "memory pressure testing\n"
         "usage: memhog <page count> [<gfp flags>]"},
                
        /* User interface commands */
        {"======= control ========", jt_noop, 0, "control commands"},
        {"help", Parser_help, 0, "help"},
        {"exit", jt_quit, 0, "quit"},
        {"quit", jt_quit, 0, "quit"},
        { 0, 0, 0, NULL }
};

int lctl_main(int argc, char **argv)
{
        int rc;

        setlinebuf(stdout);

        ptl_initialize(argc, argv);
        if (obd_initialize(argc, argv) < 0)
                exit(2);
        if (dbg_initialize(argc, argv) < 0)
                exit(3);

        Parser_init("lctl > ", cmdlist);

        if (argc > 1) {
                rc = Parser_execarg(argc - 1, argv + 1, cmdlist);
        } else {
                rc = Parser_commands();
        }

        obd_finalize(argc, argv);
        return rc;
}

#if !LIBLUSTRE_TEST
int main (int argc, char **argv)
{
        return (lctl_main (argc, argv));
}
#endif
