// Property and fuzz test of the wire protocol: the parser must never read out of bounds or crash on any bytes, only valid
// packets may come out, serialize(parse(x)) must give x back, and the CRC-16 must catch every one-bit and two-bit error.
// Build with the sanitizers: test/run_fuzz_protocol_tests.sh (fixed seed, so a failure repeats).
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

#include "Crc16.h"
#include "Packet.h"
#include "Parser.h"
#include "Serializer.h"
#include "Validator.h"

using namespace terrawatch::protocol;
static int failures = 0;
#define CHECK(c, ...) do { if (!(c)) { ++failures; std::printf("FAIL %s:%d ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); if (failures > 20) std::exit(1); } } while (0)

int main() {
    std::mt19937_64 rng(20261111);
    long parsed = 0, rejected = 0, roundtrip = 0, mutants = 0, mutants_accepted_same = 0;

    // 1. random bytes of every length 0..80 (the packet limit is 12 + 48 + 2 = 62): never crash, valid output only
    for (int n = 0; n < 400000; ++n) {
        std::vector<std::uint8_t> buf(rng() % 81); for (auto &b : buf) b = static_cast<std::uint8_t>(rng());
        Packet p; if (Parser::parse(buf.data(), buf.size(), p)) { ++parsed; CHECK(Validator::isValid(p), "parser returned an invalid packet"); CHECK(p.payloadSize <= terrawatch::constants::kMaxPayloadSize, "payload too big"); } else ++rejected;
    }

    // 2. valid packets: build, serialize, parse, compare; then flip bits
    for (int n = 0; n < 200000; ++n) {
        Packet p; p.type = static_cast<PacketType>(1 + rng() % 6); p.sourceId = 1 + rng() % 253; p.destinationId = 1 + rng() % 254; p.previousHopId = 1 + rng() % 254; p.nextHopId = rng() % 256;
        p.sequenceNumber = static_cast<std::uint16_t>(rng()); p.ttl = 1 + rng() % 16; p.hopCount = rng() % 8;
        std::array<std::uint8_t, 48> pay; for (auto &b : pay) b = static_cast<std::uint8_t>(rng()); p.setPayload(pay.data(), rng() % 49);
        std::array<std::uint8_t, terrawatch::constants::kMaxPacketSize> out{}; std::size_t size = 0;
        if (!Serializer::serialize(p, out, size)) continue;
        Packet q; bool ok = Parser::parse(out.data(), size, q);
        CHECK(ok, "a serialized packet did not parse");
        if (!ok) continue;
        ++roundtrip; CHECK(q.type == p.type && q.sourceId == p.sourceId && q.destinationId == p.destinationId && q.previousHopId == p.previousHopId && q.nextHopId == p.nextHopId && q.sequenceNumber == p.sequenceNumber && q.ttl == p.ttl && q.hopCount == p.hopCount && q.payloadSize == p.payloadSize && std::memcmp(q.payload.data(), p.payload.data(), p.payloadSize) == 0, "round trip changed a field");
        if (n % 20 != 0) continue;
        // every single-bit flip and every double-bit flip in the frame must be rejected or give a packet that differs only where the CRC cannot see
        for (std::size_t bit = 0; bit < size * 8; ++bit) {
            std::vector<std::uint8_t> m(out.begin(), out.begin() + size); m[bit / 8] ^= 1u << (bit % 8); Packet r; ++mutants;
            if (Parser::parse(m.data(), m.size(), r)) { ++mutants_accepted_same; CHECK(false, "one-bit error accepted (bit %zu of %zu)", bit, size * 8); }
        }
        for (int k = 0; k < 40; ++k) {
            std::vector<std::uint8_t> m(out.begin(), out.begin() + size); std::size_t a = rng() % (size * 8), b = rng() % (size * 8); if (a == b) continue; m[a / 8] ^= 1u << (a % 8); m[b / 8] ^= 1u << (b % 8); Packet r; ++mutants;
            if (Parser::parse(m.data(), m.size(), r)) CHECK(false, "two-bit error accepted (bits %zu and %zu of %zu)", a, b, size * 8);
        }
        // truncated and extended frames
        for (std::size_t cut = 0; cut < size; ++cut) { Packet r; CHECK(!Parser::parse(out.data(), cut, r), "a truncated frame (%zu of %zu bytes) was accepted", cut, size); }
        std::vector<std::uint8_t> longer(out.begin(), out.begin() + size); longer.push_back(0); Packet r; CHECK(!Parser::parse(longer.data(), longer.size(), r), "a frame with a trailing byte was accepted");
    }

    // 3. CRC-16/CCITT check value of "123456789" is 0x29B1 (the standard test vector)
    const char *v = "123456789"; CHECK(terrawatch::utils::Crc16::computeCcitt(reinterpret_cast<const std::uint8_t *>(v), 9) == 0x29B1, "CRC-16/CCITT-FALSE check value");

    std::printf("random: %ld parsed, %ld rejected; round trips %ld; bit-error trials %ld (accepted %ld); failures %d\n", parsed, rejected, roundtrip, mutants, mutants_accepted_same, failures);
    return failures ? 1 : 0;
}
