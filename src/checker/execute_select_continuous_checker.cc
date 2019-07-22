
 //
 // Created by quanchzhang
 //

#include "execute_select_continuous_checker.h"
#include <comm/lint_util.h>
#include <core/lint_logic.h>
#include "comm/log/logger.h"

namespace sqlitelint {

	void ExecuteSelectContinuousChecker::Check(LintEnv & env, const SqlInfo & sql_info, std::vector<Issue>* issues)
	{
		const std::string& sql = sql_info.sql_;
		const std::string& wildcard_sql = sql_info.wildcard_sql_.empty() ? sql_info.sql_ : sql_info.wildcard_sql_;

		if (sql_info.sql_type_ != SQLTYPE_SELECT) {
			return;
		}

		if (env.IsInWhiteList(kCheckerName, sql) || env.IsInWhiteList(kCheckerName, wildcard_sql)) {
			sVerbose("ReplaceIntoChecker::Check in white list");
			return;
		}

		std::vector<SqlInfo> sqlHistory = env.GetSqlHistory();
		if (sqlHistory.size() > 1) {
			int idx = sqlHistory.size() - 1;
			BinarySearchSqlInfoIdx(sqlHistory, sql_info, idx);
			if (idx > 0) {
				int last_idx;
				for (last_idx = idx - 1; last_idx >= 0; --last_idx)
					if (sqlHistory[last_idx].thread_id == sql_info.thread_id) {
						break;
					}
				if (last_idx >= 0) {
					SqlInfo lastSql_info = sqlHistory[last_idx];
					const std::string& lastWildcard_sql = lastSql_info.wildcard_sql_.empty() ? lastSql_info.sql_ : lastSql_info.wildcard_sql_;
					if (wildcard_sql == lastWildcard_sql
						&& (sql_info.execution_time_ - lastSql_info.execution_time_) <= kTakeConsiderContinuityThreshold) {
						PublishIssue(env, sql_info, issues);
					}
				}
				
			}
		}
	}

	CheckScene ExecuteSelectContinuousChecker::GetCheckScene()
	{
		return CheckScene::kEverySql;
	}

	void ExecuteSelectContinuousChecker::BinarySearchSqlInfoIdx(const std::vector<SqlInfo>& sqlHistory, const SqlInfo & sql_info, int & idx)
	{
		if (sql_info.execution_time_ == sqlHistory[idx].execution_time_) return;

		int left = 0, right = sqlHistory.size() - 1;
		while (left <= right) {
			idx = (left + right) / 2;
			if (sql_info.execution_time_ == sqlHistory[idx].execution_time_) {
				return;
			}
			if (sql_info.execution_time_ > sqlHistory[idx].execution_time_) {
				left = idx + 1;
			}
			else if (sql_info.execution_time_ < sqlHistory[idx].execution_time_) {
				right = idx - 1;
			}
		}
	}

	void ExecuteSelectContinuousChecker::PublishIssue(const LintEnv & env, const SqlInfo& sql_info, std::vector<Issue>* issues)
	{
		std::string desc = "The following select statement was executed continuously:";
		const std::string& wildcard_sql = sql_info.wildcard_sql_.empty() ? sql_info.sql_ : sql_info.wildcard_sql_;
		desc += wildcard_sql;

		Issue issue;
		issue.id = GenIssueId(env.GetDbFileName(), kCheckerName, desc);
		issue.db_path = env.GetDbPath();
		issue.create_time = GetSysTimeMillisecond();
		issue.level = IssueLevel::kSuggestion;
		issue.type = IssueType::kExecuteSelectContinuous;
		issue.desc = desc;
		issue.advice = "It is recommended to combine multiple continuous select statements into one.";

		if (recently_reported_id == issue.id) {
			sVerbose("PreparedStatementBetterChecker::PublishIssue recently reported %s", issue.id.c_str());
			return;
		}
		recently_reported_id = issue.id;
		issues->push_back(issue);
	}

}
