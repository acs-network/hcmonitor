#! /bin/bash

HUGEPGSZ=`cat /proc/meminfo  | grep Hugepagesize | cut -d : -f 2 | tr -d ' '`

#
# Creates hugepage filesystem.
#
create_mnt_huge()
{
	echo "Creating /mnt/huge and mounting as hugetlbfs"
	sudo mkdir -p /mnt/huge

	grep -s '/mnt/huge' /proc/mounts > /dev/null
	if [ $? -ne 0 ] ; then
		sudo mount -t hugetlbfs nodev /mnt/huge
	fi
}

#
# Removes hugepage filesystem.
#
remove_mnt_huge()
{
	echo "Unmounting /mnt/huge and removing directory"
	grep -s '/mnt/huge' /proc/mounts > /dev/null
	if [ $? -eq 0 ] ; then
		sudo umount /mnt/huge
	fi

	if [ -d /mnt/huge ] ; then
		sudo rm -R /mnt/huge
	fi
}

#
# Unloads igb_uio.ko.
#
remove_igb_uio_module()
{
	echo "Unloading any existing DPDK UIO module"
	/sbin/lsmod | grep -s igb_uio > /dev/null
	if [ $? -eq 0 ] ; then
		sudo /sbin/rmmod igb_uio
	fi
}

#
# Loads new igb_uio.ko (and uio module if needed).
#
load_igb_uio_module()
{
    UIO_PATH=.
	if [ ! -f $UIO_PATH/igb_uio.ko ];then
        UIO_PATH=../../x86_64-native-linuxapp-gcc/kmod
	    if [ ! -f $UIO_PATH/igb_uio.ko ];then
		    echo "## ERROR: Target does not have the DPDK UIO Kernel Module."
		    echo "       To fix, please try to rebuild target."
		    return
        fi
	fi

	remove_igb_uio_module

	/sbin/lsmod | grep -s uio > /dev/null
	if [ $? -ne 0 ] ; then
		modinfo uio > /dev/null
		if [ $? -eq 0 ]; then
			echo "Loading uio module"
			sudo /sbin/modprobe uio
		fi
	fi

	# UIO may be compiled into kernel, so it may not be an error if it can't
	# be loaded.

	echo "Loading DPDK UIO module"
	sudo /sbin/insmod $UIO_PATH/igb_uio.ko
	if [ $? -ne 0 ] ; then
		echo "## ERROR: Could not load $UIO_PATH/igb_uio.ko."
		quit
	fi
}

# Removes all reserved hugepages.
#
clear_huge_pages()
{
	echo > .echo_tmp
	for d in /sys/devices/system/node/node? ; do
		echo "echo 0 > $d/hugepages/hugepages-${HUGEPGSZ}/nr_hugepages" >> .echo_tmp
	done
	echo "Removing currently reserved hugepages"
	sudo sh .echo_tmp
	rm -f .echo_tmp

	remove_mnt_huge
}

#
# Creates hugepages on specific NUMA nodes.
#
set_numa_pages()
{
	clear_huge_pages

#	echo ""
#	echo "  Input the number of ${HUGEPGSZ} hugepages for each node"
#	echo "  Example: to have 128MB of hugepages available per node in a 2MB huge page system,"
#	echo "  enter '64' to reserve 64 * 2MB pages on each node"
    Pages=$1 # setup 32G 
	echo "Setup $Pages * ${HUGEPGSZ} hugepages for each node"

	echo > .echo_tmp
	for d in /sys/devices/system/node/node? ; do
		node=$(basename $d)
#		echo -n "Number of pages for $node: "
#		read Pages
		echo "$Pages pages for $node"
		echo "echo $Pages > $d/hugepages/hugepages-${HUGEPGSZ}/nr_hugepages" >> .echo_tmp
	done
	echo "Reserving hugepages"
	sudo sh .echo_tmp
	rm -f .echo_tmp

	create_mnt_huge
}

echo "--------------------"
load_igb_uio_module
echo "--------------------"
set_numa_pages 32
echo "--------------------"
ifconfig enp49s0f0 down
ifconfig enp49s0f1 down
sudo ./dpdk-devbind.py -b igb_uio 0000:31:00.0
#sudo ./dpdk-devbind.py -b igb_uio 0000:31:00.1
