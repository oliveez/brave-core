/* Copyright (c) 2024 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

import { createStateManager } from '../../shared/lib/state_manager'
import { createLocaleContextForTesting } from '../../shared/lib/locale_context'
import { AppModel, AppState, defaultState, defaultModel } from '../lib/app_model'
import { localeStrings } from '../lib/locale_strings'

function delay(ms: number) {
  return new Promise((resolve) => {
    setTimeout(resolve, ms)
  })
}

export function createModel(): AppModel {
  const locale = createLocaleContextForTesting(localeStrings)
  const stateManager = createStateManager<AppState>({
    ...defaultState(),
    loading: false,
    paymentId: '123'
  })

  return {
    ...defaultModel(),
    getString: locale.getString,
    getPluralString: locale.getPluralString,
    getState: stateManager.getState,
    addListener: stateManager.addListener,
    async enableRewards(countryCode) {
      await delay(500)
      setTimeout(() => {
        stateManager.update({ paymentId: 'abc123' })
      }, 20)
      return 'success'
    }
  }
}
