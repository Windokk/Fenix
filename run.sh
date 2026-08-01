#make

mkdir -p build/ovmf
[ -f build/ovmf/OVMF_CODE.4m.fd ] || cp /usr/share/edk2/x64/OVMF_CODE.4m.fd build/ovmf/
[ -f build/ovmf/OVMF_VARS.4m.fd ] || cp /usr/share/edk2/x64/OVMF_VARS.4m.fd build/ovmf/

qemu-system-x86_64 -cdrom build/fenix.iso -serial stdio -m 4096 -smp 4 -machine q35 \
    -drive if=pflash,format=raw,readonly=on,file=build/ovmf/OVMF_CODE.4m.fd \
    -drive if=pflash,format=raw,file=build/ovmf/OVMF_VARS.4m.fd \
    -device qemu-xhci,id=xhci \
    -device usb-hub,bus=xhci.0,id=xhci-hub,port=2 \
    -device usb-mouse,bus=xhci.0,port=2.3 \
    -device usb-kbd,bus=xhci.0,port=2.4

# To run on SeaBios :
#make
#qemu-system-x86_64 -cdrom build/fenix.iso -serial stdio -m 4096 -smp 4 -machine q35 \
#    -device qemu-xhci,id=xhci \
#    -device usb-kbd,bus=xhci.0 \
#    -device usb-mouse,bus=xhci.0