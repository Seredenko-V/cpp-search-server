#include "tests.h"

#include <execution>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include <cassert>
#include <cmath>

using namespace std;

namespace test {
    bool IsEqualDouble(double lhs, double rhs, double epsilon = 1e-6) {
        return abs(lhs - rhs) <= epsilon ? true : false;
    }

    namespace test_policies {
        string GenerateWord(mt19937& generator, int max_length) {
            const int length = uniform_int_distribution<int>(1, max_length)(generator);
            string word;
            word.reserve(length);
            for (int i = 0; i < length; ++i) {
                word.push_back(uniform_int_distribution<int>('a', 'z')(generator));
            }
            return word;
        }

        vector<string> GenerateDictionary(mt19937& generator, int word_count, int max_length) {
            vector<string> words;
            words.reserve(word_count);
            for (int i = 0; i < word_count; ++i) {
                words.push_back(GenerateWord(generator, max_length));
            }
            words.erase(unique(words.begin(), words.end()), words.end());
            return words;
        }

        string GenerateQuery(mt19937& generator, const vector<string>& dictionary, int word_count, double minus_prob) {
            string query;
            for (int i = 0; i < word_count; ++i) {
                if (!query.empty()) {
                    query.push_back(' ');
                }
                if (uniform_real_distribution<>(0, 1)(generator) < minus_prob) {
                    query.push_back('-');
                }
                query += dictionary[uniform_int_distribution<int>(0, dictionary.size() - 1)(generator)];
            }
            return query;
        }

        vector<string> GenerateQueries(mt19937& generator, const vector<string>& dictionary, int query_count, int max_word_count) {
            vector<string> queries;
            queries.reserve(query_count);
            for (int i = 0; i < query_count; ++i) {
                queries.push_back(GenerateQuery(generator, dictionary, max_word_count));
            }
            return queries;
        }

        void TestPolicies() {
            cerr << "TestPolicies started..."sv << endl;
            mt19937 generator;
            constexpr int kWordCount = 1'000;
            constexpr int kQueryCount = 10'000;
            const vector<string> dictionary = GenerateDictionary(generator, kWordCount, 10);
            const vector<string> documents = GenerateQueries(generator, dictionary, kQueryCount, 70);
            SearchServer search_server(dictionary[0]);
            for (size_t i = 0; i < documents.size(); ++i) {
                search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, { 1, 2, 3 });
            }
            const vector<string> queries = GenerateQueries(generator, dictionary, 100, 70);
            cerr << "Seq policy test started. Wait..."sv << endl;
            double relevance_seq = TEST(seq);
            cerr << "Par policy test started. Wait..."sv << endl;
            double relevance_par = TEST(par);
            assert(IsEqualDouble(relevance_seq, relevance_par));
            cerr << ">>> TestPolicies has been passed"sv << endl;
        }
    } // namespace test_policies
} // namespace test
