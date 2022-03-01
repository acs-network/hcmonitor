PCI_PATH="0000:31:00.1"
dev=enp49s0f1
RTE_SDK=$PWD/dpdk
${RTE_SDK}/usertools/dpdk-devbind.py --status
echo ""
echo -n "Enter name of kernel driver to bind the device to: "
read DRV
${RTE_SDK}/usertools/dpdk-devbind.py -b $DRV $PCI_PATH && echo "OK"
ifconfig dpdk0 down
ifconfig $dev 192.168.57.100/16 up

