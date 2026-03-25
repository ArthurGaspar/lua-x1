## 🧠 **Networking Model**

* Transport: UDP (unreliable, low latency)
* Server: authoritative
* Simulation: deterministic (30 ticks/sec)
* Client sends **inputs**, not actions
* Server sends **snapshots**

---

## ⏱️ **Tick Synchronization**

* Server runs at **30 ticks/sec**
* Each packet includes:

```cpp
uint32_t tick;
```

Used for:

* input targeting
* client reconciliation
* delta application

---

## 🟦 Client → Server

```
Input (WASD / mouse / ability)
→ build ClientInput
→ send CLIENT_INPUT packet
→ server receives
→ push into InputQueue
```

---

## 🟥 Server → Client

```
tick()
→ simulate
→ generate snapshot
→ serializeDelta()
→ send packet
```

---