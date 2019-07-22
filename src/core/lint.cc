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
//  Created by liyongjie on 2017/2/13.
//

#include "lint.h"
#include <iostream>
#include <thread>
#include <regex>
#include <future>
#include "comm/log/logger.h"
#include "checker/eqp/explain_query_plan_checker.h"
#include "checker/prepared_statement_better_checker.h"
#include "checker/prepared_statement_must_checker.h"
#include "checker/avoid_auto_increment_checker.h"
#include "checker/avoid_select_all_checker.h"
#include "checker/without_rowid_better_checker.h"
#include "checker/redundant_index_checker.h"
#include "checker/replace_into_checker.h"
#include "checker/merge_transaction_checker.h"
#include "checker/execute_select_continuous_checker.h"
#include "core/sql_Info_processor.h"
#include "comm/lint_util.h"
#include "lint_logic.h"

namespace sqlitelint {

    Lint::Lint(const char* db_path, OnPublishIssueCallback issued_callback)
            : env_(db_path), checked_sql_cache_(500)
            , issued_callback_(issued_callback), exit_(false){

        check_thread_ = new std::thread(&Lint::Check, this);
    }

	inline bool Lint::PrepareSqlInfo(SqlInfo* sql_info, const char* sql, const long time_cost, const char* ext_info) {
		if (sql_info == nullptr) {
			sError("Lint::PrePrepareSqlInfo sql_info NULL");
			return false;
		}
		if (sql == nullptr) {
			sError("Lint::PrePrepareSqlInfo sql NULL");
			return false;
		}
		if (env_.IsReserveSql(sql)) {
			sDebug("Lint::PrePrepareSqlInfo a reserved sql");
			return false;
		}

		sql_info->sql_ = sql;
		sql_info->ext_info_ = ext_info;
		sql_info->time_cost_ = time_cost;
		sql_info->is_in_main_thread_ = IsInMainThread();
		sql_info->thread_id = GetThreadId();

		return true;
	}

	void Lint::NotifySqlExecution(const char *sql, const long time_cost, const char* ext_info) {
		SqlInfo *sql_info = new SqlInfo();
		if (!PrepareSqlInfo(sql_info, sql, time_cost, ext_info)) {
			return;
		}

		sql_info->is_synchronous = false;

        std::unique_lock<std::mutex> lock(queue_mutex_);
		//将执行时间的设定放在锁内执行，是为了防止出现两个线程通知的info执行时间相同，这样在按照执行时间从history中查找info会出现问题
		sql_info->execution_time_ = GetSysTimeMillisecond();	
        queue_.push_back(std::unique_ptr<SqlInfo>(sql_info));
        queue_cv_.notify_one();
        lock.unlock();
    }

	void Lint::NotifySqlExecutionSync(const char *sql, const long time_cost, const char* ext_info, std::vector<std::string>& issues_id) {
		SqlInfo *sql_info = new SqlInfo();
		if (!PrepareSqlInfo(sql_info, sql, time_cost, ext_info)) {
			return;
		}

		sql_info->is_synchronous = true;
		sql_info->checked_promise = new std::promise<std::vector<std::string>>();
		std::future<std::vector<std::string>> checked_future = sql_info->checked_promise->get_future();

		std::unique_lock<std::mutex> lock(queue_mutex_);
		sql_info->execution_time_ = GetSysTimeMillisecond();
		queue_.push_back(std::unique_ptr<SqlInfo>(sql_info));
		queue_cv_.notify_one();
		lock.unlock();

		issues_id = checked_future.get();
	}

	//虽然用了双端队列，还是得确认线程安全影响大不大
    int Lint::TakeSqlInfo(std::unique_ptr<SqlInfo> &sql_info) {
        std::unique_lock<std::mutex> lock(queue_mutex_);

        if (exit_) {
            return -1;
        }

        while (queue_.empty()) {
            sInfo("Lint::TakeSqlInfo queue empty and wait");
            queue_cv_.wait(lock);
            if (exit_) {
                return -1;
            }
        }
        sql_info = std::move(queue_.front());
        queue_.pop_front();

        return 0;
    }

    void Lint::InitCheck() {
        sVerbose("Lint::Check() init check");
        std::vector<Issue>* published_issues = new std::vector<Issue>;
        ScheduleCheckers(CheckScene::kAfterInit, SqlInfo(), published_issues);
        if (!published_issues->empty()) {
            sInfo("New check some diagnosis out!");
            if (issued_callback_) {
                issued_callback_(env_.GetDbPath().c_str(), *published_issues);
            }
        }
        delete published_issues;
    }

	void Lint::Check() {

        std::vector<Issue>* published_issues = new std::vector<Issue>;
        std::unique_ptr<SqlInfo> sql_info;
        SqlInfo simple_sql_info;
		std::vector<std::string> issues_id;

        while (true) {
            int ret = TakeSqlInfo(sql_info);
            if (ret != 0) {
                sError("check exit");
                break;
            }

			issues_id.clear();
            env_.IncSqlCnt();
            PreProcessSqlString(sql_info->sql_);
            sDebug("Lint::Check checked cnt=%d", env_.GetSqlCnt());
            if (!IsSqlSupportCheck(sql_info->sql_)) {
                sDebug("Lint::Check Sql not support");
                env_.AddToSqlHistory(*sql_info);
				if (sql_info->is_synchronous)
					sql_info->checked_promise->set_value(issues_id);
                continue;
            }

            if (!PreProcessSqlInfo(sql_info.get())) {
                sWarn("Lint::Check PreProcessSqlInfo failed");
                env_.AddToSqlHistory(*sql_info);
				if (sql_info->is_synchronous)
					sql_info->checked_promise->set_value(issues_id);
                continue;
            }

            sql_info->CopyWithoutParse(simple_sql_info);
            env_.AddToSqlHistory(simple_sql_info);

            published_issues->clear();

			ScheduleCheckers(CheckScene::kSample, *sql_info, published_issues);
			ScheduleCheckers(CheckScene::kEverySql, *sql_info, published_issues);

			const std::string& wildcard_sql = sql_info->wildcard_sql_.empty() ? sql_info->sql_ : sql_info->wildcard_sql_;
            bool checked = false;
            if (!checked_sql_cache_.Get(wildcard_sql, checked)) {
                ScheduleCheckers(CheckScene::kUncheckedSql, *sql_info, published_issues);
                checked_sql_cache_.Put(wildcard_sql, true);
            } else {
                sVerbose("Lint::Check() already checked recently");
            }

            if (!published_issues->empty()) {
                sInfo("New check some diagnosis out!, sql=%s", sql_info->sql_.c_str());
                if (issued_callback_) {
                    issued_callback_(env_.GetDbPath().c_str(), *published_issues);
                }
				if (sql_info->is_synchronous) {
					for (auto it = published_issues->begin(); it != published_issues->end(); ++it) {
						issues_id.push_back(it->id);
					}
				}
            }
			if (sql_info->is_synchronous) {
				sql_info->checked_promise->set_value(issues_id);
			}
			
			sql_info = nullptr;
            env_.CheckReleaseHistory();
        }

        sError("check break");
        delete published_issues;
    }

    bool Lint::PreProcessSqlInfo(SqlInfo *sql_info) {
        int processRet = SqlInfoProcessor().Process(sql_info);

        sVerbose("Lint::PreProcessSqlInfo processRet:ret:%d", processRet);

        if (processRet != 0) {
            sError("Lint::PreProcessSqlInfo failed; sql:%s", sql_info->sql_.c_str());
            return false;
        }

        return true;
    }

    void Lint::PreProcessSqlString(std::string &sql){
        trim(sql);
        ToLowerCase(sql);
    }

    void Lint::ScheduleCheckers(const CheckScene check_scene, const SqlInfo& sql_info, std::vector<Issue> *published_issues) {
        std::map<CheckScene, std::vector<Checker*>>::iterator it = checkers_.find(check_scene);
        if (it == checkers_.end()) {
            return;
        }

        std::vector<Checker*> scene_checkers = it->second;
        size_t scene_checkers_cnt = scene_checkers.size();

        for (size_t i=0;i < scene_checkers_cnt;i++) {

            Checker* checker = scene_checkers[i];
            if (check_scene != CheckScene::kSample
                || (env_.GetSqlCnt() % checker->GetSqlCntToSample() == 0)) {
                checker->Check(env_, sql_info, published_issues);
            }

        }
    }

    void Lint::RegisterChecker(const std::string &checker_name) {
        sDebug("Lint::RegisterChecker check_name: %s", checker_name.c_str());
        if (CheckerName::kExplainQueryPlanCheckerName == checker_name) {
            RegisterChecker(new ExplainQueryPlanChecker());
        } else if (CheckerName::kRedundantIndexCheckerName == checker_name) {
            RegisterChecker(new RedundantIndexChecker());
        } else if (CheckerName::kAvoidAutoIncrementCheckerName == checker_name) {
            RegisterChecker(new AvoidAutoIncrementChecker());
        } else if (CheckerName::kAvoidSelectAllCheckerName == checker_name) {
            RegisterChecker(new AvoidSelectAllChecker());
        } else if (CheckerName::kWithoutRowIdBetterCheckerName == checker_name) {
            RegisterChecker(new WithoutRowIdBetterChecker());
        } else if (CheckerName::kPreparedStatementBetterCheckerName == checker_name) {
            RegisterChecker(new PreparedStatementBetterChecker());
		} else if (CheckerName::kPreparedStatementMustCheckerName == checker_name) {
			RegisterChecker(new PreparedStatementMustChecker());
		} else if (CheckerName::kReplaceIntoCheckerName == checker_name) {
			RegisterChecker(new ReplaceIntoChecker());
		} else if (CheckerName::kMergeTransactionCheckerName == checker_name) {
			RegisterChecker(new MergeTransactionChecker());
		} else if (CheckerName::kExecuteSelectContinuousCheckerName == checker_name) {
			RegisterChecker(new ExecuteSelectContinuousChecker());
		}
    }

    void Lint::RegisterChecker(Checker* checker) {
        std::map<CheckScene, std::vector<Checker*>>::iterator iter = checkers_.find(checker->GetCheckScene());

        if (iter != checkers_.end()) {
            iter->second.push_back(checker);
        } else {
            std::vector<Checker*> v;
            v.push_back(checker);
            checkers_.insert(std::pair<CheckScene, std::vector<Checker*>>(checker->GetCheckScene(), v));
        }
    }

    void Lint::SetWhiteList(const std::map<std::string, std::set<std::string>> &white_list) {
        env_.SetWhiteList(white_list);
    }

	Lint::~Lint() {
        sInfo("~Lint");

        std::unique_lock<std::mutex> lock(queue_mutex_);
        exit_ = true;
        queue_cv_.notify_one();
        lock.unlock();

        check_thread_->join();

        for(auto it = checkers_.begin();it != checkers_.end();it++) {
            for (Checker* checker : it->second) {
                delete checker;
            }
            it->second.clear();
        }
        checkers_.clear();

        delete check_thread_;

        sInfo("~Lint Done");
 	}
}
