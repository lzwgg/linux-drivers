#!/bin/bash
if [ ! -d /mnt/usbdisk ];then
	mkdir -p /mnt/usbdisk
fi
mount /dev/block/usbdisk0 /mnt/usbdisk
