#!/bin/bash
DISK=~/Desktop/myos/disk.img
BUILD=~/Desktop/myos/userspace/build

echo "Updating disk image..."
for elf in $BUILD/*.elf; do
    name=$(basename "$elf" .elf)
    sudo debugfs -w "$DISK" -R "rm $name" 2>/dev/null
    sudo debugfs -w "$DISK" -R "write $elf $name" 2>/dev/null
    echo "  wrote $name"
done

echo "Hello from ext2!" > /tmp/readme.txt
sudo debugfs -w "$DISK" -R "rm readme.txt" 2>/dev/null
sudo debugfs -w "$DISK" -R "write /tmp/readme.txt readme.txt" 2>/dev/null
echo "  wrote readme.txt"
echo "Done!"
chmod +x ~/Desktop/myos/tools/update_disk.sh
