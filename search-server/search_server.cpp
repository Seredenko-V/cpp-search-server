#include "search_server.h"
#include <iostream>

using namespace std;

SearchServer::SearchServer(const string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text)) { // Вызвать делегирующий конструктор из контейнера string
}

SearchServer::SearchServer(const string_view stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text)) {
}

void SearchServer::AddDocument(int document_id, const string_view document, DocumentStatus status,
    const vector<int>& ratings) {
    //LOG_DURATION_STREAM("ADD"s, cerr);
    if (documents_.count(document_id) > 0) {
        throw invalid_argument("A document with this ID already exists."s);
    } else if (document_id < 0) {
        throw invalid_argument("A document cannot have a negative ID."s);
    } else if (!IsValidWord(document)) {
        throw invalid_argument("The content of the document contains invalid characters."s);
    }
    const vector<string_view> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for (const string_view word : words) {
        word_frequencies_in_document_[document_id][string(word)] += inv_word_count;
        word_to_document_freqs_[(word_frequencies_in_document_[document_id].find(word)->first)][document_id] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    order_addition_document_.insert(document_id);
}

vector<Document> SearchServer::FindTopDocuments(const string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(execution::seq, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
    });
}

vector<Document> SearchServer::FindTopDocuments(const string_view raw_query) const {
    return FindTopDocuments(execution::seq, raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return static_cast<int>(documents_.size());
}

const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    if (!documents_.count(document_id)) {
        static map<string_view, double> empty;
        return empty;
    }
    return (map<string_view, double>&) word_frequencies_in_document_.at(document_id);
}

set<int>::const_iterator SearchServer::begin() {
    return order_addition_document_.begin();
}

set<int>::const_iterator SearchServer::end() {
    return order_addition_document_.end();
}

void SearchServer::RemoveDocument(int document_id) {
    SearchServer::RemoveDocument(execution::seq, document_id);
}

void SearchServer::RemoveDocument(execution::sequenced_policy, int document_id) {
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

void SearchServer::RemoveDocument(execution::parallel_policy, int document_id) {
    if (!documents_.count(document_id)) {
        throw invalid_argument("There is no document with the specified ID");
    }
    // для распараллеливания for
    map<string, double, less<>>& frequency_word_in_each_document = word_frequencies_in_document_.at(document_id); // частоты слов в документе
    vector<const string*> words(frequency_word_in_each_document.size());
    // параллельное перекладывание указателей на слова в вектор
    transform(execution::par, frequency_word_in_each_document.begin(), frequency_word_in_each_document.end(),
        words.begin(),
        [](const pair<const string&, double>& ptr_to_word) {
            return &ptr_to_word.first;
        });
    // удаление указанного id из ассоциаций с каждым словом
    for_each(execution::par, words.begin(), words.end(),
        [this, document_id](const string* word) {
            word_to_document_freqs_[*word].erase(document_id);
        });
    word_frequencies_in_document_.erase(document_id);
    documents_.erase(document_id);
    order_addition_document_.erase(document_id);
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const string_view raw_query, int document_id) const {
    return MatchDocument(execution::seq, raw_query, document_id);
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(execution::sequenced_policy policy,
    const string_view raw_query, int document_id) const {
    //LOG_DURATION_STREAM("MatchDocument"s, cout);
    if (document_id < 0 || word_frequencies_in_document_.count(document_id) == 0) {
        throw out_of_range("There is no document with the specified ID");
    }
    const Query query = ParseQuery(raw_query);
    if (any_of(policy, query.minus_words.begin(), query.minus_words.end(),
        [this, document_id](const string_view minus_word) {
            return word_to_document_freqs_.at(minus_word).count(document_id);
        })) {
        return { vector<string_view>(), documents_.at(document_id).status };
    }
    vector<string_view> matched_words(query.plus_words.size());

    vector<string_view>::iterator end_new_size = copy_if(policy, query.plus_words.begin(), query.plus_words.end(),
        matched_words.begin(),
        [this, document_id](const string_view plus_word) {
            return word_to_document_freqs_.at(plus_word).count(document_id);
        });

    matched_words.resize(distance(matched_words.begin(), end_new_size));
    set<string_view> unique_words(matched_words.begin(), matched_words.end());

    return { vector<string_view> { unique_words.begin(), unique_words.end() }, documents_.at(document_id).status };
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(execution::parallel_policy policy,
    const string_view raw_query, int document_id) const {
    if (document_id < 0 || word_frequencies_in_document_.count(document_id) == 0) {
        throw out_of_range("There is no document with the specified ID");
    }
    Query query = ParseQuery(raw_query, false);

    if (any_of(policy, query.minus_words.begin(), query.minus_words.end(),
        [this, document_id](const string_view minus_word) {
            return word_to_document_freqs_.at(minus_word).count(document_id);
        })) {
        return { vector<string_view>(), documents_.at(document_id).status };
    }
    vector<string_view> matched_words(query.plus_words.size());

    vector<string_view>::iterator end_new_size = copy_if(policy, query.plus_words.begin(), query.plus_words.end(),
        matched_words.begin(),
        [this, document_id](const string_view plus_word) {
            return word_to_document_freqs_.at(plus_word).count(document_id);
        });

    matched_words.resize(distance(matched_words.begin(), end_new_size));
    set<string_view> unique_words(matched_words.begin(), matched_words.end());

    return { vector<string_view> { unique_words.begin(), unique_words.end() }, documents_.at(document_id).status };
}

bool SearchServer::IsStopWord(const string_view word) const {
    return stop_words_.count(word) > 0;
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(const string_view text) const {
    vector<string_view> words;
    for (const string_view word : SplitIntoWords(text)) {
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

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
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

SearchServer::Query SearchServer::ParseQuery(const string_view text, bool sequenced_policy) const {
    Query query;
    for (const string_view word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Invalid search query.");
        }
        if (word == "-"s) {
            throw invalid_argument("There is no word after the \"-\" sign.");
        }
        QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.push_back(query_word.data);
            } else {
                query.plus_words.push_back(query_word.data);
            }
        }
    }
    if (sequenced_policy) {
        sort(query.plus_words.begin(), query.plus_words.end());
        query.plus_words.erase(unique(query.plus_words.begin(), query.plus_words.end()), query.plus_words.end());

        sort(query.minus_words.begin(), query.minus_words.end());
        query.minus_words.erase(unique(query.minus_words.begin(), query.minus_words.end()), query.minus_words.end());
    }
    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(const string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

bool SearchServer::IsValidWord(const string_view word) {
    // Допустимое слово не должно содержать специальных символов
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}
