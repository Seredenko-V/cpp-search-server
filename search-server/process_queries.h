#pragma once

#include "search_server.h"
#include <vector>
#include <string>
#include <list>
#include <algorithm>
#include <execution>

template <typename Iterator>
class AllFindedDocuments {
public:
    AllFindedDocuments() = default;

    AllFindedDocuments(const std::vector<std::vector<Document>>& finded_documents) {
        for (const std::vector<Document>& documents_one_query : finded_documents) {
            for (const Document& document : documents_one_query) {
                all_finded_documents_.push_back(&document);
            }
        }
    }

    Iterator begin() {
        return all_finded_documents_.begin();
    }

    Iterator end() {
        return all_finded_documents_.end();
    }

private:
    std::list<Iterator> all_finded_documents_;
};

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server,
    const std::vector<std::string>& queries);

std::list<Document> ProcessQueriesJoined(const SearchServer& search_server, 
    const std::vector<std::string>& queries);