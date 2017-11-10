#!/bin/bash
if [ -d /mnt/usbdisk ];then
	umount /mnt/usbdisk
	rm -r /mnt/usbdisk
fi
