#pragma once
#include "string_processing.h"
#include "document.h"
#include "concurrent_map.h"
#include "log_duration.h"

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <set>
#include <map>
#include <tuple>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <execution>
#include <utility>
#include <functional>
#include <iterator>
#include <type_traits>
#include <future>

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double DELTA = 1e-6;

class SearchServer {
public:
    template <typename StringContainer>
    SearchServer(const StringContainer& stop_words);
    // Invoke delegating constructor from string container
    explicit SearchServer(const std::string& stop_words_text);
    explicit SearchServer(const std::string_view stop_words_text);

    void AddDocument(int document_id, const std::string_view document, DocumentStatus status,
        const std::vector<int>& ratings);

    template <typename ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(ExecutionPolicy policy, const std::string_view raw_query, DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const;
    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy policy, const std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentStatus status) const;
    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy policy, const std::string_view raw_query) const;
    std::vector<Document> FindTopDocuments(const std::string_view raw_query) const;

    using words_and_status_document = std::tuple<std::vector<std::string_view>, DocumentStatus>;
    // ѕоиск интересующих слов в конкретном документе
    words_and_status_document MatchDocument(const std::string_view raw_query, int document_id) const;
    words_and_status_document MatchDocument(std::execution::sequenced_policy,
        const std::string_view raw_query, int document_id) const;
    words_and_status_document MatchDocument(std::execution::parallel_policy,
        const std::string_view raw_query, int document_id) const;

    int GetDocumentCount() const;
    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;
    void RemoveDocument(int document_id);
    void RemoveDocument(std::execution::sequenced_policy, int document_id);
    void RemoveDocument(std::execution::parallel_policy, int document_id);

    std::set<int>::const_iterator begin();
    std::set<int>::const_iterator end();

private:

    std::set<int> order_addition_document_;
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    const std::set<std::string, std::less<>> stop_words_;
    // частота слова в каждом документе
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> documents_;
    // частоты каждого слова в документе
    std::map<int, std::map<std::string, double, std::less<>>> word_frequencies_in_document_;

    bool IsStopWord(const std::string_view word) const;
    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view text) const;
    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string_view text) const;

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    Query ParseQuery(const std::string_view text, bool sequenced_policy = true) const;
    // Existence required
    double ComputeWordInverseDocumentFreq(const std::string_view word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(std::execution::sequenced_policy, const Query& query, 
        DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(std::execution::parallel_policy, const Query& query,
        DocumentPredicate document_predicate) const;

    static bool IsValidWord(const std::string_view word);
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
    bool is_valid_words = std::all_of(stop_words.begin(), stop_words.end(), [this](const auto& word) {
        return IsValidWord(word);
    });
    if (!is_valid_words) {
        throw std::invalid_argument("—топ-слова содержат недопустимые символы."s);
    }
}

template <typename ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy policy, const std::string_view raw_query, DocumentPredicate document_predicate) const {
    const Query query = ParseQuery(raw_query);
    if (!IsValidWord(raw_query)) {
        throw std::invalid_argument("—одержимое запроса содержит недопустимые символы"s);
    }
    std::vector<Document> matched_documents = FindAllDocuments(policy, query, document_predicate);

    sort(policy, matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
        if (std::abs(lhs.relevance - rhs.relevance) < DELTA) {
            return lhs.rating > rhs.rating;
        } else {
            return lhs.relevance > rhs.relevance;
        }
    });

    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const {
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy policy, const std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(policy, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
    });
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy policy, const std::string_view raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(std::execution::sequenced_policy, const SearchServer::Query& query,
    DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;
    for (const std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (const std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(std::execution::parallel_policy policy, const Query& query,
    DocumentPredicate document_predicate) const {
    ConcurrentMap<int, double> document_to_relevance(std::thread::hardware_concurrency());

    for_each(policy, query.plus_words.begin(), query.plus_words.end(), 
        [&] (const std::string_view word) {
            if (word_to_document_freqs_.count(word) != 0) {
                const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                for_each(word_to_document_freqs_.at(word).begin(), word_to_document_freqs_.at(word).end(),
                    [this, &document_to_relevance, inverse_document_freq, document_predicate]
                        (const std::pair<int, double> relevance_doc) {
                        const auto& document_data = documents_.at(relevance_doc.first);
                        if (document_predicate(relevance_doc.first, document_data.status, document_data.rating)) {
                            document_to_relevance[relevance_doc.first] += relevance_doc.second * inverse_document_freq;
                        }
                    });
            }
        });

    for_each(policy, query.minus_words.begin(), query.minus_words.end(), 
        [&] (const std::string_view word) {
            if (word_to_document_freqs_.count(word) != 0) {
                for_each(word_to_document_freqs_.at(word).begin(), word_to_document_freqs_.at(word).end(), 
                    [&document_to_relevance] (const std::pair<int, double> relevance_doc) {
                        document_to_relevance.Erase(relevance_doc.first);
                    });
            }
        });

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}