/* Copyright (c) 2023 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_ads/core/internal/common/unittest/unittest_local_state_pref_storage.h"

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "brave/components/brave_ads/core/internal/common/unittest/unittest_current_test_util.h"
#include "brave/components/brave_ads/core/internal/common/unittest/unittest_pref_info.h"

namespace brave_ads {

namespace {

base::flat_map</*uuid=*/std::string, PrefInfo>& LocalStatePrefStorage() {
  static base::NoDestructor<base::flat_map<std::string, PrefInfo>> prefs;
  return *prefs;
}

}  // namespace

PrefInfo& LocalStatePref(const std::string& path) {
  const std::string uuid = test::GetUuidForCurrentTestAndValue(path);
  return LocalStatePrefStorage()[uuid];
}

bool HasLocalStatePref(const std::string& path) {
  return base::Contains(LocalStatePrefStorage(),
                        test::GetUuidForCurrentTestAndValue(path));
}

}  // namespace brave_ads
