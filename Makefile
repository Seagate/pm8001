#
# Makefile for the pm8001
#
# Copyright (c) 2010-2012 Xyratex International Inc.,
# All rights reserved.
#
# This file is licensed under GPLv2.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; version 2 of the
# License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA

obj-m    := pm8001.o
pm8001-objs := pm8001_ctl.o pm8001_hwi.o pm8001_sas.o pm8001_init.o $(if \
	$(wildcard ${SUBDIRS}/pm8001_debugfs.c \
			pm8001_debugfs.c),pm8001_debugfs.o)
DRV_NAME 	:= pm8001
DRV_MAJ_VERSION := 0.1.36
DRV_BUILD_VER  := F08

KVER	:= $(shell uname -r)
KDIR 	:= $(firstword \
		$(wildcard ./KDIR) \
		$(wildcard /lib/modules/$(KVER)/build) \
		$(wildcard /usr/src/kernels/$(KVER)))
# Old kernel Or use the KDIR softlink
# KDIR	:= /home/scratch/kongkon-ssbuild/ssbuild/FP7.5-drop1/mscsa-321/mscsa-thirdparty/CentOS/BUILD/kernel-2.6.32-220.el6/linux-2.6.32-220.el6.x86_64/
# New kernel or do not use the KDIR soft link
KDIR	:= /home/kdutta/ssbuild/FP7.8/mscsa-thirdparty/CentOS/BUILD/kernel-2.6.32-220.el6/linux-2.6.32-220.el6.x86_64/
PWD    := $(shell pwd)
SRCFILES := *.bin pm8001install *.[ch] *.txt *.spec Makefile
BINFILES := *.bin pm8001install release.txt $(DRV_NAME).ko
SRCTAR := $(DRV_NAME)-$(DRV_MAJ_VERSION).$(DRV_BUILD_VER)_src.tar.bz2
BINTAR := $(DRV_NAME)-$(DRV_MAJ_VERSION).$(DRV_BUILD_VER)_bin.tar.bz2
RPMDIRS := BUILD SPECS RPMS SRPMS SOURCES BUILDROOT
.PHONY : default install tarfiles dist newrev

%.bin: %.h
	${CC} -x c -c -o ${@} ${<}
	objcopy -O binary ${@}

default: pm8001.ko

pm8001.ko:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) MODFLAGS='-DMODULE -D_CONFIG_SCSI_PM8001_DEBUG_FS' modules

install:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) MODFLAGS='-DMODULE -D_CONFIG_SCSI_PM8001_DEBUG_FS' modules_install

tarfiles: $(SRCTAR) $(BINTAR)

$(SRCTAR): $(SRCFILES)
	tar -cjvf $(SRCTAR) $(SRCFILES)

$(BINTAR): $(BINFILES) Makefile
	tar -cjvf $(BINTAR) $(BINFILES)

$(RPMDIRS):
	mkdir $@

dist: $(RPMDIRS) $(SRCTAR) $(BINTAR)
	rpmbuild --define '_topdir $(PWD)' --define 'KVER $(KVER)' -ta $(SRCTAR)

#
# To update the build version use
# make DRV_BUILD_VER=<version> newrev
#
newrev:
	sed -i~ -e 's/^DRV_BUILD_VER.*$$/DRV_BUILD_VER  := $(DRV_BUILD_VER)/' Makefile
	sed -i~ -e 's/^%global BUILD_VER.*$$/%global BUILD_VER $(DRV_BUILD_VER)/' $(DRV_NAME).spec
	sed -i~ -e 's/^#define DRV_BUILD_VER.*$$/#define DRV_BUILD_VER \"$(DRV_BUILD_VER)\"/' pm8001_sas.h

clean:
	@$(RM) -rf $(RPMDIRS)
	@$(RM) *~
	@$(RM) -r *.o *.ko *.ko.unsigned modules.order Module.symvers .pm* .tmp_versions pm8001.mod.c Module.markers tags $(SRCTAR) $(BINTAR)
