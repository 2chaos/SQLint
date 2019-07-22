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

#include "replace_into_checker.h"
#include "comm/log/logger.h"
#include "comm/lint_util.h"
#include "core/lint_logic.h"
#include "core/sql_Info_processor.h"
#include "eqp/where_clause_table_column_finder.h"
#include <regex>

namespace sqlitelint {

	void ReplaceIntoChecker::Check(LintEnv& env, const SqlInfo& sql_info, std::vector<Issue>* issues) {
		const std::string& sql = sql_info.sql_;
		const std::string& wildcard_sql = sql_info.wildcard_sql_.empty() ? sql_info.sql_ : sql_info.wildcard_sql_;
		sVerbose("ReplaceIntoChecker::Check sql: %s, whildcard_sql: %s", sql.c_str(), wildcard_sql.c_str());

		if (sql_info.sql_type_ != SQLTYPE_REPLACE) {
			return;
		}

		if (env.IsInWhiteList(kCheckerName, sql) || env.IsInWhiteList(kCheckerName, wildcard_sql)) {
			sVerbose("ReplaceIntoChecker::Check in white list");
			return;
		}

		std::string table_name;
		int columnCnt;
		ExtractTableAndCountColumn(wildcard_sql, table_name, columnCnt);

		std::vector<TableInfo> tables = env.GetTablesInfo();
		for (TableInfo table_info : tables) {
			if (table_info.table_name_ == table_name) {
				if (columnCnt != table_info.columns_.size()) {
					PublishIssue(env, table_name, sql_info, issues);
				}
				break;
			}
		}
	}

	CheckScene ReplaceIntoChecker::GetCheckScene() {
		return CheckScene::kEverySql;
	}

	void ReplaceIntoChecker::PublishIssue(const LintEnv & env, const std::string & table_name, const SqlInfo& sql_info, std::vector<Issue>* issues)
	{
		const std::string& wildcard_sql = sql_info.wildcard_sql_.empty() ? sql_info.sql_ : sql_info.wildcard_sql_;

		Issue issue;
		issue.id = GenIssueId(env.GetDbFileName(), kCheckerName, wildcard_sql);
		issue.db_path = env.GetDbPath();
		issue.create_time = GetSysTimeMillisecond();
		issue.level = IssueLevel::kWarning;
		issue.sql = sql_info.sql_;
		issue.type = IssueType::kReplaceIntoMissingParameter;
		issue.table = table_name;
		issue.desc = "The 'replace into' statement does not contain all the columns of the table: " + table_name + ", which may result in data overwriting.";
		issue.advice = "You'd better check the construction of the table: " + table_name + ", which may have changed at some point.";

		issues->push_back(issue);
	}

	void ReplaceIntoChecker::ExtractTableAndCountColumn(const std::string &detail, std::string &table, int& columnCnt) {
		std::regex kTableRgx = std::regex("(?:replace\\s*into\\s*)([^( ]+)(?:.*values\\s*)(.*)");
		std::smatch m;
		std::string values;
		if (regex_search(detail, m, kTableRgx)) {
			table.assign(m[1]);
			values.assign(m[2]);
			columnCnt = 1;
			for (auto iter = values.begin(); iter != values.end(); iter++ ) {
				if (*iter == ',') ++columnCnt;
			}

			return;
		}
	}

}