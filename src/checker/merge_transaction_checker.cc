#include "comm/lint_util.h"
#include "comm/log/logger.h"
#include "core/lint_logic.h"
#include "merge_transaction_checker.h"
#include <vector>
namespace sqlitelint {

	void sqlitelint::MergeTransactionChecker::Check(LintEnv & env, const SqlInfo & sql_info, std::vector<Issue>* issues)
	{
		const std::string& sql = sql_info.sql_;
		const std::string& wildcard_sql = sql_info.wildcard_sql_.empty() ? sql_info.sql_ : sql_info.wildcard_sql_;
		
		if (sql_info.sql_type_ != SQLTYPE_TRANSACTION_BEGIN) {
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
					if (lastSql_info.sql_type_ == SQLTYPE_TRANSACTION_COMMIT)
						PublishIssue(env, sql_info, issues);
				}
				
			}
		}
	}

	CheckScene sqlitelint::MergeTransactionChecker::GetCheckScene()
	{
		return CheckScene::kEverySql;
	}

	void MergeTransactionChecker::BinarySearchSqlInfoIdx(std::vector<SqlInfo>& sqlHistory, const SqlInfo & sql_info, int & idx)
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

	void sqlitelint::MergeTransactionChecker::PublishIssue(const LintEnv & env, const SqlInfo & sql_info, std::vector<Issue>* issues)
	{
		const std::string& wildcard_sql = sql_info.wildcard_sql_.empty() ? sql_info.sql_ : sql_info.wildcard_sql_;

		Issue issue;
		issue.id = GenIssueId(env.GetDbFileName(), kCheckerName, wildcard_sql);
		issue.db_path = env.GetDbPath();
		issue.create_time = GetSysTimeMillisecond();
		issue.level = IssueLevel::kSuggestion;
		issue.type = IssueType::kMergeTransaction;
		issue.desc = "Found a transaction ends with another transaction.";
		issue.advice = "It is recommended to merge the two transaction into one.";
		issue.ext_info = sql_info.ext_info_;
		issue.sql_time_cost = sql_info.time_cost_;
		issue.is_in_main_thread = sql_info.is_in_main_thread_;

		issues->push_back(issue);
	}

}
