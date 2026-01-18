// Quick test to check KeySlot serialization round-trip
#include "src/core/MultiUserTypes.h"
#include <iostream>

using namespace KeepTower;

int main() {
    // Create a KeySlot similar to the test
    KeySlot slot1;
    slot1.active = true;
    slot1.username = "admin";
    slot1.wrapped_dek.fill(0xAA);
    slot1.salt.fill(0xBB);

    // Serialize
    auto serialized = slot1.serialize();
    std::cout << "Serialized size: " << serialized.size() << " bytes\n";

    // Deserialize
    auto result = KeySlot::deserialize(serialized, 0);

    if (result.has_value()) {
        auto& [slot2, bytes_consumed] = result.value();
        std::cout << "Deserialization successful!\n";
        std::cout << "Bytes consumed: " << bytes_consumed << "\n";
        std::cout << "Username: " << slot2.username << "\n";
        std::cout << "Active: " << (slot2.active ? "true" : "false") << "\n";
        std::cout << "username_hash_size: " << (int)slot2.username_hash_size << "\n";
        return 0;
    } else {
        std::cout << "Deserialization FAILED!\n";
        return 1;
    }
}
