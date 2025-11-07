import array
from pyspw_rmap import SpwRmapTCPNode, TargetNode
from datetime import timedelta


target = TargetNode(
    initiator_logical_address=0xFE,
    # target_spacewire_address=[0x01, 0x02],
    # reply_address=[0x01, 0x03],
    target_spacewire_address=[0x02],
    reply_address=[0x03],
)


initiator = SpwRmapTCPNode(ip_address="192.168.2.101", port=10030)
initiator.set_buffer(send_buffer_size=2**20, recv_buffer_size=2**20)
initiator.connect(
    timedelta(milliseconds=1000),
    timedelta(milliseconds=1000),
    timedelta(seconds=1),
)

data = bytearray(4)
# data[:] = b"\x00\x00\x00\x00"
# initiator.write(target, 0x44A20000 + 0x0000006C, memoryview(data))
#
initiator.read(target, 0x44A20000 + 0x0000006C, memoryview(data))

for b in data:
    print(f"{b:02X}", end=" ")
