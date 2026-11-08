/*
 * SPDX-FileCopyrightText: 2026 KAS
 *
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef KASLIBCXX_SERIAL_HELPER_H
#define KASLIBCXX_SERIAL_HELPER_H

#include <cstdint>
#include <string>

namespace kas::serial::enums {
enum class BaudRate : std::uint16_t {
    B_0,
    B_50,
    B_75,
    B_110,
    B_134,
    B_150,
    B_200,
    B_300,
    B_600,
    B_1200,
    B_1800,
    B_2400,
    B_4800,
    B_9600,
    B_19200,
    B_38400,
    B_57600,
    B_115200,
    B_230400,
    B_460800,
    B_500000,
    B_576000,
    B_921600,
    B_1000000,
    B_1152000,
    B_1500000,
    B_2000000,
    B_2500000,
    B_3000000,
    B_3500000,
    B_4000000,
};
//------------------
enum class NumStopBits : std::uint8_t {
    One,
    Two,
};
//------------------
enum class NumDataBits : std::uint8_t {
    Five,
    Six,
    Seven,
    Eight,
};
//------------------
enum class Parity : std::uint8_t {
    None,
    Even,
    Odd,
};
//------------------
enum class SoftwareFlowControl : std::uint8_t {
    F_IXON,
    F_IXOFF,
    F_IXANY,
};
}

namespace kas::serial::structs {

//------------------

struct SerialConfig {
    enums::Parity parity = enums::Parity::None;
    enums::NumStopBits stop_bits = enums::NumStopBits::One;
    enums::NumDataBits data_bits = enums::NumDataBits::Eight;
    enums::SoftwareFlowControl software_flow_control_bits = enums::SoftwareFlowControl::F_IXOFF;
    bool raw_mode = true;
    bool rts_enabled = false;
    bool rts_invert = false;
    bool hardware_flow_control = false;
};
//------------------
struct PortName {
    std::string value;
};
//------------------
struct BaudRate {
    enums::BaudRate value;
};
//------------------
}

#endif