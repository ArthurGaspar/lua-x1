#pragma once
#include <cstdint>

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