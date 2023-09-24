#pragma once

#include "search_server.h"
#include "log_duration.h"

#include <execution>
#include <iostream>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace test {
    namespace test_policies {
        #define TEST(policy) Test(#policy, search_server, queries, execution::policy)

        std::string GenerateWord(std::mt19937& generator, int max_length);

        std::vector<std::string> GenerateDictionary(std::mt19937& generator, int word_count, int max_length);

        std::string GenerateQuery(std::mt19937& generator, const std::vector<std::string>& dictionary, int word_count, double minus_prob = 0);

        std::vector<std::string> GenerateQueries(std::mt19937& generator, const std::vector<std::string>& dictionary, int query_count,
                                                 int max_word_count);

        template <typename ExecutionPolicy>
        double Test(std::string_view mark, const SearchServer& search_server, const std::vector<std::string>& queries, ExecutionPolicy&& policy) {
            LOG_DURATION_STREAM(mark, cerr);
            double total_relevance = 0;
            for (const std::string_view query : queries) {
                for (const auto& document : search_server.FindTopDocuments(policy, query)) {
                    total_relevance += document.relevance;
                }
            }
            return total_relevance;
        }

        void TestPolicies();
    } // namespace test_policies
} // namespace test
