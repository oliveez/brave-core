/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "brave/components/brave_rewards/core/credentials/credentials_promotion.h"
#include "brave/components/brave_rewards/core/credentials/credentials_util.h"
#include "brave/components/brave_rewards/core/database/database.h"
#include "brave/components/brave_rewards/core/rewards_engine_impl.h"

namespace brave_rewards::internal {
namespace credential {

CredentialsPromotion::CredentialsPromotion(RewardsEngineImpl& engine)
    : engine_(engine), common_(engine), promotion_server_(engine) {}

CredentialsPromotion::~CredentialsPromotion() = default;

void CredentialsPromotion::Start(const CredentialsTrigger& trigger,
                                 ResultCallback callback) {
  auto get_callback =
      base::BindOnce(&CredentialsPromotion::OnStart, weak_factory_.GetWeakPtr(),
                     std::move(callback), trigger);

  engine_->database()->GetCredsBatchByTrigger(trigger.id, trigger.type,
                                              std::move(get_callback));
}

void CredentialsPromotion::OnStart(ResultCallback callback,
                                   const CredentialsTrigger& trigger,
                                   mojom::CredsBatchPtr creds) {
  mojom::CredsBatchStatus status = mojom::CredsBatchStatus::NONE;
  if (creds) {
    status = creds->status;
  }

  switch (status) {
    case mojom::CredsBatchStatus::NONE: {
      Blind(std::move(callback), trigger);
      break;
    }
    case mojom::CredsBatchStatus::BLINDED: {
      auto get_callback = base::BindOnce(&CredentialsPromotion::Claim,
                                         weak_factory_.GetWeakPtr(),
                                         std::move(callback), trigger);

      engine_->database()->GetCredsBatchByTrigger(trigger.id, trigger.type,
                                                  std::move(get_callback));
      break;
    }
    case mojom::CredsBatchStatus::CLAIMED: {
      auto get_callback = base::BindOnce(
          &CredentialsPromotion::FetchSignedCreds, weak_factory_.GetWeakPtr(),
          std::move(callback), trigger);

      engine_->database()->GetPromotion(trigger.id, std::move(get_callback));
      break;
    }
    case mojom::CredsBatchStatus::SIGNED: {
      auto get_callback = base::BindOnce(&CredentialsPromotion::Unblind,
                                         weak_factory_.GetWeakPtr(),
                                         std::move(callback), trigger);

      engine_->database()->GetCredsBatchByTrigger(trigger.id, trigger.type,
                                                  std::move(get_callback));
      break;
    }
    case mojom::CredsBatchStatus::FINISHED: {
      std::move(callback).Run(mojom::Result::OK);
      break;
    }
    case mojom::CredsBatchStatus::CORRUPTED: {
      std::move(callback).Run(mojom::Result::FAILED);
      break;
    }
  }
}

void CredentialsPromotion::Blind(ResultCallback callback,
                                 const CredentialsTrigger& trigger) {
  auto blinded_callback =
      base::BindOnce(&CredentialsPromotion::OnBlind, weak_factory_.GetWeakPtr(),
                     std::move(callback), trigger);
  common_.GetBlindedCreds(trigger, std::move(blinded_callback));
}

void CredentialsPromotion::OnBlind(ResultCallback callback,
                                   const CredentialsTrigger& trigger,
                                   mojom::Result result) {
  if (result != mojom::Result::OK) {
    engine_->LogError(FROM_HERE) << "Blinding failed";
    std::move(callback).Run(result);
    return;
  }

  auto get_callback =
      base::BindOnce(&CredentialsPromotion::Claim, weak_factory_.GetWeakPtr(),
                     std::move(callback), trigger);

  engine_->database()->GetCredsBatchByTrigger(trigger.id, trigger.type,
                                              std::move(get_callback));
}

void CredentialsPromotion::Claim(ResultCallback callback,
                                 const CredentialsTrigger& trigger,
                                 mojom::CredsBatchPtr creds) {
  if (!creds) {
    engine_->LogError(FROM_HERE) << "Creds not found";
    std::move(callback).Run(mojom::Result::FAILED);
    return;
  }

  auto blinded_creds = ParseStringToBaseList(creds->blinded_creds);

  if (!blinded_creds || blinded_creds->empty()) {
    engine_->LogError(FROM_HERE)
        << "Blinded creds are corrupted, we will try to blind again";
    auto save_callback =
        base::BindOnce(&CredentialsPromotion::RetryPreviousStepSaved,
                       weak_factory_.GetWeakPtr(), std::move(callback));

    engine_->database()->UpdateCredsBatchStatus(trigger.id, trigger.type,
                                                mojom::CredsBatchStatus::NONE,
                                                std::move(save_callback));
    return;
  }

  auto url_callback =
      base::BindOnce(&CredentialsPromotion::OnClaim, weak_factory_.GetWeakPtr(),
                     std::move(callback), trigger);

  DCHECK(blinded_creds.has_value());
  promotion_server_.post_creds().Request(
      trigger.id, std::move(blinded_creds.value()), std::move(url_callback));
}

void CredentialsPromotion::OnClaim(ResultCallback callback,
                                   const CredentialsTrigger& trigger,
                                   mojom::Result result,
                                   const std::string& claim_id) {
  if (result != mojom::Result::OK) {
    std::move(callback).Run(result);
    return;
  }

  auto save_callback =
      base::BindOnce(&CredentialsPromotion::ClaimedSaved,
                     weak_factory_.GetWeakPtr(), std::move(callback), trigger);

  engine_->database()->SavePromotionClaimId(trigger.id, claim_id,
                                            std::move(save_callback));
}

void CredentialsPromotion::ClaimedSaved(ResultCallback callback,
                                        const CredentialsTrigger& trigger,
                                        mojom::Result result) {
  if (result != mojom::Result::OK) {
    engine_->LogError(FROM_HERE) << "Claim id was not saved";
    std::move(callback).Run(mojom::Result::FAILED);
    return;
  }

  auto save_callback =
      base::BindOnce(&CredentialsPromotion::ClaimStatusSaved,
                     weak_factory_.GetWeakPtr(), std::move(callback), trigger);

  engine_->database()->UpdateCredsBatchStatus(trigger.id, trigger.type,
                                              mojom::CredsBatchStatus::CLAIMED,
                                              std::move(save_callback));
}

void CredentialsPromotion::ClaimStatusSaved(ResultCallback callback,
                                            const CredentialsTrigger& trigger,
                                            mojom::Result result) {
  if (result != mojom::Result::OK) {
    engine_->LogError(FROM_HERE) << "Claim status not saved";
    std::move(callback).Run(mojom::Result::FAILED);
    return;
  }

  auto get_callback =
      base::BindOnce(&CredentialsPromotion::FetchSignedCreds,
                     weak_factory_.GetWeakPtr(), std::move(callback), trigger);

  engine_->database()->GetPromotion(trigger.id, std::move(get_callback));
}

void CredentialsPromotion::RetryPreviousStepSaved(ResultCallback callback,
                                                  mojom::Result result) {
  if (result != mojom::Result::OK) {
    engine_->LogError(FROM_HERE) << "Previous step not saved";
    std::move(callback).Run(mojom::Result::FAILED);
    return;
  }

  std::move(callback).Run(mojom::Result::RETRY);
}

void CredentialsPromotion::FetchSignedCreds(ResultCallback callback,
                                            const CredentialsTrigger& trigger,
                                            mojom::PromotionPtr promotion) {
  if (!promotion) {
    engine_->LogError(FROM_HERE) << "Corrupted data";
    std::move(callback).Run(mojom::Result::FAILED);
    return;
  }

  if (promotion->claim_id.empty()) {
    engine_->LogError(FROM_HERE)
        << "Claim id is empty, we will try claim step again";

    auto save_callback =
        base::BindOnce(&CredentialsPromotion::RetryPreviousStepSaved,
                       weak_factory_.GetWeakPtr(), std::move(callback));

    engine_->database()->UpdateCredsBatchStatus(
        trigger.id, trigger.type, mojom::CredsBatchStatus::BLINDED,
        std::move(save_callback));
    return;
  }

  auto url_callback =
      base::BindOnce(&CredentialsPromotion::OnFetchSignedCreds,
                     weak_factory_.GetWeakPtr(), std::move(callback), trigger);

  promotion_server_.get_signed_creds().Request(trigger.id, promotion->claim_id,
                                               std::move(url_callback));
}

void CredentialsPromotion::OnFetchSignedCreds(ResultCallback callback,
                                              const CredentialsTrigger& trigger,
                                              mojom::Result result,
                                              mojom::CredsBatchPtr batch) {
  // Note: Translate mojom::Result::RETRY_SHORT into
  // mojom::Result::RETRY, as promotion only supports the standard
  // retry
  if (result == mojom::Result::RETRY_SHORT) {
    std::move(callback).Run(mojom::Result::RETRY);
    return;
  }

  if (result != mojom::Result::OK || !batch) {
    engine_->LogError(FROM_HERE) << "Problem parsing response";
    std::move(callback).Run(mojom::Result::FAILED);
    return;
  }

  batch->trigger_id = trigger.id;
  batch->trigger_type = trigger.type;

  auto save_callback =
      base::BindOnce(&CredentialsPromotion::SignedCredsSaved,
                     weak_factory_.GetWeakPtr(), std::move(callback), trigger);

  engine_->database()->SaveSignedCreds(std::move(batch),
                                       std::move(save_callback));
}

void CredentialsPromotion::SignedCredsSaved(ResultCallback callback,
                                            const CredentialsTrigger& trigger,
                                            mojom::Result result) {
  if (result != mojom::Result::OK) {
    engine_->LogError(FROM_HERE) << "Signed creds were not saved";
    std::move(callback).Run(mojom::Result::FAILED);
    return;
  }

  auto get_callback =
      base::BindOnce(&CredentialsPromotion::Unblind, weak_factory_.GetWeakPtr(),
                     std::move(callback), trigger);

  engine_->database()->GetCredsBatchByTrigger(trigger.id, trigger.type,
                                              std::move(get_callback));
}

void CredentialsPromotion::Unblind(ResultCallback callback,
                                   const CredentialsTrigger& trigger,
                                   mojom::CredsBatchPtr creds) {
  if (!creds) {
    engine_->LogError(FROM_HERE) << "Corrupted data";
    std::move(callback).Run(mojom::Result::FAILED);
    return;
  }

  auto get_callback = base::BindOnce(&CredentialsPromotion::VerifyPublicKey,
                                     weak_factory_.GetWeakPtr(),
                                     std::move(callback), trigger, *creds);

  engine_->database()->GetPromotion(trigger.id, std::move(get_callback));
}

void CredentialsPromotion::VerifyPublicKey(ResultCallback callback,
                                           const CredentialsTrigger& trigger,
                                           const mojom::CredsBatch& creds,
                                           mojom::PromotionPtr promotion) {
  if (!promotion) {
    engine_->LogError(FROM_HERE) << "Corrupted data";
    std::move(callback).Run(mojom::Result::FAILED);
    return;
  }

  auto promotion_keys = ParseStringToBaseList(promotion->public_keys);

  if (!promotion_keys || promotion_keys->empty()) {
    engine_->LogError(FROM_HERE) << "Public key is missing";
    std::move(callback).Run(mojom::Result::FAILED);
    return;
  }

  bool valid = false;
  for (auto& item : promotion_keys.value()) {
    if (item.GetString() == creds.public_key) {
      valid = true;
    }
  }

  if (!valid) {
    engine_->LogError(FROM_HERE) << "Public key is not valid";
    std::move(callback).Run(mojom::Result::FAILED);
    return;
  }

  std::vector<std::string> unblinded_encoded_creds;
  if (engine_->options().is_testing) {
    unblinded_encoded_creds = UnBlindCredsMock(creds);
  } else {
    auto result = UnBlindCreds(creds);
    if (!result.has_value()) {
      engine_->LogError(FROM_HERE) << "UnBlindTokens error";
      engine_->Log(FROM_HERE) << result.error();
      std::move(callback).Run(mojom::Result::FAILED);
      return;
    }
    unblinded_encoded_creds = std::move(result).value();
  }

  const double cred_value =
      promotion->approximate_value / promotion->suggestions;

  auto save_callback =
      base::BindOnce(&CredentialsPromotion::Completed,
                     weak_factory_.GetWeakPtr(), std::move(callback), trigger);

  uint64_t expires_at = 0ul;
  if (promotion->type != mojom::PromotionType::ADS) {
    expires_at = promotion->expires_at;
  }

  common_.SaveUnblindedCreds(expires_at, cred_value, creds,
                             unblinded_encoded_creds, trigger,
                             std::move(save_callback));
}

void CredentialsPromotion::Completed(ResultCallback callback,
                                     const CredentialsTrigger& trigger,
                                     mojom::Result result) {
  if (result != mojom::Result::OK) {
    engine_->LogError(FROM_HERE) << "Unblinded token save failed";
    std::move(callback).Run(result);
    return;
  }

  engine_->database()->PromotionCredentialCompleted(trigger.id,
                                                    std::move(callback));
  engine_->client()->UnblindedTokensReady();
}

void CredentialsPromotion::RedeemTokens(const CredentialsRedeem& redeem,
                                        ResultCallback callback) {
  DCHECK(redeem.type != mojom::RewardsType::TRANSFER);

  if (redeem.token_list.empty()) {
    engine_->LogError(FROM_HERE) << "Token list empty";
    std::move(callback).Run(mojom::Result::FAILED);
    return;
  }

  std::vector<std::string> token_id_list;
  for (const auto& item : redeem.token_list) {
    token_id_list.push_back(base::NumberToString(item.id));
  }

  if (redeem.publisher_key.empty()) {
    engine_->LogError(FROM_HERE) << "Publisher key is empty";
    std::move(callback).Run(mojom::Result::FAILED);
    return;
  }

  promotion_server_.post_suggestions().Request(
      redeem,
      base::BindOnce(&CredentialsPromotion::OnRedeemTokens,
                     weak_factory_.GetWeakPtr(), std::move(token_id_list),
                     redeem, std::move(callback)));
}

void CredentialsPromotion::OnRedeemTokens(
    std::vector<std::string> token_id_list,
    CredentialsRedeem redeem,
    ResultCallback callback,
    mojom::Result result) {
  if (result != mojom::Result::OK) {
    engine_->LogError(FROM_HERE) << "Failed to parse redeem tokens response";
    std::move(callback).Run(mojom::Result::FAILED);
    return;
  }

  std::string id;
  if (!redeem.contribution_id.empty()) {
    id = redeem.contribution_id;
  }

  engine_->database()->MarkUnblindedTokensAsSpent(token_id_list, redeem.type,
                                                  id, std::move(callback));
}

void CredentialsPromotion::DrainTokens(const CredentialsRedeem& redeem,
                                       PostSuggestionsClaimCallback callback) {
  DCHECK(redeem.type == mojom::RewardsType::TRANSFER);

  if (redeem.token_list.empty()) {
    engine_->LogError(FROM_HERE) << "Token list empty";
    std::move(callback).Run(mojom::Result::FAILED, "");
    return;
  }

  std::vector<std::string> token_id_list;
  for (const auto& item : redeem.token_list) {
    token_id_list.push_back(base::NumberToString(item.id));
  }

  auto url_callback = base::BindOnce(
      &CredentialsPromotion::OnDrainTokens, weak_factory_.GetWeakPtr(),
      std::move(callback), std::move(token_id_list), redeem);

  promotion_server_.post_suggestions_claim().Request(redeem,
                                                     std::move(url_callback));
}

void CredentialsPromotion::OnDrainTokens(
    PostSuggestionsClaimCallback callback,
    const std::vector<std::string>& token_id_list,
    const CredentialsRedeem& redeem,
    mojom::Result result,
    std::string drain_id) {
  if (result != mojom::Result::OK) {
    engine_->LogError(FROM_HERE) << "Failed to parse drain tokens response";
    std::move(callback).Run(mojom::Result::FAILED, "");
    return;
  }

  std::string id;
  if (!redeem.contribution_id.empty()) {
    id = redeem.contribution_id;
  }

  DCHECK(redeem.type == mojom::RewardsType::TRANSFER);

  auto mark_tokens_callback = base::BindOnce(
      [](PostSuggestionsClaimCallback callback, std::string drain_id,
         mojom::Result result) {
        if (result != mojom::Result::OK) {
          std::move(callback).Run(mojom::Result::FAILED, "");
        } else {
          std::move(callback).Run(mojom::Result::OK, std::move(drain_id));
        }
      },
      std::move(callback), std::move(drain_id));

  engine_->database()->MarkUnblindedTokensAsSpent(
      token_id_list, mojom::RewardsType::TRANSFER, id,
      std::move(mark_tokens_callback));
}

}  // namespace credential
}  // namespace brave_rewards::internal
