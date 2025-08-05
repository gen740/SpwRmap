#include <RMAPInitiator.hh>
#include <SpaceWire.hh>
#include <print>
#include <thread>

class SpaceWireIFDummy final : public SpaceWireIF {
 public:
  void open() override { std::println("SpaceWireIFDummy: open called"); }
  void receive(std::vector<uint8_t>* buffer) override {
    // std::println("SpaceWireIFDummy: receive called");
    buffer->push_back(0x00);
  }
  void send(uint8_t* data, size_t length, SpaceWireEOPMarker::EOPType) override {
    std::println("SpaceWireIFDummy: send called");

    for (size_t i = 0; i < length; ++i) {
      std::println("Sending byte: {:#2X}", data[i]);
    }

    std::exit(1);
  }
  void setTxLinkRate(uint32_t) override { std::println("SpaceWireIFDummy: setTxLinkRate called"); };
  auto getTxLinkRateType() -> uint32_t override {
    std::println("SpaceWireIFDummy: getTxLinkRateType called");
    return 10;
  }
  void emitTimecode(uint8_t, uint8_t) override {
    std::println("SpaceWireIFDummy: emitTimecode called");
  }
  void setTimeoutDuration(double) override {
    std::println("SpaceWireIFDummy: setTimeoutDuration called");
  }
  void cancelReceive() override { std::println("SpaceWireIFDummy: cancelReceive called"); }
};

int main() {
  SpaceWireIFDummy spwif;
  spwif.open();
  RMAPEngine rmap_engine(&spwif);
  RMAPInitiator initiator(&rmap_engine);

  RMAPTargetNode node;
  node.setInitiatorLogicalAddress(0xFE);
  node.setTargetLogicalAddress(0x35);
  node.setTargetSpaceWireAddress({0x01, 0x02, 0x03});
  node.setReplyAddress({0x01, 0x02, 0x03, 0x04, 0x05, 0x06});
  node.setDefaultKey(0x20);
  std::vector<uint8_t> data = {};

  rmap_engine.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  try {
    initiator.setInitiatorLogicalAddress(0xFE);
    initiator.setTransactionID(0xd737);
    initiator.setIncrementMode(false);
    initiator.read(&node, 0x51525354, 0x654321, data.data());
  } catch (RMAPInitiatorException& e) {
    std::println("RMAP Initiator Exception: {}", e.toString());
    std::exit(1);
  } catch (RMAPTargetNodeException& e) {
    std::println("RMAP Target Node Exception: {}", e.toString());
    std::exit(1);
  } catch (CxxUtilities::Exception& e) {
    std::println("CxxUtilities Exception: {}", e.toString());
    std::exit(1);
  } catch (...) {
    std::println("An unexpected error occurred.");
    std::exit(1);
  }

  // initiator.getCommandPacketPointer();

  std::println("This is a legacy SpaceWire RMAP example.");
}
