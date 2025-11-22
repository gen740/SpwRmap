from __future__ import annotations

import contextlib
import time
from typing import Iterable, List

from pyspw_rmap import SpwRmapTCPNode, TargetNode


def make_target(logical_address: int = 0x32) -> TargetNode:
  target = TargetNode()
  target.logical_address = logical_address
  target.target_spacewire_address = [0x06, 0x02]
  target.reply_address = [0x01, 0x03]
  return target


@contextlib.contextmanager
def open_node(ip: str = "127.0.0.1", port: str = "10030"):
  node = SpwRmapTCPNode(ip_address=ip, port=port)
  node.start()
  try:
    yield node
  finally:
    node.stop()


def print_bytes(label: str, data: Iterable[int]) -> None:
  hex_values = " ".join(f"0x{byte:02X}" for byte in data)
  print(f"{label}: {hex_values}")


def main() -> None:
  target = make_target()

  with open_node() as node:
    # Blocking write.
    payload: List[int] = [0x01, 0x02, 0x03, 0x04]
    node.write(target, 0x44A20000, payload)
    print("Synchronous write completed.")

    # Blocking read.
    read_data = node.read(target, 0x44A20000, len(payload))
    print_bytes("Synchronous read", read_data)

    # Simple polling loop.
    for i in range(4):
      data = node.read(target, 0x44A20010, 4)
      print_bytes(f"Poll #{i + 1}", data)
      time.sleep(0.5)


if __name__ == "__main__":
  main()
