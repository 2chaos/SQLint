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

//Sometimes, one could replace a record into a modified table.
//It could override some data which is important;
//For example:
// create table tbl_name(
//	 id int,
//	 name text,
//	 sex text,
//	 country text,
//	 emali text
// );
// replace into tbl_name(id, name) values(1, 'Jack');
//We compare the number of values to be replaced and the number of columns in 
//the table. If they are not equal, we publish a warning.
//
//Created by quanchzhang
//

#ifndef SQLITE_LINT_CHECKER_REPLACE_INTO_CHECKER_H
#define SQLITE_LINT_CHECKER_REPLACE_INTO_CHECKER_H

#include "checker.h"

namespace sqlitelint {

	class ReplaceIntoChecker :public Checker {
	public:
		virtual void Check(LintEnv& env, const SqlInfo& sql_info, std::vector<Issue>* issues);
		virtual CheckScene GetCheckScene();

	private:
		constexpr static const char* const kCheckerName = CheckerName::kReplaceIntoCheckerName;
		constexpr static const char* const kPragmaOptimizeKeyWord = "replace into";	

		void PublishIssue(const LintEnv& env, const std::string& table_name, const SqlInfo& sql_info, std::vector<Issue>* issues);

		void ExtractTableAndCountColumn(const std::string &detail, std::string &table, int& columnCnt);
	};
}

#endif // SQLITE_LINT_CHECKER_REPLACE_INTO_CHECKER_H

