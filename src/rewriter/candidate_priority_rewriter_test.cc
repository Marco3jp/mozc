// Copyright 2026 Marco3jp
//
// Use of this source code is governed by the BSD 3-Clause license that can be
// found in the LICENSE file at the root of this repository.

#include "rewriter/candidate_priority_rewriter.h"

#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "base/file_util.h"
#include "base/system_util.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "request/conversion_request.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace {

Segment* AddSegment(absl::string_view key,
                    const std::vector<absl::string_view>& values,
                    Segments* segments) {
  Segment* segment = segments->push_back_segment();
  segment->set_key(key);
  for (absl::string_view value : values) {
    converter::Candidate* candidate = segment->add_candidate();
    candidate->key = key;
    candidate->content_key = key;
    candidate->value = value;
    candidate->content_value = value;
  }
  return segment;
}

std::string GetCandidates(const Segment& segment) {
  std::vector<absl::string_view> values;
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    values.push_back(segment.candidate(i).value);
  }
  return absl::StrJoin(values, " ");
}

class CandidatePriorityRewriterTest : public testing::TestWithTempUserProfile {
 protected:
  CandidatePriorityRewriter rewriter_{"/nonexistent/candidate_priority.tsv"};
  const ConversionRequest request_;
};

TEST_F(CandidatePriorityRewriterTest, NoRules) {
  Segments segments;
  const Segment* segment =
      AddSegment("にき", {"二期", "二季", "仁木"}, &segments);
  EXPECT_FALSE(rewriter_.Rewrite(request_, &segments));
  EXPECT_EQ(GetCandidates(*segment), "二期 二季 仁木");
}

TEST_F(CandidatePriorityRewriterTest, PromoteAndDemote) {
  ASSERT_TRUE(rewriter_.LoadFromString(
      "# comment\n"
      "にき\t仁木\t1\n"
      "じっそう\t実装\t3\r\n"));

  Segments segments;
  const Segment* niki =
      AddSegment("にき", {"二期", "二季", "仁木"}, &segments);
  const Segment* jissou =
      AddSegment("じっそう", {"実装", "実相", "じっそう", "ジッソウ"},
                 &segments);
  EXPECT_TRUE(rewriter_.Rewrite(request_, &segments));
  EXPECT_EQ(GetCandidates(*niki), "仁木 二期 二季");
  EXPECT_EQ(GetCandidates(*jissou), "実相 じっそう 実装 ジッソウ");
}

TEST_F(CandidatePriorityRewriterTest, MultipleRulesForSameReading) {
  ASSERT_TRUE(rewriter_.LoadFromString(
      "にき\t二期\t3\n"
      "にき\t新木\t1\n"
      "にき\t二木\t2\n"));

  Segments segments;
  const Segment* segment = AddSegment(
      "にき", {"二期", "二季", "仁木", "二木", "新木"}, &segments);
  EXPECT_TRUE(rewriter_.Rewrite(request_, &segments));
  EXPECT_EQ(GetCandidates(*segment), "新木 二木 二期 二季 仁木");
}

TEST_F(CandidatePriorityRewriterTest, RankBeyondCandidates) {
  ASSERT_TRUE(rewriter_.LoadFromString("にき\t二期\t100\n"));

  Segments segments;
  const Segment* segment =
      AddSegment("にき", {"二期", "二季", "仁木"}, &segments);
  EXPECT_TRUE(rewriter_.Rewrite(request_, &segments));
  EXPECT_EQ(GetCandidates(*segment), "二季 仁木 二期");
}

TEST_F(CandidatePriorityRewriterTest, MissingCandidateIsNotAdded) {
  ASSERT_TRUE(rewriter_.LoadFromString("にき\t二騎\t1\n"));

  Segments segments;
  const Segment* segment =
      AddSegment("にき", {"二期", "二季", "仁木"}, &segments);
  EXPECT_FALSE(rewriter_.Rewrite(request_, &segments));
  EXPECT_EQ(GetCandidates(*segment), "二期 二季 仁木");
}

TEST_F(CandidatePriorityRewriterTest, MatchesContentValue) {
  ASSERT_TRUE(rewriter_.LoadFromString("にき\t二季\t1\n"));

  Segments segments;
  Segment* segment = segments.push_back_segment();
  segment->set_key("にきは");
  for (absl::string_view content_value : {"二期", "二季"}) {
    converter::Candidate* candidate = segment->add_candidate();
    candidate->key = "にきは";
    candidate->content_key = "にき";
    candidate->value = absl::StrCat(content_value, "は");
    candidate->content_value = content_value;
  }
  EXPECT_TRUE(rewriter_.Rewrite(request_, &segments));
  EXPECT_EQ(GetCandidates(*segment), "二季は 二期は");
}

TEST_F(CandidatePriorityRewriterTest, InvalidLinesAreIgnored) {
  EXPECT_FALSE(rewriter_.LoadFromString(
      "にき\t二期\n"
      "にき\t二期\t0\n"
      "にき\t二期\tabc\n"
      "\t二期\t1\n"));
}

TEST_F(CandidatePriorityRewriterTest, LoadsFromUserProfileDirectory) {
  ASSERT_OK(FileUtil::SetContents(
      FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(),
                         CandidatePriorityRewriter::kFileName),
      "にき\t仁木\t1\n"));
  CandidatePriorityRewriter rewriter;

  Segments segments;
  const Segment* segment =
      AddSegment("にき", {"二期", "二季", "仁木"}, &segments);
  EXPECT_TRUE(rewriter.Rewrite(request_, &segments));
  EXPECT_EQ(GetCandidates(*segment), "仁木 二期 二季");
}

}  // namespace
}  // namespace mozc
