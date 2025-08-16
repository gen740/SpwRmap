from pyspw_rmap import TargetNode, LegacySpwRmapTCPNode, SpwRmapTCPNode
import time

Offset = 0x44A20000
RmapTest = 0x0000006C + Offset
TiTime = 0x00000060 + Offset
EnableFlag = 0x00000038 + Offset
ForcetrigFlag = 0x00000034 + Offset
VaFlag = 0x00000028 + Offset
ObsmodeFlag = 0x00000030 + Offset
DRAMWritePointer = 0x000000D4 + Offset
DRAMWritePointerResetReq = 0x000000D8 + Offset
PseudoONOFF = 0x000000C0 + Offset
PseudoRate = 0x000000C4 + Offset
PseudoCounter = 0x000000C8 + Offset
SetUpModeFlag = 0x0000002C + Offset
ExtSignalModeFlag = 0x0000003C + Offset
Timecode = 0x0000009C + Offset


a = TargetNode(
    logical_address=0x34, target_spacewire_address=[1, 2], reply_address=[1, 3]
)

# spw_node = LegacySpwRmapTCPNode("192.168.2.100", 10030)
spw_node = SpwRmapTCPNode("192.168.2.100", 10030)
spw_node.connect()
spw_node.set_buffer(4096, 4096)


for i in range(100000):
    # time.sleep(0.01)

    data_receive = bytearray(4)
    spw_node.read(target_node=a, memory_address=TiTime, data=memoryview(data_receive))

    print(f"TiTime: {int.from_bytes(data_receive, 'big')}")
