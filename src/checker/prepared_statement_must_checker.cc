
#include "prepared_statement_must_checker.h"
#include "core/lint_logic.h"
#include "comm/lint_util.h"
#include <comm/log/logger.h>

namespace sqlitelint {
	void PreparedStatementMustChecker::Check(LintEnv & env, const SqlInfo & sql_info, std::vector<Issue>* issues)
	{
		if (env.IsInWhiteList(kCheckerName, sql_info.wildcard_sql_)
			|| env.IsInWhiteList(kCheckerName, sql_info.sql_)) {
			sVerbose("PreparedStatementMustChecker::Check in white list: %s; %s", sql_info.wildcard_sql_.c_str(), sql_info.sql_.c_str());
			return;
		}

		if (!sql_info.is_prepared_statement_) {
			PublishIssue(env, sql_info, issues);
		}
	}
	CheckScene PreparedStatementMustChecker::GetCheckScene()
	{
		return CheckScene::kUncheckedSql;
	}
	void PreparedStatementMustChecker::PublishIssue(const LintEnv & env, const SqlInfo& sql_info, std::vector<Issue>* issues)
	{
		const std::string& wildcard_sql = sql_info.wildcard_sql_.empty() ? sql_info.sql_ : sql_info.wildcard_sql_;
		sVerbose("PreparedStatementMustChecker::PublishIssue sql:%s", wildcard_sql.c_str());

		Issue issue;
		issue.id = GenIssueId(env.GetDbFileName(), kCheckerName, wildcard_sql);
		issue.db_path = env.GetDbPath();
		issue.create_time = GetSysTimeMillisecond();
		issue.level = IssueLevel::kSuggestion;
		issue.type = IssueType::kPreparedStatementMust;
		issue.sql = sql_info.sql_;
		issue.desc = "Found a sql which does not use prepared statement:" + sql_info.sql_;
		issue.advice = "It is recommended to use prepared statement instead.";
		issue.ext_info = sql_info.ext_info_;
		issue.sql_time_cost = sql_info.time_cost_;
		issue.is_in_main_thread = sql_info.is_in_main_thread_;

		if (issue.id == recently_reported_id) {
			sVerbose("PreparedStatementMustChecker::PublishIssue recently reported %s", issue.id.c_str());
			return;
		}
		recently_reported_id = issue.id;
		issues->push_back(issue);
	}
}