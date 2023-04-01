#include "search_server.h"

using namespace std;

SearchServer::SearchServer(const string& stop_words_text): SearchServer(SplitIntoWords(stop_words_text)) 
{
}

void SearchServer::AddDocument(int document_id, const string& document, DocumentStatus status,
                    const vector<int>& ratings) {
    if(document_id < 0 || documents_.count(document_id)){
        throw invalid_argument("invalid id");
    }
    const vector<string> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for (const string& word : words) {
        if(!IsValidWord(word)){
            throw invalid_argument("document contains unavailable characters");
        }
    }
    for (const string& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        id_to_string_freq_[document_id][word] += inv_word_count;
    }
    std::set<std::string> w(words.begin(), words.end());
    id_words[document_id] = w;
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    ids_.insert(document_id);
}

vector<Document> SearchServer::FindTopDocuments(const string& raw_query, DocumentStatus status) const {
    return FindTopDocuments(
        raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        });
}

vector<Document> SearchServer::FindTopDocuments(const string& raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(const string& raw_query, int document_id) const {
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
    return tuple(matched_words, documents_.at(document_id).status);
}

bool SearchServer::IsValidWord(const string& word)  {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {return c >= '\0' && c < ' ';});
}

bool SearchServer::IsStopWord(const string& word) const {
    return stop_words_.count(word) > 0;
}

void SearchServer::IsValidQueryWord(const string& word) const {
    if (!IsValidWord(word) || word.empty() || word[0] == '-'){
            throw invalid_argument("query contains anvailable characters");
        }
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
    int rating_sum = 0;
    for (const int rating : ratings) {
        rating_sum += rating;
    }
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string text) const {
    bool is_minus = false;
    // Word shouldn't be empty
    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
    }
    return { text, is_minus, IsStopWord(text) };
}

SearchServer::Query SearchServer::ParseQuery(const string& text) const {
    Query query;
    for (const string& word : SplitIntoWords(text)) {
        const QueryWord query_word = ParseQueryWord(word);
        IsValidQueryWord(query_word.data);
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

double SearchServer::ComputeWordInverseDocumentFreq(const string& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

set<int>::const_iterator SearchServer::begin() const {
    return ids_.begin();
}

set<int>::const_iterator SearchServer::end() const {
    return ids_.end();
}

const map<string, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static const map<string, double> res;
    if(id_to_string_freq_.count(document_id)) {
        return id_to_string_freq_.at(document_id);
    }
    return res;
}

void SearchServer::RemoveDocument(int document_id) {
    ids_.erase(document_id);

    documents_.erase(document_id);

    id_to_string_freq_.erase(document_id);

    for(auto [word, container]: word_to_document_freqs_) {
        if(container.count(document_id)) {
            container.erase(document_id);
            break;
        }
    }

}

set<string> SearchServer::GetWordsById(int doc_id) const {
    set<string> res;
    if(id_words.count(doc_id)) {
        res = id_words.at(doc_id);
    }
    return res;
}