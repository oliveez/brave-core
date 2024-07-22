/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_BRAVE_ADS_CORE_PUBLIC_HISTORY_AD_HISTORY_VALUE_UTIL_H_
#define BRAVE_COMPONENTS_BRAVE_ADS_CORE_PUBLIC_HISTORY_AD_HISTORY_VALUE_UTIL_H_

#include "base/values.h"
#include "brave/components/brave_ads/core/public/history/ad_history_item_info.h"

namespace brave_ads {

base::Value::Dict AdHistoryItemToValue(
    const AdHistoryItemInfo& ad_history_item);
AdHistoryItemInfo AdHistoryItemFromValue(const base::Value::Dict& dict);

base::Value::List AdHistoryToValue(const AdHistoryList& ad_history);
AdHistoryList AdHistoryFromValue(const base::Value::List& list);

base::Value::List AdHistoryToUIValue(const AdHistoryList& ad_history);

}  // namespace brave_ads

#endif  // BRAVE_COMPONENTS_BRAVE_ADS_CORE_PUBLIC_HISTORY_AD_HISTORY_VALUE_UTIL_H_
