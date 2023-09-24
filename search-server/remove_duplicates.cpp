#include "remove_duplicates.h"

using namespace std;

void RemoveDuplicates(SearchServer& search_server) {
    set<int> documents_for_delete; // перечень дубликатов
    set<set<string_view>> original_documents;
    for (const int document_id : search_server) {
        set<string_view> words_document;
        for (const auto& [word, freq] : search_server.GetWordFrequencies(document_id)) {
            words_document.insert(word);
        }
        if (original_documents.count(words_document)) {
            documents_for_delete.insert(document_id);
        } else {
            original_documents.insert(words_document);
        }
    }
    for (const int id : documents_for_delete) {
        cout << "Found duplicate document id "s << id << endl;
        search_server.RemoveDocument(id);
    }
}
