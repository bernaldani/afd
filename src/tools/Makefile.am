## Makefile.am -- Process this file with automake to produce Makefile.in
## Copyright (C) 2001 - 2007 Holger Kiehl
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2, or (at your option)
## any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
##

if WITHOUT_DUPCHECK
   DUPCHECK =
   EXTRA_DUPCHECK = dup_spy
else
   DUPCHECK = dup_spy
   EXTRA_DUPCHECK =
endif

INCLUDES                     = -I.. -I../init_afd -I../amg -I../fd\
                               -I../statistics
bin_PROGRAMS                 = afdcmd afdcfg afd_status fsa_view fra_view\
                               get_dc_data get_hostname grib2wmo set_pw udc
sbin_PROGRAMS                = cache_spy convert_fsa convert_stat\
                               current_job_list_spy dc_id_spy dir_spy\
                               $(DUPCHECK) file_dir_check file_mask_list_spy\
                               fsa_edit gen_file queue_spy job_list_spy\
                               reset_fsa set_counter rm_job show_exec_stat\
                               view_ls_data
EXTRA_PROGRAMS               = bfc binfc byte_order convert_jid count_burst\
                               del_cache $(EXTRA_DUPCHECK) get_counter\
                               force_archive_check get_full_date\
                               get_file_size gts2tiff linger rdu sysconf\
                               tiff_sniffer watch_dir fra_version\
                               convert_ls_data view_error_queue
MAINTAINERCLEANFILES         = Makefile.in $(EXTRA_PROGRAMS)
CLEANFILES                   = gmon.out core core.*
afdcmd_SOURCES               = afdcmd.c
afdcmd_LDADD                 = ../common/libafd.a
afdcfg_SOURCES               = afdcfg.c
afdcfg_LDADD                 = ../common/libafd.a
afd_status_SOURCES           = afd_status.c
afd_status_LDADD             = ../common/libafd.a
bfc_SOURCES                  = bull_file_check.c
bfc_LDADD                    = ../common/libafd.a
binfc_SOURCES                = binfc.c
binfc_LDADD                  = ../common/libafd.a ../misc/libmisc.a
byte_order_SOURCES           = byte_order.c
cache_spy_SOURCES            = cache_spy.c
cache_spy_LDADD              = ../common/libafd.a
convert_fsa_SOURCES          = convert_fsa.c
convert_fsa_LDADD            = ../common/libafd.a
convert_jid_SOURCES          = convert_jid.c
convert_jid_LDADD            = ../common/libafd.a
convert_ls_data_SOURCES      = convert_ls_data.c
convert_ls_data_LDADD        = ../common/libafd.a
convert_stat_SOURCES         = convert_stat.c
convert_stat_LDADD           = ../common/libafd.a
count_burst_SOURCES          = count_burst.c
current_job_list_spy_SOURCES = current_job_list_spy.c
current_job_list_spy_LDADD   = ../common/libafd.a
dc_id_spy_SOURCES            = dc_id_spy.c
dc_id_spy_LDADD              = ../common/libafd.a
del_cache_SOURCES            = del_cache.c
del_cache_LDADD              = ../common/libafd.a
dir_spy_SOURCES              = dir_spy.c
dir_spy_LDADD                = ../common/libafd.a
dup_spy_SOURCES              = dup_spy.c
dup_spy_LDADD                = ../common/libafd.a
file_dir_check_SOURCES       = file_dir_check.c
file_dir_check_LDADD         = ../common/libafd.a
file_mask_list_spy_SOURCES   = file_mask_list_spy.c
file_mask_list_spy_LDADD     = ../common/libafd.a
force_archive_check_SOURCES  = force_archive_check.c
force_archive_check_LDADD    = ../common/libafd.a
fsa_view_SOURCES             = fsa_view.c
fsa_view_LDADD               = ../common/libafd.a
fra_version_SOURCES          = fra_version.c
fra_version_LDADD            = ../common/libafd.a
fra_view_SOURCES             = fra_view.c
fra_view_LDADD               = ../common/libafd.a
fsa_edit_SOURCES             = fsa_edit.c
fsa_edit_LDADD               = ../common/libafd.a
gen_file_SOURCES             = gen_file.c
get_counter_SOURCES          = get_counter.c
get_counter_LDADD            = ../common/libafd.a
get_dc_data_SOURCES          = get_dc_data.c
get_dc_data_LDADD            = ../common/libafd.a
get_file_size_SOURCES        = get_file_size.c
get_full_date_SOURCES        = get_full_date.c
get_hostname_SOURCES         = get_hostname.c
get_hostname_LDADD           = ../common/libafd.a
grib2wmo_SOURCES             = grib2wmo.c\
                               receive_log.c
grib2wmo_LDADD               = ../misc/libmisc.a
gts2tiff_SOURCES             = gts2tiff.c\
                               receive_log.c
gts2tiff_LDADD               = ../misc/libmisc.a ../common/libafd.a 
job_list_spy_SOURCES         = job_list_spy.c
job_list_spy_LDADD           = ../common/libafd.a
linger_SOURCES               = linger.c
queue_spy_SOURCES            = queue_spy.c
queue_spy_LDADD              = ../common/libafd.a
rdu_SOURCES                  = rdu.c
reset_fsa_SOURCES            = reset_fsa.c
reset_fsa_LDADD              = ../common/libafd.a
rm_job_SOURCES               = rm_job.c
rm_job_LDADD                 = ../common/libafd.a
set_counter_SOURCES          = set_counter.c
set_counter_LDADD            = ../common/libafd.a
set_pw_SOURCES               = set_pw.c
set_pw_LDADD                 = ../common/libafd.a
show_exec_stat_SOURCES       = show_exec_stat.c
show_exec_stat_LDADD         = ../common/libafd.a
sysconf_SOURCES              = sysconf.c
sysconf_LDADD                = ../common/libafd.a
tiff_sniffer_SOURCES         = tiff_sniffer.c
udc_SOURCES                  = update_db.c
udc_LDADD                    = ../common/libafd.a
view_error_queue_SOURCES     = view_error_queue.c
view_error_queue_LDADD       = ../common/libafd.a
view_ls_data_SOURCES         = view_ls_data.c
watch_dir_SOURCES            = watch_dir.c
watch_dir_LDADD              = ../common/libafd.a

if WITH_SETUID_PROGS
install-exec-local:
	@if [ "`find $(DESTDIR)$(bindir) -user 0 -name afdcmd -print`" = "" ] ; \
	then \
		chmod 4755 $(DESTDIR)$(bindir)/afdcmd ; \
		chmod 4755 $(DESTDIR)$(bindir)/udc ; \
		chmod 4755 $(DESTDIR)$(bindir)/get_dc_data ; \
	fi
	cd $(DESTDIR)$(bindir) && rm -f uhc
	cd $(DESTDIR)$(bindir) && ln -s udc uhc
else
install-exec-local:
	cd $(DESTDIR)$(bindir) && rm -f uhc
	cd $(DESTDIR)$(bindir) && ln -s udc uhc
endif

uninstall-local:
	cd $(DESTDIR)$(bindir) && rm -f uhc