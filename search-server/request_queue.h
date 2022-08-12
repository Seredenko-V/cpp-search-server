#pragma once
#include "search_server.h"
#include <vector>
#include <string>
#include <deque>
#include <ctime>

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);

    // "обёртки" для всех методов поиска, чтобы сохранять результаты для нашей статистики
    template <typename ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> AddFindRequest(ExecutionPolicy policy, const std::string& raw_query, 
        DocumentPredicate document_predicate);
    template <typename ExecutionPolicy>
    std::vector<Document> AddFindRequest(ExecutionPolicy policy, const std::string& raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const;

private:
    struct QueryResult {
        size_t quantity_documents;
    };
    std::deque<QueryResult> requests_;
    const static time_t min_in_day_ = 1440;
    const SearchServer& search_server_;
    int nothing_was_found_ = 0; // кол-во запросов без результата
    void SearchResults(const std::string& raw_query, std::vector<Document>& result_find);
};

template <typename ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(ExecutionPolicy policy, const std::string& raw_query,
    DocumentPredicate document_predicate) {
    std::vector<Document> result_find = search_server_.FindTopDocuments(policy, raw_query, document_predicate);
    SearchResults(raw_query, result_find);
    return result_find;
}

template <typename ExecutionPolicy>
std::vector<Document> RequestQueue::AddFindRequest(ExecutionPolicy policy, const std::string& raw_query, DocumentStatus status) {
    std::vector<Document> result_find = search_server_.FindTopDocuments(policy, raw_query, status);
    SearchResults(raw_query, result_find);
    return result_find;
}