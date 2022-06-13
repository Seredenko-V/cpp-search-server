#pragma once
#include "search_server.h"
#include <vector>
#include <string>
#include <deque>
#include <ctime>

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);

    // "������" ��� ���� ������� ������, ����� ��������� ���������� ��� ����� ����������
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(const std::string& raw_query);
    int GetNoResultRequests() const;

private:
    struct QueryResult {
        std::string text_query;
        std::vector<Document> documents;
    };
    std::deque<QueryResult> requests_;
    const static time_t min_in_day_ = 1440;
    const SearchServer& search_server_;
    size_t nothing_was_found_ = 0; // ���-�� �������� ��� ����������
    void SearchResults(const std::string& raw_query, std::vector<Document>& result_find);
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, 
    DocumentPredicate document_predicate) {
    std::vector<Document> result_find = search_server_.FindTopDocuments(raw_query, document_predicate);
    SearchResults(raw_query, result_find);
    return result_find;
}