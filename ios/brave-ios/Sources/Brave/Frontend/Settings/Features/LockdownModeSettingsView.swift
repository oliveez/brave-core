//
// Copyright 2024 Wan Fung, Poon.
//
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
//

import Data
import OSLog
import Preferences
import SwiftUI

struct LockdownModeSettingsView: View {
  @Environment(\.managedObjectContext) private var viewContext
  @Environment(\.editMode) private var editMode
  @State private var selection = Set<String>()
  private var isDeviceLockdown = Preferences.Shields.deviceLockdownStatus.value
  var tabManager: TabManager

  init(tabManager: TabManager) {
    self.tabManager = tabManager
  }

  @SectionedFetchRequest(
    entity: Domain.entity(),
    sectionIdentifier: \Domain.url!,
    sortDescriptors: [
      NSSortDescriptor(keyPath: \Domain.url, ascending: true)
    ],
    predicate: NSCompoundPredicate(
      type: .and,
      subpredicates: [
        NSPredicate(format: "lockdown_disabled != nil"),
        NSPredicate(format: "url != nil"),
      ]
    )
  )

  var fetchedDomains: SectionedFetchResults<String, Domain>

  @State private var searchQuery = ""

  var filteredSections: [SectionedFetchResults<String, Domain>.Section] {
    if searchQuery.isEmpty {
      return Array(fetchedDomains)
    } else {
      return fetchedDomains.filter { section in
        section.contains { domain in
          domain.url!.lowercased().contains(searchQuery.lowercased())
        }
      }
    }
  }

  var body: some View {
    VStack {
      List(selection: $selection) {
        ForEach(filteredSections) { section in
          ForEach(section) { domain in
            domainToggle(domain: domain)
          }
        }
      }
    }
    .searchable(
      text: $searchQuery,
      placement: .navigationBarDrawer(
        displayMode: .always
      )
    )
    .navigationTitle("Exclude Website")
    .navigationBarTitleDisplayMode(.inline)
    .toolbar {
      if !(filteredSections.isEmpty) {
        editWithResetButton()
      }
    }
  }

  func domainToggle(domain: Domain) -> some View {
    Toggle(
      domain.url!,
      isOn: .init(
        get: { domain.lockdown_disabled!.boolValue },
        set: { newValue in
          domain.lockdown_disabled = NSNumber(value: newValue)
          do {
            try viewContext.save()
            if isDeviceLockdown {
              let selectedTabURL = tabManager.selectedTab?.webView?.url?.domainURL
              if selectedTabURL?.absoluteString == domain.url {
                tabManager.reloadSelectedTab()
              } else {
                tabManager.resetLockdownWebpage(domain: domain)
              }
            }
          } catch {
            Logger.module.error(
              "Error saving lockdown mode status with managed object context: \(error)"
            )
          }
        }
      )
    )
    .toggleStyle(
      SwitchToggleStyle(tint: .accentColor)
    )
    .swipeActions(
      allowsFullSwipe: false,
      content: {
        Button(
          role: .destructive,
          action: {
            resetDomain(domain: domain)
          },
          label: {
            Text("Reset")
          }
        )
      }
    )
  }

  func resetDomain(domain: Domain) {
    domain.lockdown_disabled = nil
    var tabSelected: Bool = false
    let selectedTabDomain = tabManager.selectedTab?.webView?.url?.domainURL.absoluteString
    if selectedTabDomain == domain.url {
      tabSelected = true
    }
    do {
      try viewContext.save()
      if isDeviceLockdown {
        if !tabSelected {
          tabManager.resetLockdownWebpage(domain: domain)
        } else {
          tabManager.reloadSelectedTab()
        }
      }
    } catch let error as NSError {
      Logger.module.error(
        "Error resetting lockdown mode status with managed object context: \(error)"
      )
    }
  }

  func resetSelectedDomains() {
    var tabSelected: Bool = false
    let selectedTabDomain = tabManager.selectedTab?.webView?.url?.domainURL.absoluteString
    var resettingDomains: [SectionedFetchResults<String, Domain>.Section] = []
    for section in filteredSections {
      for domain in section {
        // selection contains only core data id, so we can only use domain.id
        if selection.contains(domain.id) {
          domain.lockdown_disabled = nil
          if selectedTabDomain == domain.url {
            tabSelected = true
          } else {
            resettingDomains.append(section)
          }
        }
      }
    }
    do {
      try viewContext.save()
      if isDeviceLockdown {
        if tabSelected {
          tabManager.reloadSelectedTab()
        }
        for section in resettingDomains {
          for domain in section {
            tabManager.resetLockdownWebpage(domain: domain)
          }
        }
      }
    } catch let error as NSError {
      Logger.module.error(
        "Error resetting multiple lockdown mode statuses with managed object context: \(error)"
      )
    }
    selection.removeAll()
  }

  func editWithResetButton() -> some View {
    Button {
      withAnimation {
        if editMode?.wrappedValue == .active {
          if !selection.isEmpty {
            resetSelectedDomains()
          }
          editMode?.wrappedValue = .inactive
        } else {
          editMode?.wrappedValue = .active
        }
      }
    } label: {
      if editMode?.wrappedValue == .active {
        if !selection.isEmpty {
          Text("Reset").tint(.red)
        } else {
          Text("Done")
        }
      } else {
        Text("Edit")
      }
    }
  }
}
