#!/bin/bash
if [ -L /dev/block/usbdisk0 ];then
	umount /mnt/usbdisk
	rm -r /mnt/usbdisk
fi
