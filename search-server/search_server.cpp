#include "search_server.h"

using namespace std;

SearchServer::SearchServer(const string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
{
}

void SearchServer::AddDocument(int document_id, const string& document, DocumentStatus status, 
    const vector<int>& ratings) {
    if (documents_.count(document_id) > 0) {
        throw invalid_argument("A document with this ID already exists."s);
    } else if (document_id < 0) {
        throw invalid_argument("A document cannot have a negative ID."s);
    } else if (!IsValidWord(document)) {
        throw invalid_argument("The content of the document contains invalid characters."s);
    }
    const vector<string> words = SplitIntoWordsNoStop(document);
    //words_in_documents_[document_id] = words;
    const double inv_word_count = 1.0 / words.size();
    for (const string& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        //words_in_documents_[document_id].push_back(word);
        word_frequencies_in_document_[document_id][word] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    order_addition_document_.insert(document_id);
}

vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
        });
}

vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

const map<string, double>& SearchServer::GetWordFrequencies(int document_id) const {
    if (!documents_.count(document_id)) {
        return empty_;
    }
    return word_frequencies_in_document_.at(document_id);
}

set<int>::const_iterator SearchServer::begin() {
    return order_addition_document_.begin();
}

set<int>::const_iterator SearchServer::end() {
    return order_addition_document_.end();
}

void SearchServer::RemoveDocument(int document_id) {
    if (!documents_.count(document_id)) {
        throw invalid_argument("There is no document with the specified ID.");
    }
    // log(количество документов) * количество слов в удаляемом документе, 
    // т.к. у каждого документа свой словарь
    for (const auto& [word, freq] : word_frequencies_in_document_.at(document_id)) {
        // log(количество слов во всех документах)
        word_to_document_freqs_[word].erase(document_id);
    }
    word_frequencies_in_document_.erase(document_id);
    documents_.erase(document_id);
    order_addition_document_.erase(document_id);
}

tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(const string& raw_query, int document_id) const {
    LOG_DURATION_STREAM("MatchDocument"s, cout);
    if (document_id < 0 || !IsValidWord(raw_query)) {
        throw invalid_argument("Invalid request.");
    }
    const Query query = ParseQuery(raw_query);
    vector<string> matched_words;
    for (const string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }
    return { matched_words, documents_.at(document_id).status };
}

//int SearchServer::GetDocumentId(int serial_number) const {
//    if (serial_number < 0 || serial_number >= static_cast<int>(order_addition_document_.size())) {
//        throw out_of_range("Неверное значение индекса."s);
//    }
//    return order_addition_document_[serial_number];
//}

bool SearchServer::IsStopWord(const string& word) const {
    return stop_words_.count(word) > 0;
}

vector<string> SearchServer::SplitIntoWordsNoStop(const string& text) const {
    vector<string> words;
    for (const string& word : SplitIntoWords(text)) {
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string text) const {
    bool is_minus = false;
    // Word shouldn't be empty
    if (text.empty()) {
        throw invalid_argument("There is an empty word in the query.");
    }
    if (text[0] == '-') {
        if (text[1] == '-') {
            throw invalid_argument("The request contains two \"-\" characters in a row.");
        }
        is_minus = true;
        text = text.substr(1);
    }
    return { text, is_minus, IsStopWord(text) };
}

SearchServer::Query SearchServer::ParseQuery(const string& text) const {
    Query query;
    for (const string& word : SplitIntoWords(text)) {
        if (word == "-"s) {
            throw invalid_argument("There is no word after the \"-\" sign.");
        }
        QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.insert(query_word.data);
            } else {
                query.plus_words.insert(query_word.data);
            }
        }
    }
    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(const string& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

bool SearchServer::IsValidWord(const string& word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}