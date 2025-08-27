
west build -b nrf5340dk_nrf5340_cpuapp -s . -d build

west flash -d build


screen /dev/tty.usbmodem0010500306563 115200