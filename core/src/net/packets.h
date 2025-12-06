#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include <cassert>

#pragma pack(push, 1)

// --------------------------------------------------------------
//  PACKET TYPES
// --------------------------------------------------------------
enum class PacketType : uint8_t
{
    INVALID = 0,
    CLIENT_INPUT = 1,       // Client -> Server
    SERVER_SNAPSHOT = 2,    // Server -> Client (full snapshot)
    SERVER_SNAPSHOT_DELTA = 3 // Server -> Client (delta snapshot)
};

// --------------------------------------------------------------
//  PACKET HEADER
// --------------------------------------------------------------
struct PacketHeader
{
    uint16_t size;        // Total packet size in bytes
    PacketType type;      // Packet Type
    uint32_t tick;        // Relevant simulation tick (authoritative)

    PacketHeader(PacketType t = PacketType::INVALID, uint32_t tick_ = 0)
        : size(0), type(t), tick(tick_) {}
};

// --------------------------------------------------------------
//  SERIALIZATION BUFFER HELPERS
// --------------------------------------------------------------
struct BufferWriter
{
    uint8_t* data;
    size_t capacity;
    size_t offset;

    BufferWriter(uint8_t* d, size_t cap) : data(d), capacity(cap), offset(0) {}

    template<typename T>
    void write(const T& value)
    {
        assert(offset + sizeof(T) <= capacity);
        memcpy(data + offset, &value, sizeof(T));
        offset += sizeof(T);
    }

    void writeRaw(const void* src, size_t bytes)
    {
        assert(offset + bytes <= capacity);
        memcpy(data + offset, src, bytes);
        offset += bytes;
    }
};

struct BufferReader
{
    const uint8_t* data;
    size_t size;
    size_t offset;

    BufferReader(const uint8_t* d, size_t s) : data(d), size(s), offset(0) {}

    template<typename T>
    T read()
    {
        assert(offset + sizeof(T) <= size);
        T v;
        memcpy(&v, data + offset, sizeof(T));
        offset += sizeof(T);
        return v;
    }

    void readRaw(void* dst, size_t bytes)
    {
        assert(offset + bytes <= size);
        memcpy(dst, data + offset, bytes);
        offset += bytes;
    }
};

// --------------------------------------------------------------
//  BASIC ENTITY STATE
// --------------------------------------------------------------
struct EntityState
{
    uint32_t id;
    float x;
    float y;
    float vx;
    float vy;

    bool operator!=(const EntityState& other) const
    {
        return id != other.id ||
            x != other.x ||
            y != other.y ||
            vx != other.vx ||
            vy != other.vy;
    }
};

// --------------------------------------------------------------
//  CLIENT INPUT PACKET
// --------------------------------------------------------------
struct ClientInputPacket
{
    PacketHeader header;
    uint8_t inputCount;      // number of inputs
    uint8_t inputs[256];     // raw input bytes (max 256)

    ClientInputPacket() : inputCount(0)
    {
        header.type = PacketType::CLIENT_INPUT;
        header.size = sizeof(ClientInputPacket);
    }

    void addInput(uint8_t i)
    {
        if (inputCount < 256)
            inputs[inputCount++] = i;
    }
};

// --------------------------------------------------------------
//  FULL SNAPSHOT PACKET (SERVER -> CLIENT)
// --------------------------------------------------------------
struct SnapshotPacket
{
    PacketHeader header;
    uint16_t entityCount;

    // Entities follow directly in memory (packed)
    // entityCount * sizeof(EntityState)

    SnapshotPacket() : entityCount(0)
    {
        header.type = PacketType::SERVER_SNAPSHOT;
    }

    size_t computeSize() const
    {
        return sizeof(PacketHeader) +
            sizeof(uint16_t) +
            entityCount * sizeof(EntityState);
    }
};

// --------------------------------------------------------------
//  DELTA SNAPSHOT PACKET
// --------------------------------------------------------------
struct SnapshotDeltaPacket
{
    PacketHeader header;
    uint16_t entityCount;
    uint32_t changedMask; // bitmask: 1 bit per field

    // For each entity, only changed fields are written

    SnapshotDeltaPacket() : entityCount(0), changedMask(0)
    {
        header.type = PacketType::SERVER_SNAPSHOT_DELTA;
    }
};

#pragma pack(pop)


// --------------------------------------------------------------
//  SERIALIZATION OF SNAPSHOTS
// --------------------------------------------------------------
inline void serializeSnapshot(
    const std::vector<EntityState>& entities,
    uint8_t* outBuffer,
    size_t outCapacity,
    size_t& outSize,
    uint32_t tick)
{
    BufferWriter w(outBuffer, outCapacity);

    SnapshotPacket pkt;
    pkt.entityCount = (uint16_t)entities.size();
    pkt.header.tick = tick;
    pkt.header.size = (uint16_t)pkt.computeSize();

    w.write(pkt.header);
    w.write(pkt.entityCount);

    for (const auto& e : entities)
        w.write(e);

    outSize = w.offset;
}

inline void serializeDeltaSnapshot(
    const std::vector<EntityState>& before,
    const std::vector<EntityState>& after,
    uint8_t* outBuffer,
    size_t outCapacity,
    size_t& outSize,
    uint32_t tick)
{
    assert(before.size() == after.size());
    BufferWriter w(outBuffer, outCapacity);

    size_t headerOffset = w.offset;
    SnapshotDeltaPacket pkt;
    pkt.entityCount = 0;
    pkt.header.tick = tick;

    w.write(pkt.header);
    w.write(pkt.entityCount);

    for (size_t i = 0; i < before.size(); ++i)
    {
        const auto& A = before[i];
        const auto& B = after[i];

        uint8_t entityMask = 0;

        if (A.x != B.x)   entityMask |= (1 << 0);
        if (A.y != B.y)   entityMask |= (1 << 1);
        if (A.vx != B.vx) entityMask |= (1 << 2);
        if (A.vy != B.vy) entityMask |= (1 << 3);
        if (A.id != B.id) entityMask |= (1 << 4);

        if (entityMask == 0) {
            continue; 
        }

        w.write((uint16_t)i);
        w.write(entityMask);

        // Bit layout:
        // bit 0 = x changed
        // bit 1 = y changed
        // bit 2 = vx changed
        // bit 3 = vy changed
        // bit 4 = id changed  (rare)
        // With more fields this should be extended, as well as the size of bit

        if (entityMask & (1 << 0)) w.write(B.x);
        if (entityMask & (1 << 1)) w.write(B.y);
        if (entityMask & (1 << 2)) w.write(B.vx);
        if (entityMask & (1 << 3)) w.write(B.vy);
        if (entityMask & (1 << 4)) w.write(B.id);

        pkt.entityCount++;
    }

    size_t finalOffset = w.offset;
    w.offset = headerOffset;
    
    // Update Packet struct details
    pkt.header.size = (uint16_t)finalOffset;
    
    // Re-write the header data with correct count
    w.write(pkt.header);
    w.write(pkt.entityCount); // Now contains actual number of changed entities

    outSize = finalOffset;
}

