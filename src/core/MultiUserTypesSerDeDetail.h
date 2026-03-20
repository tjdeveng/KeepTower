// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "../utils/Log.h"

namespace KeepTower::MultiUserTypesSerDeDetail {

inline bool require_bytes(const std::vector<uint8_t>& data,
                          size_t pos,
                          size_t needed,
                          std::string_view what) {
    if (pos > data.size() || (data.size() - pos) < needed) {
        Log::error("{}: Insufficient data (need {}, have {})", what, needed,
                   (pos <= data.size() ? (data.size() - pos) : 0));
        return false;
    }
    return true;
}

} // namespace KeepTower::MultiUserTypesSerDeDetail
