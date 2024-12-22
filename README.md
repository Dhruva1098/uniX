# uniX
we will use a lba throughtout
I will make conversion from chs as:
    sectors = (LBA % sectors per track) + 1
    head = (LBA / sectors per tracl) %heads
    cylinder = (LBA / sectors per track ) / heads
