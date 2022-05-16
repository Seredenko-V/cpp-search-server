#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <numeric>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double DELTA = 1e-6;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id,
            DocumentData{
                ComputeAverageRating(ratings),
                status
            });
    }

    template <typename FilteringParameter>
    vector<Document> FindTopDocuments(const string& raw_query, FilteringParameter filtering_parameter) const {
        const Query query = ParseQuery(raw_query);

        vector<Document> matched_documents = FindAllDocuments(query, filtering_parameter);
        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < DELTA) {
                    return lhs.rating > rhs.rating;
                }
                else {
                    return lhs.relevance > rhs.relevance;
                }
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }


    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::ACTUAL; });
    }

    vector<Document> FindTopDocuments(const string& raw_query, const DocumentStatus& status) const {
        return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus status_document, int rating) { return status_document == status; });
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
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

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {
            text,
            is_minus,
            IsStopWord(text)
        };
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                }
                else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename FilteringParameter>
    vector<Document> FindAllDocuments(const Query& query, FilteringParameter filtering_parameter) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const DocumentData current_document = documents_.at(document_id);
                if (filtering_parameter(document_id, current_document.status, current_document.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                document_id,
                relevance,
                documents_.at(document_id).rating
                });
        }
        return matched_documents;
    }
};

template <typename First, typename Second>
ostream& operator<<(ostream& out, const pair<First, Second>& container) {
    out << container.first << ": "s << container.second;
    return out;
}

template <typename Type>
void Print(ostream& out, const Type& container) {
    bool flag = false;
    for (const auto& element : container) {
        if (!flag) {
            out << element;
            flag = true;
        }
        else {
            out << ", "s << element;
        }
    }
}

template <typename Type>
ostream& operator<<(ostream& out, const vector<Type>& container) {
    out << "["s;
    Print(out, container);
    out << "]"s;
    return out;
}

template <typename Type>
ostream& operator<<(ostream& out, const set<Type>& container) {
    out << "{"s;
    Print(out, container);
    out << "}"s;
    return out;
}

template <typename Key, typename Value>
ostream& operator<<(ostream& out, const map<Key, Value>& container) {
    out << "{"s;
    Print(out, container);
    out << "}"s;
    return out;
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
    const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
    const string& hint) {
    if (!value) {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename TestFunc>
void RunTestImpl(const TestFunc& foo, const string& func_name) {
    foo(); // запуск теста
    cerr << func_name << " OK"s << endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    /* Сначала убеждаемся, что поиск слова, не входящего в список стоп - слов,
    находит нужный документ */
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    /* Затем убеждаемся, что поиск этого же слова, входящего в список стоп - слов,
    возвращает пустой результат */
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

// Тест проверяет исключение из результатов поиска документов, содержащих минус-слова
void TestExcludeDocumentsContainingMinusWords() {
    const vector<int> doc_id = { 1,2 };
    const vector<string> content = { "cat on the street of the city"s, "a dog on Pushkin street"s };
    const vector<int> ratings = { 1, 2, 3 };
    /* Убеждаемся, что поиск документов по запросу без минус-слов работает корректно */
    {
        SearchServer server;
        server.SetStopWords("a on of in the"s);
        server.AddDocument(doc_id[0], content[0], DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id[1], content[1], DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("street"s);
        ASSERT_EQUAL(found_docs.size(), 2u);
        ASSERT_EQUAL(found_docs[0].id, doc_id[0]);
        ASSERT_EQUAL(found_docs[1].id, doc_id[1]);
    }
    /* Проверяем корректность поиска при наличии минус-слова в запросе */
    {
        SearchServer server;
        server.SetStopWords("a on of in the"s);
        server.AddDocument(doc_id[0], content[0], DocumentStatus::ACTUAL, ratings);
        {
            const auto found_docs = server.FindTopDocuments("-cat on the street"s);
            ASSERT_EQUAL(found_docs.size(), 0u);
        }
        server.AddDocument(doc_id[1], content[1], DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("-cat on the street"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, doc_id[1]);
    }
}

// Тест проверяет корректность возврата всех слов запроса, присутствующих в документе
void TestMatchDocument() {
    const vector<int> doc_id = { 1,2 };
    const vector<string> content = { "cat on the street of the city"s, "a dog on Pushkin street"s };
    const vector<int> ratings = { 1, 2, 3 };
    SearchServer server;
    server.SetStopWords("a on of in the"s);
    server.AddDocument(doc_id[0], content[0], DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id[1], content[1], DocumentStatus::ACTUAL, ratings);
    /* Убеждаемся, что возврат совпадающих слов запроса со словами документа
    работает корректно без минус-слов в запросе */
    {
        string query = "cat in the city"s;
        //const auto matched_documents = server.MatchDocument(query, doc_id[0]);
        const auto& [words_doc, status_doc] = server.MatchDocument(query, doc_id[0]);
        vector<string> result_matched = { "cat"s, "city"s };
        ASSERT_EQUAL(words_doc, result_matched);
    }
    /* Убеждаемся, что возврат совпадающих слов запроса со словами документа
    работает корректно при наличии минус-слов в запросе */
    {
        string query = "-dog on the street"s;
        const auto& [words_doc, status_doc] = server.MatchDocument(query, doc_id[1]);
        vector<string> result_matched;
        ASSERT_EQUAL(words_doc, result_matched);
    }
}

// Тест проверяет корректность сортировки найденных документов по релевантности
void TestRelevanceSorting() {
    const vector<int> doc_id = { 1,2,3 };
    const vector<string> content = { "cat on the street of the city"s, "a dog on Pushkin street"s, "penguin in the subway"s };
    const vector<int> ratings = { 1, 2, 3 };
    SearchServer server;
    server.SetStopWords("a on of in the"s);
    server.AddDocument(doc_id[0], content[0], DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id[1], content[1], DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id[2], content[2], DocumentStatus::ACTUAL, ratings);
    const vector<Document> found_docs = server.FindTopDocuments("cat on the street"s);
    ASSERT_EQUAL(found_docs.size(), 2u);
    ASSERT(found_docs[0].relevance > found_docs[1].relevance);
    double real_relevance = (log(server.GetDocumentCount() * 1.0 / 1) * (1.0 / 3))
        + (log(server.GetDocumentCount() * 1.0 / 2) * (1.0 / 3));
    ASSERT(abs(found_docs[0].relevance - real_relevance) <= DELTA);
}

// Тест проверяет корректность вычисления среднего рейтинга документов
void TestCalculatingRating() {
    const vector<int> doc_id = { 1,2 };
    const vector<string> content = { "cat on the street of the city"s, "a dog on Pushkin street"s };
    const vector<vector<int>> ratings = { { 1, 2, 3 }, { 4, 5, 6 } };
    SearchServer server;
    server.SetStopWords("a on of in the"s);
    server.AddDocument(doc_id[0], content[0], DocumentStatus::ACTUAL, ratings[0]);
    server.AddDocument(doc_id[1], content[1], DocumentStatus::ACTUAL, ratings[1]);
    const vector<Document> found_docs = server.FindTopDocuments("cat on the street"s);
    ASSERT(found_docs[0].rating < found_docs[1].rating);
    ASSERT_EQUAL(found_docs[0].rating, (1 + 2 + 3) / 3); // необязательна, для личного успокоения
}

// Тест проверяет корректность фильтрации поиска в соответствие предикату
void TestFilteringPredicate() {
    const vector<int> doc_id = { 1,2,3,4 };
    const vector<string> content = {
        "cat on the street of the city"s,
        "a dog on Pushkin street"s,
        "cat in the Magnit store"s,
        "giraffe in the subway of St. Petersburg"s };
    const vector<int> ratings = { 1,2,3 };
    SearchServer server;
    server.SetStopWords("a on of in the"s);
    server.AddDocument(doc_id[0], content[0], DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id[1], content[1], DocumentStatus::IRRELEVANT, ratings);
    server.AddDocument(doc_id[2], content[2], DocumentStatus::BANNED, ratings);
    server.AddDocument(doc_id[3], content[3], DocumentStatus::REMOVED, ratings);
    const vector<Document> found_docs = server.FindTopDocuments("cat on the street"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
    ASSERT_EQUAL(found_docs.size(), 1u);
    ASSERT_EQUAL(found_docs[0].id, 2u);
}

// Тест проверяет корректность поиска документов, имеющих заданный статус
void TestSearchDocumentWithStatus() {
    const vector<int> doc_id = { 1,2,3,4 };
    const vector<string> content = {
        "cat on the street of the city"s,
        "a dog on Pushkin street"s,
        "cat in the Magnit store"s,
        "giraffe in the subway of St. Petersburg"s };
    const vector<int> ratings = { 1,2,3 };
    SearchServer server;
    server.SetStopWords("a on of in the"s);
    server.AddDocument(doc_id[0], content[0], DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id[1], content[1], DocumentStatus::BANNED, ratings);
    server.AddDocument(doc_id[2], content[2], DocumentStatus::BANNED, ratings);
    server.AddDocument(doc_id[3], content[3], DocumentStatus::REMOVED, ratings);
    const vector<Document> found_docs = server.FindTopDocuments("cat on the street"s, DocumentStatus::BANNED);
    ASSERT_EQUAL(found_docs.size(), 2u);
    ASSERT_EQUAL(found_docs[0].id, 2u); // т.к. при одинаковых параметрах сортировка по лексикограф. признаку
    ASSERT_EQUAL(found_docs[1].id, 3u);
}

// Тест проверяет корректность вычисления релевантности найденных документов
void TestCalculatingRelevance() {
    const vector<int> doc_id = { 1,2 };
    const vector<string> content = { "cat on the cat of the city"s, "a dog on Pushkin street"s };
    const vector<vector<int>> ratings = { { 1, 2, 3 }, { 4, 5, 6 } };
    SearchServer server;
    server.SetStopWords("a on of in the"s);
    server.AddDocument(doc_id[0], content[0], DocumentStatus::ACTUAL, ratings[0]);
    server.AddDocument(doc_id[1], content[1], DocumentStatus::ACTUAL, ratings[1]);
    const vector<Document> found_docs = server.FindTopDocuments("cat on the street"s);
    ASSERT(abs(found_docs[0].relevance - (log(server.GetDocumentCount() * 1.0 / 1) * (2.0 / 3))) <= DELTA);
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludeDocumentsContainingMinusWords);
    RUN_TEST(TestMatchDocument);
    RUN_TEST(TestRelevanceSorting);
    RUN_TEST(TestCalculatingRating);
    RUN_TEST(TestFilteringPredicate);
    RUN_TEST(TestSearchDocumentWithStatus);
    RUN_TEST(TestCalculatingRelevance);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}