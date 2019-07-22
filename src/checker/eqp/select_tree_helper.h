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
// Created by wuzhiwen on 2017/3/8.
//

#ifndef SQLITE_LINT_CHECKER_EQP_SELECT_TREE_HELPER_H
#define SQLITE_LINT_CHECKER_EQP_SELECT_TREE_HELPER_H

#include <string>
#include <vector>
#include <map>
#include "core/lint_info.h"
#include "core/lint_env.h"

namespace sqlitelint {
    class SelectTreeHelper {
    public:
        SelectTreeHelper(Select *select);
        ~SelectTreeHelper();
        void Process();
        Select* GetSelect(const std::string& table);
        bool HasUsingOrOn();
        bool HasFuzzyMatching();
        bool HasBitOperation();
		bool HasAnd();
        bool HasOr();
        bool HasIn();
		const std::vector<std::vector<std::string>>& getColumns();

    private:
        void ProcessExprList(const ExprList *p);
        void ProcessExpr(const Expr *p);
        void ProcessSrcList(const SrcList *p, Select *s);
        void ProcessSelect(Select *p);
		void AddSelectTree(const char *table, Select *p);
        void ProcessToken(const Expr *p, int op);
		inline bool checkOP(int op);
		inline std::string getIDfromExpr(const Expr* p);

		void GetColumnsConnectedByAnd(const Expr* p, std::vector<std::string>& parallelColomuns);

        std::map<std::string, std::vector<Select*>> select_tree_map_;
		std::vector<std::vector<std::string>> columns_to_check_index;
        Select *select_;
        bool has_using_or_on;
        bool has_fuzzy_matching;
        bool has_bit_operation;
		bool has_and;
        bool has_or;
        bool has_in;

    };
}

#endif //SQLITE_LINT_CHECKER_EQP_SELECT_TREE_HELPER_H
