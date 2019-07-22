/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

 //
 // In WXWork project, prepared statement is necessary.
 //	If we detect a statement which is not prepared and it could be prepared,
 //	we publish a suggestion.
 //
 // Created by quanchzhang
 //

#ifndef SQLITE_LINT_CHECKER_PREPARED_STATEMENT_MUST_CHECKER_H
#define SQLITE_LINT_CHECKER_PREPARED_STATEMENT_MUST_CHECKER_H

#include <vector>
#include "checker.h"

namespace sqlitelint {

	class PreparedStatementMustChecker : public Checker {
	public:
		virtual void Check(LintEnv& env, const SqlInfo& sql_info, std::vector<Issue>* issues) override;
		virtual CheckScene GetCheckScene() override;

	private:
		constexpr static const char* const  kCheckerName = CheckerName::kPreparedStatementMustCheckerName;

		void PublishIssue(const LintEnv& env, const SqlInfo& sql_info, std::vector<Issue>* issues);

		std::string recently_reported_id;
	};
}

#endif //SQLITE_LINT_CHECKER_PREPARED_STATEMENT_MUST_CHECKER_H
