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
#include <cmath>

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
enum class EntityType : uint8_t {
    Character = 0,
    Projectile = 1
};

struct EntityState {
    uint32_t id = 0;
    EntityType type = EntityType::Character;
    int32_t pos_x = 0; // fixed-point
    int32_t pos_y = 0;
    int32_t vel_x = 0; // fixed-point units PER SIMULATION TICK
    int32_t vel_y = 0;
    int32_t health = 0;
    int32_t radius = 0;
    int32_t lifetime_ticks = -1; // -1 = infinite; >0 = how many life ticks
    uint8_t flags = 0;

    constexpr EntityState() = default; // allows for compile-time instances of struct

    // Callbacks (simplified for demo)
    // ex.: std::string on_hit_callback; 

    constexpr bool operator==(EntityState const& o) const {
        return id == o.id &&
               pos_x == o.pos_x &&
               pos_y == o.pos_y &&
               vel_x == o.vel_x &&
               vel_y == o.vel_y &&
               health == o.health &&
               flags == o.flags &&
               type == o.type;
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
    
    // Friction to character only. Projectiles fly constantly
    if (e.type == EntityType::Character) {
        e.vel_x = approach_zero(e.vel_x, FRICTION_PER_TICK);
        e.vel_y = approach_zero(e.vel_y, FRICTION_PER_TICK);
    }

    if (e.lifetime_ticks > 0) {
        e.lifetime_ticks--;
    }
}

// ---------- Demo server class ----------
class DemoServer {
private:
    static DemoServer* = s_instance;

    uint32_t server_tick;
    uint32_t next_entity_id; // ID Generator
    std::unordered_map<uint32_t, EntityState> entities;
    std::unordered_map<uint32_t, InputQueue> input_queues;
    std::unordered_map<uint32_t, EntityState> prev_snapshot_map; // For delta calc
    std::unordered_map<uint32_t, EntityState> prev_snapshot_map_before_tick;

public:
    DemoServer() : server_tick(0), next_entity_id(1001) {
        s_instance = this;

        // First char
        EntityState e;
        e.id = next_entity_id++;
        e.type = EntityType::Character;
        e.pos_x = to_fixed(0.0f);
        e.pos_y = to_fixed(0.0f);
        e.vel_x = 0;
        e.vel_y = 0;
        e.health = 100;
        e.flags = 0;
        e.radius = to_fixed(0.5f);
        entities[e.id] = e;
        prev_snapshot_map.clear();
    }

    ~DemoServer() {
        if (s_instance == this) s_instance = nullptr;
    }

    // In a real engine, this would probably be connected to a UDP socket listener
    // Singleton Accessor
    static DemoServer* GetInstance() { return s_instance; }
    bool receiveInput(ClientInput const& in) {
        // ensure a queue exists for this client
        auto &q = input_queues[in.client_id];
        return q.push(in);
    }

    // Gameplay API

    // it = entities.find(id) which is key
    // second... = specific value
    // map has KEY -> VALUE

    bool GetPosition(int id, float &x, float &y) {
        auto it = entities.find(id);
        if (it == entities.end()) return false;
        x = to_world(it->second.pos_x);
        y = to_world(it->second.pos_y);
        return true;
    }

    bool SetMovement(int id, float vx, float vy) {
        auto it = entities.find(id);
        if (it == entities.end()) return false;
        // world-units/sec to fixed-units/tick
        float ticks_per_sec = (float)SERVER_TICK_RATE;
        it->second.vel_x = to_fixed(vx / ticks_per_sec);
        it->second.vel_y = to_fixed(vy / ticks_per_sec);
        return true;
    }

    bool ApplyDamage(int source_id, int target_id, int amount, const char* damage_type) {
        auto it = entities.find(target_id);
        if (it == entities.end()) return false;
        
        it->second.health -= amount;
        if (it->second.health < 0) it->second.health = 0;
        std::cout << "[Gameplay] Entity " << target_id << " took " << amount << " dmg (" << damage_type << ") from Entity " << source_id << "\n";
        return true;
    }

    bool ApplyKnockback(int source_id, int target_id, float dir_x, float dir_y, float force, float duration) {
        auto it = entities.find(target_id);
        if (it == entities.end()) return false;

        float ticks_per_sec = (float)SERVER_TICK_RATE;
        
        float len = std::sqrt(dir_x*dir_x + dir_y*dir_y);
        if(len > 0) { dir_x /= len; dir_y /= len; }

        int32_t fx = to_fixed((dir_x * force) / ticks_per_sec);
        int32_t fy = to_fixed((dir_y * force) / ticks_per_sec);

        it->second.vel_x += fx;
        it->second.vel_y += fy;

        std::cout << "[Gameplay] Knockback applied to " << target_id << " by " << source_id 
                  << " | Force: " << force << " | Duration: " << duration << "s\n";
        return true;
    }

    int SpawnProjectile(int caster_id, float x, float y, float dx, float dy, float speed, float radius, float life_time) {
        EntityState proj;
        proj.id = next_entity_id++;
        proj.type = EntityType::Projectile;
        proj.pos_x = to_fixed(x);
        proj.pos_y = to_fixed(y);
        
        // Speed is units/sec
        float ticks_per_sec = (float)SERVER_TICK_RATE;
        float vel_per_tick = speed / ticks_per_sec;
        
        proj.vel_x = to_fixed(dx * vel_per_tick);
        proj.vel_y = to_fixed(dy * vel_per_tick);
        
        proj.radius = to_fixed(radius);
        
        if (life_time > 0) {
            proj.lifetime_ticks = static_cast<int32_t>(life_time * ticks_per_sec);
        }

        entities[proj.id] = proj;
        
        std::cout << "[Gameplay] Spawned Projectile " << proj.id << " at " << x << "," << y << "\n";
        return (int)proj.id;
    }

    // Run single tick
    Snapshot tick() {
        // 1) gather all inputs for current tick for all clients and apply
        for (auto &kv : input_queues) { // kv = key-value || kv.first is key, kv.second is value
            uint32_t client = kv.first;
            InputQueue &q = kv.second;
            auto inputs = q.popForTick(server_tick);
            // For demo: map client_id to entity_id directly (hardcode)
            // Hardcoded client 1 controls entity 1001
            uint32_t ent_id = 1001;
            auto it = entities.find(ent_id);
            if (it != entities.end() && !inputs.empty()) {
                applyInputsToEntity(it->second, inputs);
            }
        }

        // 2) simulate physics & logic for all entities
        std::vector<uint32_t> to_remove;
        for (auto &kv : entities) {
            simulateEntityTick(kv.second);

            // lifetime check
            if (kv.second.type == EntityType::Projectile && kv.second.lifetime_ticks == 0) {
                 to_remove.push_back(kv.first);
            }
        }

        for(auto id : to_remove) entities.erase(id);

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

    // helper to get last-sent snapshot
    std::unordered_map<uint32_t, EntityState> const& getPrevBefore() const { return prev_snapshot_map_before_tick; }
    
    // For demo: we keep prev_snapshot_map_before_tick as last-sent snapshot map (so delta is computed vs last)
    void updatePrevBeforeFromSnapshot(Snapshot const& s) {
        prev_snapshot_map_before_tick.clear();
        for (auto const& e : s.entities) prev_snapshot_map_before_tick[e.id] = e;
    }

};

// ---------- Extern engine functions ----------
// These act as the bridge between Lua (C-style) and DemoServer (C++ Class)

extern "C" { // Use C linkage if Lua library was compiled as C, usually C++ is fine but this is safer

bool Engine_GetPosition(int entity_id, float &out_x, float &out_y) {
    if (!DemoServer::GetInstance()) return false;
    return DemoServer::GetInstance()->GetPosition(entity_id, out_x, out_y);
}

bool Engine_SetMovement(int entity_id, float vx, float vy) {
    if (!DemoServer::GetInstance()) return false;
    return DemoServer::GetInstance()->SetMovement(entity_id, vx, vy);
}

bool Engine_ApplyDamage(int source_id, int target_id, int amount, const char* damage_type) {
    if (!DemoServer::GetInstance()) return false;
    return DemoServer::GetInstance()->ApplyDamage(source_id, target_id, amount, damage_type);
}

bool Engine_ApplyKnockback(int source_id, int target_id, float dir_x, float dir_y, float force, float duration) {
    if (!DemoServer::GetInstance()) return false;
    return DemoServer::GetInstance()->ApplyKnockback(source_id, target_id, dir_x, dir_y, force, duration);
}

int Engine_SpawnProjectile(int caster_id, float spawn_x, float spawn_y, float dir_x, float dir_y, float speed, float radius, float life_time, const char* on_hit_cb) {
    if (!DemoServer::GetInstance()) return -1;
    return DemoServer::GetInstance()->SpawnProjectile(caster_id, spawn_x, spawn_y, dir_x, dir_y, speed, radius, life_time);
}

}

// ---------- Demo main: simulate a few ticks with synthetic inputs ----------
// EXPECTED RESULTS
// Tick 0 = full and delta are the same
// Ticks 1-9 = delta is way smaller than full
// Ticks 10-39 = delta is even smaller as there is nothing changing
int main() {
    using namespace std::chrono;
    DemoServer server; // This constructor sets s_instance

    std::cout << "Server started.\n";

    // Lua/Engine API Test
    std::cout << "--- API Test: Spawning Projectile ---\n";
    int proj_id = Engine_SpawnProjectile(1001, 0.0, 0.0, 1.0, 0.0, 10.0, 0.5, 2.0, "explode");
    Engine_ApplyKnockback(proj_id, 1001, -1.0, 0.0, 5.0, 0.2);

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
    }

    // We'll run 40 ticks and show snapshots
    Snapshot last_sent_snap; // empty for tick 0
    bool has_last = false;

    // run sim
    uint64_t tick_ns = TICK_NS;
    auto next_tick_time = steady_clock::now();

    for (uint32_t i = 0; i < 40; ++i) {
        next_tick_time += nanoseconds(tick_ns);
        std::this_thread::sleep_until(next_tick_time);

        // fails only on tick 0
        if (has_last) server.updatePrevBeforeFromSnapshot(last_sent_snap);

        Snapshot snap = server.tick();

        // Print status
        for (auto const& e : snap.entities) {
            std::cout << "[Tick " << snap.server_tick << "] Entity ID " << e.id
                      << " Type: " << (e.type == EntityType::Character ? "PLR" : "PRJ")
                      << " pos=(" << to_world(e.pos_x) << "," << to_world(e.pos_y) << ")"
                      << " vel=(" << to_world(e.vel_x) << "," << to_world(e.vel_y) << ")\n";

        auto full = serializeFull(snap);
        auto delta = serializeDelta(snap, server.getPrevBefore());
        std::cout << "  Serialized: full=" << full.size() << " bytes, delta=" << delta.size() << " bytes\n";

        last_sent_snap = snap;
        has_last = true;
        }
    }
    std::cout << "Demo finished.\n";
    return 0;
}
