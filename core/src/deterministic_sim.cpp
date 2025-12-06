// deterministic_sim.cpp
// Build: ./vendor/mingw64/bin/g++ -std=c++17 core/src/deterministic_sim.cpp -O2 -pthread -o core/src/deterministic_sim.exe
// (ON ROOT OF THE PROJECT)
//
// This demo shows:
// - fixed-step tick loop at 30 t/s
// - fixed-point integer entity state (pos/vel)
// - InputQueue (max 256) per client
// - snapshot and simple delta compression (change_mask, only changed fields sent)
//
// This is a prototype for local testing. Replace I/O with real network code later.

#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>
#include <deque>
#include <unordered_map>
#include <cstring>
#include <cassert>

// ---------- Config ----------
constexpr int SERVER_TICK_RATE = 30; // 30t/s
constexpr uint64_t TICK_NS = 1000000000ull / SERVER_TICK_RATE; // 33,333,333 ns (approx)
constexpr int32_t POS_SCALE = 1000; // fixed-point scale: 1.0 world unit = 1000 units
constexpr size_t MAX_CLIENT_INPUT_QUEUE = 256;

// ---------- Fixed-point helpers ----------
inline int32_t to_fixed(float world_units) {
    return static_cast<int32_t>(world_units * POS_SCALE);
}
inline float to_world(int32_t fixed) {
    return static_cast<float>(fixed) / POS_SCALE;
}

// ---------- Entity State ----------
struct EntityState {
    uint32_t id = 0;
    int32_t pos_x = 0; // fixed-point
    int32_t pos_y = 0;
    int32_t vel_x = 0; // fixed-point units PER SIMULATION TICK
    int32_t vel_y = 0;
    int32_t health = 0;
    uint8_t flags = 0;

    constexpr EntityState() = default; // allows for compile-time instances of struct

    constexpr bool operator==(EntityState const& o) const {
        return id == o.id &&
               pos_x == o.pos_x &&
               pos_y == o.pos_y &&
               vel_x == o.vel_x &&
               vel_y == o.vel_y &&
               health == o.health &&
               flags == o.flags;
    }

    constexpr bool operator!=(EntityState const& o) const {
        return !(*this == o);
    }
};

// ---------- Input ----------
struct ClientInput {
    uint32_t client_id;       // which client
    uint32_t input_seq;       // should always increase (1... 2... 3...)
    uint32_t target_tick;     // which server tick this input is for
    int8_t move_dx;           // -127..127 (signed). Interpreted as normalized direction * 127.
    int8_t move_dy;           // -127..127
    uint8_t action_flags;     // bitmask (attack, cast, etc.)
    uint16_t ability_id;      // optional: ability id
    int32_t target_x;         // optional: fixed-point target pos
    int32_t target_y;

    ClientInput()
        : client_id(0), input_seq(0), target_tick(0),
          move_dx(0), move_dy(0), action_flags(0), ability_id(0),
          target_x(0), target_y(0) {}
};

// ---------- InputQueue (circular, max 256) ----------
class InputQueue {
public:
    InputQueue() { }
    [[nodiscard]] bool push(ClientInput const& in) {
        if (queue.size() >= MAX_CLIENT_INPUT_QUEUE) return false;
        queue.push_back(in);
        return true;
    }

    std::vector<ClientInput> popForTick(uint32_t tick) {
        // FIFO order
        std::vector<ClientInput> res; // target tick (prio)
        std::deque<ClientInput> remaining; // other tick
        while (!queue.empty()) {
            ClientInput c = queue.front();
            queue.pop_front();
            if (c.target_tick == tick) res.push_back(c);
            else remaining.push_back(c);
        }
        queue.swap(remaining);
        return res;
    }

    size_t size() const { return queue.size(); }

private:
    std::deque<ClientInput> queue; // small, bounded by MAX_CLIENT_INPUT_QUEUE
};

// ---------- Snapshot / Delta serialization ----------
// In production we should use proper endian handling.

// change mask bits
enum ChangeBits : uint8_t {
    CH_PosX = 1 << 0, // << shifts position to the left n = 0 times
    CH_PosY = 1 << 1, // << shifts position to the left n = 1 times
    CH_VelX = 1 << 2, // ...
    CH_VelY = 1 << 3,
    CH_Health = 1 << 4,
    CH_Flags = 1 << 5
};
// << changes the bit position
// all properties are inside a same mask based on their positions
// 00010101 --> POSX, VELX and HEALTH changed (all are 1, and their positions are respective to the)

struct Snapshot {
    uint32_t server_tick;
    std::vector<EntityState> entities;
};

// helper endian functions
inline void write_u8(std::vector<uint8_t>& buf, uint8_t v) { buf.push_back(v); }
inline void write_u16(std::vector<uint8_t>& buf, uint16_t v) {
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}
inline void write_u32(std::vector<uint8_t>& buf, uint32_t v) {
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}
inline void write_i32(std::vector<uint8_t>& buf, int32_t v) { write_u32(buf, static_cast<uint32_t>(v)); }

// Serialize full snapshot, example
[[nodiscard]] std::vector<uint8_t> serializeFull(Snapshot const& snap) {
    std::vector<uint8_t> out;
    write_u32(out, snap.server_tick);
    write_u32(out, static_cast<uint32_t>(snap.entities.size()));
    for (auto const& e : snap.entities) {
        write_u32(out, e.id);
        write_i32(out, e.pos_x);
        write_i32(out, e.pos_y);
        write_i32(out, e.vel_x);
        write_i32(out, e.vel_y);
        write_i32(out, e.health);
        write_u8(out, e.flags);
    }
    return out;
}

// Serialize delta relative to previous snapshot (prev states provided in map), example
[[nodiscard]] std::vector<uint8_t> serializeDelta(Snapshot const& snap, std::unordered_map<uint32_t, EntityState> const& prev_map) {
    std::vector<uint8_t> out;
    write_u32(out, snap.server_tick);

    // this line stores the index where entity count will be written:
    size_t count_pos = out.size();
    write_u32(out, 0);

    uint32_t entity_count = 0;
    for (auto const& e : snap.entities) {
        auto it = prev_map.find(e.id);
        uint8_t mask = 0;
        if (it == prev_map.end()) {
            // new entity -> send everything
            mask = CH_PosX | CH_PosY | CH_VelX | CH_VelY | CH_Health | CH_Flags;
        } else {
            EntityState const& prev = it->second;
            if (e.pos_x != prev.pos_x) mask |= CH_PosX;
            if (e.pos_y != prev.pos_y) mask |= CH_PosY;
            if (e.vel_x != prev.vel_x) mask |= CH_VelX;
            if (e.vel_y != prev.vel_y) mask |= CH_VelY;
            if (e.health != prev.health) mask |= CH_Health;
            if (e.flags != prev.flags) mask |= CH_Flags;
            if (mask == 0) continue;
        }
        write_u32(out, e.id);
        write_u8(out, mask);
        if (mask & CH_PosX) write_i32(out, e.pos_x);
        if (mask & CH_PosY) write_i32(out, e.pos_y);
        if (mask & CH_VelX) write_i32(out, e.vel_x);
        if (mask & CH_VelY) write_i32(out, e.vel_y);
        if (mask & CH_Health) write_i32(out, e.health);
        if (mask & CH_Flags) write_u8(out, e.flags);

        ++entity_count;
    }

    // patch entity_count
    uint32_t le_count = entity_count;
    out[count_pos + 0] = static_cast<uint8_t>(le_count & 0xFF);
    out[count_pos + 1] = static_cast<uint8_t>((le_count >> 8) & 0xFF);
    out[count_pos + 2] = static_cast<uint8_t>((le_count >> 16) & 0xFF);
    out[count_pos + 3] = static_cast<uint8_t>((le_count >> 24) & 0xFF);

    return out;
}

// ---------- Simple deterministic "physics" & logic ----------
// For the demo: movement and simple velocity decay (friction). All integer arithmetic.

// Convert to velocity per tick in fixed-point world units.
// Suppose max_speed = 5.0 units/sec. We need vel per tick:
// vel_per_tick_fixed = round(max_speed * (1/TICK_RATE) * POS_SCALE * (normalized_dir / 127))
// We avoid floats by using integer arithmetic: use constants in fixed integer form.
constexpr int32_t MAX_SPEED_FIXED_PER_TICK = static_cast<int32_t>( (5.0f * POS_SCALE) / SERVER_TICK_RATE );
constexpr int32_t FRICTION_PER_TICK = 25; // 0.025 world units per tick drag

void applyInputsToEntity(EntityState &e, std::vector<ClientInput> const& inputs) {
    for (auto const& in : inputs) {
        if (in.move_dx != 0 || in.move_dy != 0) {
            int32_t nx = static_cast<int32_t>(in.move_dx); // -127..127
            int32_t ny = static_cast<int32_t>(in.move_dy);
            int32_t new_vx = (MAX_SPEED_FIXED_PER_TICK * nx) / 127;
            int32_t new_vy = (MAX_SPEED_FIXED_PER_TICK * ny) / 127;
            e.vel_x = new_vx;
            e.vel_y = new_vy;
        }
        // action_flags and ability casting would go here - for demo we ignore
    }
}

inline int32_t approach_zero(int32_t current_val, int32_t amount) {
    if (current_val > amount) return current_val - amount;
    if (current_val < -amount) return current_val + amount;
    return 0;
}

void simulateEntityTick(EntityState &e) {
    e.pos_x += e.vel_x;
    e.pos_y += e.vel_y;

    // simple friction: gradually reduce vel to zero deterministically
    e.vel_x = approach_zero(e.vel_x, FRICTION_PER_TICK);
    e.vel_y = approach_zero(e.vel_y, FRICTION_PER_TICK);
}

// ---------- Demo server class ----------
class DemoServer {
public:
    DemoServer() : server_tick(0) {
        // Single entity for this demo
        EntityState e;
        e.id = 1001;
        e.pos_x = to_fixed(0.0f);
        e.pos_y = to_fixed(0.0f);
        e.vel_x = 0;
        e.vel_y = 0;
        e.health = 100;
        e.flags = 0;
        entities[e.id] = e;
        prev_snapshot_map.clear();
    }

    // In a real engine, this would probably be connected to a UDP socket listener
    bool receiveInput(ClientInput const& in) {
        // ensure a queue exists for this client
        auto &q = input_queues[in.client_id];
        return q.push(in);
    }

    // Run single tick
    Snapshot tick() {
        // 1) gather all inputs for current tick for all clients and apply
        for (auto &kv : input_queues) { // kv = key-value || kv.first is key, kv.second is value
            uint32_t client = kv.first;
            InputQueue &q = kv.second;
            auto inputs = q.popForTick(server_tick);
            uint32_t ent_id = 1001; // ugly hardcode
            auto it = entities.find(ent_id);
            if (it != entities.end() && !inputs.empty()) {
                applyInputsToEntity(it->second, inputs);
            }
        }

        // 2) simulate physics & logic for all entities
        for (auto &kv : entities) {
            simulateEntityTick(kv.second);
        }

        // 3) produce snapshot
        Snapshot snap;
        snap.server_tick = server_tick;
        for (auto const& kv : entities) snap.entities.push_back(kv.second);

        // 4) update prev_snapshot_map for delta next time
        prev_snapshot_map.clear();
        for (auto const& kv : entities) prev_snapshot_map[kv.first] = kv.second;

        server_tick++;
        return snap;
    }

    // Serialize snapshots on two versions (full and delta) for demo and return sizes for comparison of their bytes.
    void serializeSnapshots(Snapshot const& snap) {
        auto full = serializeFull(snap);
        auto delta = serializeDelta(snap, prev_snapshot_map_before_tick); // compare with snapshot BEFORE this tick
        std::cout << "[Serialize] tick=" << snap.server_tick
                  << " full=" << full.size() << " bytes"
                  << " delta=" << delta.size() << " bytes"
                  << " (entities=" << snap.entities.size() << ")\n";
    }

private:
    uint32_t server_tick;
    std::unordered_map<uint32_t, EntityState> entities;
    std::unordered_map<uint32_t, InputQueue> input_queues;
    std::unordered_map<uint32_t, EntityState> prev_snapshot_map;
    std::unordered_map<uint32_t, EntityState> prev_snapshot_map_before_tick;

public:
    // helper to get last-sent snapshot
    std::unordered_map<uint32_t, EntityState> const& getPrevBefore() const { return prev_snapshot_map_before_tick; }
    
    // For demo: we keep prev_snapshot_map_before_tick as last-sent snapshot map (so delta is computed vs last)
    void updatePrevBeforeFromSnapshot(Snapshot const& s) {
        prev_snapshot_map_before_tick.clear();
        for (auto const& e : s.entities) prev_snapshot_map_before_tick[e.id] = e;
    }
};

// ---------- Demo main: simulate a few ticks with synthetic inputs ----------
// EXPECTED RESULTS
// Tick 0 = full and delta are the same
// Ticks 1-9 = delta is way smaller than full
// Ticks 10-39 = delta is even smaller as there is nothing changing
int main() {
    using namespace std::chrono;
    DemoServer server;

    // create synthetic client input sequence for client 1 to move right for 10 ticks
    // after tick 10, it does not move anymore
    uint32_t client_id = 1;
    uint32_t seq = 1;
    uint32_t start_tick = 0;
    for (uint32_t t = start_tick; t < start_tick + 10; ++t) {
        ClientInput in;
        in.client_id = client_id;
        in.input_seq = seq++;
        in.target_tick = t;
        // move right: dx = +127, dy = 0
        in.move_dx = 127;
        in.move_dy = 0;
        in.action_flags = 0;
        server.receiveInput(in);

        if (!server.receiveInput(in)) {
            std::cerr << "Warning: dropped input for tick " << t << "\n";
        }
    }

    // We'll run 40 ticks and show snapshots
    Snapshot last_sent_snap; // empty for tick 0
    bool has_last = false;

    uint64_t tick_ns = TICK_NS;
    auto next_tick_time = steady_clock::now();

    for (uint32_t i = 0; i < 40; ++i) {
        next_tick_time += nanoseconds(tick_ns);
        std::this_thread::sleep_until(next_tick_time);

        // fails only on tick 0
        if (has_last) server.updatePrevBeforeFromSnapshot(last_sent_snap);

        Snapshot snap = server.tick();

        for (auto const& e : snap.entities) {
            std::cout << "[Tick " << snap.server_tick << "] Entity " << e.id
                      << " pos=(" << to_world(e.pos_x) << "," << to_world(e.pos_y) << ")"
                      << " vel=(" << to_world(e.vel_x) << "," << to_world(e.vel_y) << ")\n";
        }

        auto full = serializeFull(snap);
        auto delta = serializeDelta(snap, server.getPrevBefore());
        std::cout << "  Serialized: full=" << full.size() << " bytes, delta=" << delta.size() << " bytes\n";

        last_sent_snap = snap;
        has_last = true;
    }
    std::cout << "Demo finished.\n";
    return 0;
}
