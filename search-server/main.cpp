#include <algorithm>
#include <cassert>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <iostream>
#include <tuple>
#include <typeinfo>

using namespace std;

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

#define ASSERT(expr) AssertImpl((expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl((expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename F>
void RunTestImpl(const F& func, const string& func_str) {
    func();
    cerr << func_str << " OK"s << endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double CORRECTION = 1e-6;

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

    void AddDocument(int document_id, const string& document, DocumentStatus status,
        const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    }

    template <typename Predictor>
    vector<Document> FindTopDocuments(const string& raw_query, Predictor filter) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, filter);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < CORRECTION) {
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

    vector<Document> FindTopDocuments(const string& raw_query,
        DocumentStatus status = DocumentStatus::ACTUAL) const {
        return FindTopDocuments(raw_query,
            [&status](int document_id, DocumentStatus sts, int rating)
            {
                return sts == status;
            });
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
        int document_id) const {
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
    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
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
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return { text, is_minus, IsStopWord(text) };
    }

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

    template <typename Predictor>
    vector<Document> FindAllDocuments(const Query& query, Predictor filter) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                if (filter(document_id, documents_.at(document_id).status, documents_.at(document_id).rating)) {
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
            matched_documents.push_back(
                { document_id, relevance, documents_.at(document_id).rating });
        }
        return matched_documents;
    }
};

// -------- Начало модульных тестов поисковой системы ----------
// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                    "Stop words must be excluded from documents"s);
    }
}

// Поддержка минус-слов. Документы, содержащие минус-слова поискового запроса, не должны включаться в результаты поиска
void TestExcludeDocsContainsMinusWords() {
    const vector<int> ratings = {1, 2, 3};
    SearchServer server;

    server.SetStopWords("in the"s);
    server.AddDocument(0, "big cat in the city"s, DocumentStatus::ACTUAL, ratings);
    ASSERT(server.FindTopDocuments("-big cat"s).empty());

    server.AddDocument(1, "small cat in the city"s, DocumentStatus::ACTUAL, ratings);
    ASSERT_EQUAL(server.FindTopDocuments("-big cat"s)[0].id, 1);
}

// При матчинге документа по поисковому запросу должны быть возвращены все слова из поискового запроса, присутствующие в документе. 
// Если есть соответствие хотя бы по одному минус-слову, должен возвращаться пустой список слов.
void TestDocsMatching() {
    const vector<int> ratings = {1, 2, 3};
    {   
        const string content = "-big cat in the city"s;
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(0, "big cat in the city"s, DocumentStatus::ACTUAL, ratings);
        auto [words, docstatus] = server.MatchDocument(content, 0);
        ASSERT(words.empty());
    }

    {
        const string content = "big cat in the city"s;
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(0, "big cat in the city"s, DocumentStatus::ACTUAL, ratings);
        auto [words, docstatus] = server.MatchDocument(content, 0);
        ASSERT_EQUAL(words.size(), 3);
    }
}

// Сортировка найденных документов по релевантности. Возвращаемые при поиске документов результаты должны 
// быть отсортированы в порядке убывания релевантности.
void TestDocsSortByRelevance() {
    const vector<int> ratings = {1, 2, 3};
    SearchServer server;
    server.SetStopWords("in the"s);
    server.AddDocument(0, "big cat in the village"s, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(1, "big grey cat in the city"s, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(2, "big cat in the city"s, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(3, "grey cat in the town"s, DocumentStatus::ACTUAL, ratings);
    vector<Document> documents = server.FindTopDocuments("big grey cat in the city"s);
    ASSERT_EQUAL(documents[0].id, 1);
    ASSERT(documents[1].relevance < documents[0].relevance);
    ASSERT(documents[2].relevance < documents[1].relevance);
    ASSERT(documents[3].relevance < documents[2].relevance);   
}

// Вычисление рейтинга документов. Рейтинг добавленного документа равен среднему арифметическому оценок документа.
void TestCalculateRating() {
    {
        const vector<int> ratings = {1, 2, 3};
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(0, "cat in the village"s, DocumentStatus::ACTUAL, ratings);
        vector<Document> documents = server.FindTopDocuments("cat in the city"s);
        ASSERT_EQUAL(documents[0].rating, 2);
    }

    {
        const vector<int> ratings = {0, 0};
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(0, "cat in the village"s, DocumentStatus::ACTUAL, ratings);
        vector<Document> documents = server.FindTopDocuments("cat in the city"s);
        ASSERT_EQUAL(documents[0].rating, 0);
    }

    {
        const vector<int> ratings = {-1, -2, -3};
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(0, "cat in the village"s, DocumentStatus::ACTUAL, ratings);
        vector<Document> documents = server.FindTopDocuments("cat in the city"s);
        ASSERT_EQUAL(documents[0].rating, -2);
    }

    {
        const vector<int> ratings = {-1, -1, 5};
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(0, "cat in the village"s, DocumentStatus::ACTUAL, ratings);
        vector<Document> documents = server.FindTopDocuments("cat in the city"s);
        ASSERT_EQUAL(documents[0].rating, 1);
    }

}

void TestFindDocsWithStatus() {
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(0, "big cat in the village"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(1, "big cat in the city"s, DocumentStatus::IRRELEVANT, ratings);
        server.AddDocument(2, "big cat in the city"s, DocumentStatus::BANNED, ratings);
        server.AddDocument(3, "big cat in the city"s, DocumentStatus::REMOVED, ratings); 
        vector<Document> documents = server.FindTopDocuments("big cat in the city"s);
        ASSERT_EQUAL(documents[0].id, 0);
        ASSERT_EQUAL(documents.size(), 1u); 
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(0, "big cat in the village"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(1, "big cat in the city"s, DocumentStatus::IRRELEVANT, ratings);
        server.AddDocument(2, "big cat in the city"s, DocumentStatus::BANNED, ratings);
        server.AddDocument(3, "big cat in the city"s, DocumentStatus::REMOVED, ratings); 
        vector<Document> documents = server.FindTopDocuments("big cat in the city"s, DocumentStatus::ACTUAL);
        ASSERT_EQUAL(documents[0].id, 0);
        ASSERT_EQUAL(documents.size(), 1u); 
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(0, "big cat in the village"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(1, "big cat in the city"s, DocumentStatus::IRRELEVANT, ratings);
        server.AddDocument(2, "big cat in the city"s, DocumentStatus::BANNED, ratings);
        server.AddDocument(3, "big cat in the city"s, DocumentStatus::REMOVED, ratings); 
        vector<Document> documents = server.FindTopDocuments("big cat in the city"s, DocumentStatus::BANNED);
        ASSERT_EQUAL(documents[0].id, 2);
        ASSERT_EQUAL(documents.size(), 1u);  
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(0, "big cat in the village"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(1, "big cat in the city"s, DocumentStatus::IRRELEVANT, ratings);
        server.AddDocument(2, "big cat in the city"s, DocumentStatus::BANNED, ratings);
        server.AddDocument(3, "big cat in the city"s, DocumentStatus::REMOVED, ratings);       
        vector<Document> documents = server.FindTopDocuments("big cat in the city"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(documents[0].id, 1);
        ASSERT_EQUAL(documents.size(), 1u);   
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(0, "big cat in the village"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(1, "big cat in the city"s, DocumentStatus::IRRELEVANT, ratings);
        server.AddDocument(2, "big cat in the city"s, DocumentStatus::BANNED, ratings);
        server.AddDocument(3, "big cat in the city"s, DocumentStatus::REMOVED, ratings);       
        vector<Document> documents = server.FindTopDocuments("big cat in the city"s, DocumentStatus::REMOVED);
        ASSERT_EQUAL(documents[0].id, 3);
        ASSERT_EQUAL(documents.size(), 1u);   
    }   
}

void TestRelevanceCalculation() {
    const vector<int> ratings = { 1, 2, 3 };
    SearchServer server;
    server.SetStopWords("in"s);
    server.AddDocument(0, "small cat in village"s, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(1, "big cat in city"s, DocumentStatus::ACTUAL, ratings);
    vector<Document> docs = server.FindTopDocuments("big cat city"s);
    ASSERT(abs(docs[0].relevance - 0.462098) < CORRECTION);
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludeDocsContainsMinusWords);
    RUN_TEST(TestDocsMatching);
    RUN_TEST(TestDocsSortByRelevance);
    RUN_TEST(TestCalculateRating);
    RUN_TEST(TestFindDocsWithStatus);
    RUN_TEST(TestRelevanceCalculation);
}
// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}