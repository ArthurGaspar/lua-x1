#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include <cassert>
#include "../client_input.h"
#include "../entity_state.h"

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
//  CLIENT INPUT PACKET
// --------------------------------------------------------------
struct ClientInputPacket
{
    PacketHeader header;
    uint8_t inputCount;      // number of inputs
    ClientInput inputs[32];

    ClientInputPacket() : inputCount(0)
    {
        header.type = PacketType::CLIENT_INPUT;
        header.size = sizeof(ClientInputPacket);
    }

    void addInput(const ClientInput& i)
    {
        if (inputCount < 32)
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

        if (A == B) continue; 

        uint16_t entityMask = 0;
        if (A.pos_x != B.pos_x)   entityMask |= (1 << 0);
        if (A.pos_y != B.pos_y)   entityMask |= (1 << 1);
        if (A.vel_x != B.vel_x)   entityMask |= (1 << 2);
        if (A.vel_y != B.vel_y)   entityMask |= (1 << 3);
        if (A.health != B.health) entityMask |= (1 << 4);
        if (A.status_flags != B.status_flags)   entityMask |= (1 << 5);
        if (A.type != B.type)     entityMask |= (1 << 6);
        if (A.id != B.id)         entityMask |= (1 << 7);
        if (A.radius != B.radius) entityMask |= (1 << 8);
        // lifetime_ticks has not been added here for now

        w.write((uint16_t)i);
        w.write(entityMask);

        if (entityMask & (1 << 0)) w.write(B.pos_x);
        if (entityMask & (1 << 1)) w.write(B.pos_y);
        if (entityMask & (1 << 2)) w.write(B.vel_x);
        if (entityMask & (1 << 3)) w.write(B.vel_y);
        if (entityMask & (1 << 4)) w.write(B.health);
        if (entityMask & (1 << 5)) w.write(B.status_flags);
        if (entityMask & (1 << 6)) w.write(B.type);
        if (entityMask & (1 << 7)) w.write(B.id);
        if (entityMask & (1 << 8)) w.write(B.radius);

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

