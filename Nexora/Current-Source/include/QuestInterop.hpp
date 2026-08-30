/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Liam Sitbon
 *
 * Optional Quest mod interoperability through SongCore's public capability API.
 * This header contains no implementation copied from any peer mod.
 */
#pragma once

#include <string_view>

#include "songcore/shared/SongCore.hpp"

namespace QuestModInterop {

inline constexpr std::string_view kCinema = "Cinema";
inline constexpr std::string_view kNexora = "Nexora";
inline constexpr std::string_view kNoodleExtensions = "Noodle Extensions";
inline constexpr std::string_view kVivify = "Vivify";

struct PeerSet {
  bool cinema = false;
  bool nexora = false;
  bool noodleExtensions = false;
  bool vivify = false;
};

inline PeerSet InstalledPeers() {
  return {
      .cinema = SongCore::API::Capabilities::IsCapabilityRegistered(kCinema),
      .nexora = SongCore::API::Capabilities::IsCapabilityRegistered(kNexora),
      .noodleExtensions =
          SongCore::API::Capabilities::IsCapabilityRegistered(kNoodleExtensions),
      .vivify = SongCore::API::Capabilities::IsCapabilityRegistered(kVivify),
  };
}

template <typename Range>
PeerSet RequiredPeers(Range const& requirements) {
  PeerSet peers;
  for (auto const& requirement : requirements) {
    std::string_view const value(requirement);
    peers.cinema |= value == kCinema;
    peers.nexora |= value == kNexora;
    peers.noodleExtensions |= value == kNoodleExtensions;
    peers.vivify |= value == kVivify;
  }
  return peers;
}

struct MapContext {
  PeerSet installed;
  PeerSet required;

  bool CinemaShouldYieldToNexora() const {
    return required.nexora && !required.cinema;
  }
};

template <typename Range>
MapContext Inspect(Range const& requirements) {
  return {.installed = InstalledPeers(), .required = RequiredPeers(requirements)};
}

}  // namespace QuestModInterop
