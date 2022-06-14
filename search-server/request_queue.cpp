#include "request_queue.h"

using namespace std;

RequestQueue::RequestQueue(const SearchServer& search_server)
    : search_server_(search_server) {
}

vector<Document> RequestQueue::AddFindRequest(const string& raw_query, DocumentStatus status) {
    vector<Document> result_find = search_server_.FindTopDocuments(raw_query, status);
    SearchResults(raw_query, result_find);
    return result_find;
}

vector<Document> RequestQueue::AddFindRequest(const string& raw_query) {
    vector<Document> result_find = search_server_.FindTopDocuments(raw_query);
    SearchResults(raw_query, result_find);
    return result_find;
}

int RequestQueue::GetNoResultRequests() const {
    return nothing_was_found_;
}

void RequestQueue::SearchResults(const string& raw_query, vector<Document>& result_find) {
    if (requests_.size() >= min_in_day_) {
        requests_.pop_front();
        if (nothing_was_found_ != 0) {
            --nothing_was_found_;
        }
    }
    requests_.push_back({ result_find.size() });
    if (result_find.empty()) {
        ++nothing_was_found_;
    }
}