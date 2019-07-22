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
//If a sql select statement was executed continuously, there mey be a 
//optimization to merge them into one statement;
//For example:
//select name from table_name where id=5;
//select name from table_name where id=10;
//then, we can do a optimization like:
//select name from table_name where id in (5,10);
//We detect select statement to find whether it was the same as last one.
//If it exists, we give a suggestion.
//
// Created by quanchzhang
//

#ifndef SQLITE_LINT_CHECKER_EXECUTE_SELECT_CONTINUOUS_CHECKER_H
#define SQLITE_LINT_CHECKER_EXECUTE_SELECT_CONTINUOUS_CHECKER_H

#include <vector>
#include "checker.h"

namespace sqlitelint {

	class ExecuteSelectContinuousChecker : public Checker {
	public:
		virtual void Check(LintEnv& env, const SqlInfo& sql_info, std::vector<Issue>* issues) override;
		virtual CheckScene GetCheckScene() override;

	private:
		constexpr static const char* const  kCheckerName = CheckerName::kExecuteSelectContinuousCheckerName;

		const static int kTakeConsiderContinuityThreshold = 1000;

		void BinarySearchSqlInfoIdx(const std::vector<SqlInfo>& sqlHistory, const SqlInfo& sql_info, int& idx);
		void PublishIssue(const LintEnv& env, const SqlInfo& sql_info, std::vector<Issue>* issues);

		std::string recently_reported_id;
	};
}

#endif //SQLITE_LINT_CHECKER_EXECUTE_SELECT_CONTINUOUS_CHECKER_H
