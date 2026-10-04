// Copyright 2026 Marco3jp
//
// Use of this source code is governed by the BSD 3-Clause license that can be
// found in the LICENSE file at the root of this repository.

#include "rewriter/candidate_priority_rewriter.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "base/file_util.h"
#include "base/system_util.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "request/conversion_request.h"

namespace mozc {

CandidatePriorityRewriter::CandidatePriorityRewriter()
    : CandidatePriorityRewriter(FileUtil::JoinPath(
          SystemUtil::GetUserProfileDirectory(), kFileName)) {}

CandidatePriorityRewriter::CandidatePriorityRewriter(std::string filename)
    : filename_(std::move(filename)) {
  Reload();
}

int CandidatePriorityRewriter::capability(
    const ConversionRequest& request) const {
  return CONVERSION;
}

bool CandidatePriorityRewriter::Reload() {
  rules_.clear();
  absl::StatusOr<std::string> contents = FileUtil::GetContents(filename_);
  if (!contents.ok()) {
    // The rule file is optional.
    return true;
  }
  LoadFromString(*contents);
  return true;
}

bool CandidatePriorityRewriter::LoadFromString(absl::string_view contents) {
  rules_.clear();
  for (absl::string_view line : absl::StrSplit(contents, '\n')) {
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    if (line.empty() || line.front() == '#') {
      continue;
    }
    const std::vector<absl::string_view> fields = absl::StrSplit(line, '\t');
    int rank = 0;
    if (fields.size() < 3 || fields[0].empty() || fields[1].empty() ||
        !absl::SimpleAtoi(fields[2], &rank) || rank < 1) {
      LOG(WARNING) << "Invalid line in " << filename_ << ": " << line;
      continue;
    }
    rules_[fields[0]].push_back({std::string(fields[1]), rank - 1});
  }
  for (auto& [reading, rules] : rules_) {
    absl::c_stable_sort(rules, [](const Rule& lhs, const Rule& rhs) {
      return lhs.rank < rhs.rank;
    });
  }
  return !rules_.empty();
}

bool CandidatePriorityRewriter::Rewrite(const ConversionRequest& request,
                                        Segments* segments) const {
  if (rules_.empty()) {
    return false;
  }

  bool modified = false;
  for (Segment& segment : segments->conversion_segments()) {
    // Collects (candidate, rank) pairs first, since moving candidates changes
    // their indices.
    std::vector<std::pair<const converter::Candidate*, int>> targets;
    for (size_t i = 0; i < segment.candidates_size(); ++i) {
      const converter::Candidate& candidate = segment.candidate(i);
      const auto it = rules_.find(candidate.content_key);
      if (it == rules_.end()) {
        continue;
      }
      for (const Rule& rule : it->second) {
        if (rule.surface != candidate.content_value) {
          continue;
        }
        const bool already_targeted =
            absl::c_any_of(targets, [&rule](const auto& target) {
              return target.first->content_value == rule.surface;
            });
        if (!already_targeted) {
          targets.emplace_back(&candidate, rule.rank);
        }
        break;
      }
    }

    if (targets.empty()) {
      continue;
    }

    const std::vector<converter::Candidate*> original(
        segment.candidates().begin(), segment.candidates().end());
    const int last = static_cast<int>(segment.candidates_size()) - 1;
    auto index_of = [&segment](const converter::Candidate* candidate) {
      const auto& candidates = segment.candidates();
      return static_cast<int>(absl::c_find(candidates, candidate) -
                              candidates.begin());
    };

    // Moves all targets to the tail first so that placing one target never
    // shifts another target that has already been placed.
    for (const auto& [candidate, rank] : targets) {
      segment.move_candidate(index_of(candidate), last);
    }
    absl::c_stable_sort(targets, [](const auto& lhs, const auto& rhs) {
      return lhs.second < rhs.second;
    });
    for (const auto& [candidate, rank] : targets) {
      segment.move_candidate(index_of(candidate), std::min(rank, last));
    }

    if (!absl::c_equal(original, segment.candidates())) {
      modified = true;
    }
  }
  return modified;
}

}  // namespace mozc
