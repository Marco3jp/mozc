// Copyright 2026 Marco3jp
//
// Use of this source code is governed by the BSD 3-Clause license that can be
// found in the LICENSE file at the root of this repository.

#ifndef MOZC_REWRITER_CANDIDATE_PRIORITY_REWRITER_H_
#define MOZC_REWRITER_CANDIDATE_PRIORITY_REWRITER_H_

#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "converter/segments.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {

// Moves candidates to fixed positions according to a user-editable rule file,
// without relying on the learning (user history) features.
//
// The rule file is a TSV placed in the user profile directory:
//
//   # reading <TAB> surface <TAB> rank (1-origin)
//   にき	二期	1
//
// When a segment has a candidate whose content_key/content_value match the
// reading/surface, the candidate is moved to |rank|. Candidates that do not
// exist in the segment are not added; use the user dictionary for that.
class CandidatePriorityRewriter : public RewriterInterface {
 public:
  static constexpr absl::string_view kFileName = "candidate_priority.tsv";

  // Loads rules from |kFileName| in the user profile directory.
  CandidatePriorityRewriter();
  // Loads rules from |filename|. Mainly for testing.
  explicit CandidatePriorityRewriter(std::string filename);

  int capability(const ConversionRequest& request) const override;

  bool Rewrite(const ConversionRequest& request,
               Segments* segments) const override;

  // Re-reads the rule file. Called when the user dictionary is edited.
  bool Reload() override;

  // Parses the rule file contents. Returns false if no valid rule is found.
  bool LoadFromString(absl::string_view contents);

 private:
  struct Rule {
    std::string surface;
    int rank;  // 0-origin
  };

  std::string filename_;
  // reading -> rules sorted by rank.
  absl::flat_hash_map<std::string, std::vector<Rule>> rules_;
};

}  // namespace mozc

#endif  // MOZC_REWRITER_CANDIDATE_PRIORITY_REWRITER_H_
