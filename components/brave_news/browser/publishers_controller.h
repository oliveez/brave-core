// Copyright (c) 2021 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BRAVE_COMPONENTS_BRAVE_NEWS_BROWSER_PUBLISHERS_CONTROLLER_H_
#define BRAVE_COMPONENTS_BRAVE_NEWS_BROWSER_PUBLISHERS_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/one_shot_event.h"
#include "brave/components/api_request_helper/api_request_helper.h"
#include "brave/components/brave_news/common/brave_news.mojom.h"
#include "brave/components/brave_news/common/locales_helper.h"

namespace brave_news {

using GetPublishersCallback = mojom::BraveNewsController::GetPublishersCallback;

class SubscriptionsSnapshot;

class PublishersController {
 public:
  explicit PublishersController(
      api_request_helper::APIRequestHelper* api_request_helper);
  ~PublishersController();
  PublishersController(const PublishersController&) = delete;
  PublishersController& operator=(const PublishersController&) = delete;

  // The following methods return a pointer to a |Publisher|. This pointer is
  // not safe to hold onto, as the object it points to will be destroyed when
  // the publishers are updated (which happens regularly). If you need it to
  // live longer, take a clone.
  const mojom::Publisher* GetPublisherForSite(const GURL& site_url) const;
  const mojom::Publisher* GetPublisherForFeed(const GURL& feed_url) const;
  const Publishers& GetLastPublishers() const;

  void GetOrFetchPublishers(const SubscriptionsSnapshot& subscriptions,
                            GetPublishersCallback callback,
                            bool wait_for_current_update = false);
  void GetLocale(const SubscriptionsSnapshot& subscriptions,
                 mojom::BraveNewsController::GetLocaleCallback);
  const std::string& GetLastLocale() const;
  void EnsurePublishersIsUpdating(const SubscriptionsSnapshot& subscriptions);
  void ClearCache();

 private:
  void GetOrFetchPublishers(const SubscriptionsSnapshot& subscriptions,
                            base::OnceClosure callback,
                            bool wait_for_current_update);
  void UpdateDefaultLocale();

  raw_ptr<api_request_helper::APIRequestHelper> api_request_helper_;

  std::unique_ptr<base::OneShotEvent> on_current_update_complete_;
  std::string default_locale_;
  Publishers publishers_;
  bool is_update_in_progress_ = false;
};

}  // namespace brave_news

#endif  // BRAVE_COMPONENTS_BRAVE_NEWS_BROWSER_PUBLISHERS_CONTROLLER_H_
