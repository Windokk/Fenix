make
qemu-system-x86_64 -cdrom build/fenix.iso -serial stdio -m 512 -smp 4 -machine q35 \
    -device qemu-xhci,id=xhci \
    -device usb-kbd,bus=xhci.0 \
    -device usb-mouse,bus=xhci.0