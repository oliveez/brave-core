// Copyright 2023 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

import BraveShields
import Data
import Foundation
import Preferences

@MainActor
class ShieldsSettingsViewModel: ObservableObject {
  private var domain: Domain
  private let tab: Tab

  @Published var stats: TPPageStats
  @Published var shieldsEnabled: Bool {
    didSet {
      domain.shield_allOff = NSNumber(booleanLiteral: !shieldsEnabled)
    }
  }
  @Published var blockAdsAndTrackingLevel: ShieldLevel {
    didSet {
      domain.domainBlockAdsAndTrackingLevel = blockAdsAndTrackingLevel
    }
  }
  @Published var blockScripts: Bool {
    didSet {
      domain.shield_noScript = NSNumber(booleanLiteral: blockScripts)
    }
  }
  @Published var fingerprintProtection: Bool {
    didSet {
      domain.shield_fpProtection = NSNumber(booleanLiteral: fingerprintProtection)
    }
  }
  @Published var lockdownModeDomainDisable: Bool {
    didSet {
      domain.lockdown_disabled = NSNumber(booleanLiteral: lockdownModeDomainDisable)
    }
  }
  @Published var deviceLockedDown: Bool = Preferences.Shields.deviceLockdownStatus.value

  var isPrivateBrowsing: Bool {
    return tab.isPrivate
  }

  init(tab: Tab, domain: Domain) {
    self.domain = domain
    self.tab = tab
    self.shieldsEnabled = !domain.areAllShieldsOff
    self.blockAdsAndTrackingLevel = domain.domainBlockAdsAndTrackingLevel
    self.blockScripts = domain.isShieldExpected(.noScript, considerAllShieldsOption: true)
    self.fingerprintProtection = domain.isShieldExpected(
      .fpProtection,
      considerAllShieldsOption: true
    )
    self.stats = tab.contentBlocker.stats
    // Lockdown Mode is always enabled when lockdown_disabled in domain is nil or false
    self.lockdownModeDomainDisable = domain.domainLockdownDisable
    Preferences.Shields.deviceLockdownStatus.observe(from: self)
    tab.contentBlocker.statsDidChange = { [weak self] _ in
      self?.stats = tab.contentBlocker.stats
    }
  }
}

extension ShieldsSettingsViewModel: PreferencesObserver {
  func preferencesDidChange(for key: String) {
    if key == Preferences.Shields.deviceLockdownStatus.key {
      deviceLockedDown = Preferences.Shields.deviceLockdownStatus.value
    }
  }
}
