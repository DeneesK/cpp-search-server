#include "search_server.h"

using namespace std;

SearchServer::SearchServer(const string& stop_words_text): SearchServer(SplitIntoWords(stop_words_text)) 
{
}

SearchServer::SearchServer(const string_view stop_words_text): SearchServer(SplitIntoWords(static_cast<string>(stop_words_text)))
{
}

void SearchServer::AddDocument(int document_id, const string_view document, DocumentStatus status, const vector<int>& ratings) {
    if(document_id < 0 || documents_.count(document_id)){
        throw invalid_argument("invalid id");
    }
    const auto words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for (auto word : words) {
        if(!IsValidWord(word)){
            throw invalid_argument("document contains unavailable characters");
        }
    }
    for (const auto &word : words) {
        word_to_document_freqs_[static_cast<string>(word)][document_id] += inv_word_count;
        id_to_string_freq_[document_id][static_cast<string>(word)] += inv_word_count;
    }
    set<string_view> w(words.begin(), words.end());
    id_words[document_id] = w;
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    ids_.insert(document_id);
}

vector<Document> SearchServer::FindTopDocuments(const string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(
        raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        });
}

vector<Document> SearchServer::FindTopDocuments(const string_view raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

words_docstatus SearchServer::MatchDocument(string_view raw_query, int document_id) const {
    if (!documents_.count(document_id)) {
        throw out_of_range("");
    }
    const Query query = ParseQuery(raw_query, true);

    auto& words = id_words.at(document_id);

    for (string_view word : query.minus_words) {
        if(words.count(word)) {
            return {{}, documents_.at(document_id).status};
        }
    }

    vector<string_view> matched_words;
    matched_words.reserve(query.plus_words.size());
    
    for (string_view word: query.plus_words) {
        if(words.count(word)) {
            matched_words.push_back(word);
        }
    }


    return tuple(matched_words, documents_.at(document_id).status);
}

words_docstatus SearchServer::MatchDocument(execution::sequenced_policy policy, 
                                                                  string_view raw_query, int document_id) const {
    return MatchDocument(raw_query, document_id);
}

words_docstatus SearchServer::MatchDocument(execution::parallel_policy policy, 
                                                                  string_view raw_query, int document_id) const {
    if (!documents_.count(document_id)) {
        throw out_of_range("");
    }
    Query query = ParseQuery(raw_query, false);
    auto& words = id_words.at(document_id);

    for (string_view word: query.minus_words) {
        if(words.count(word)) {
            return {{}, documents_.at(document_id).status};
        }
    }

    sort(query.plus_words.begin(), query.plus_words.end());
    auto plus_words_end = unique(query.plus_words.begin(), query.plus_words.end());


    vector<string_view> matched_words;
    matched_words.reserve(distance(query.plus_words.begin(), plus_words_end));

    // copy_if(policy, query.plus_words.begin(), plus_words_end, back_inserter(matched_words),
    //         [&](const string& word){return words.count(word);});

    mutex m;

    for_each(policy, query.plus_words.begin(), plus_words_end, [&](const auto& word)
                    {if(words.count(word)) {
                        lock_guard guard(m);
                        matched_words.push_back(word);
                        };
                    }
            );

    return tuple(matched_words, documents_.at(document_id).status);
}

bool SearchServer::IsStopWord(string_view word) const
{
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(string_view word)
{
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c)
                   { return c >= '\0' && c < ' '; });
}

void SearchServer::IsValidQueryWord(string_view word) const {
    if (!IsValidWord(word) || word.empty() || word[0] == '-'){
            throw invalid_argument("query contains unavailable characters");
        }
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(string_view text) const {
    vector<string_view> words;
    for (const string_view& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Word "s + string(word) + " is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(move(word));
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

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view& text) const {
    bool is_minus = false;
    // Word shouldn't be empty
    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
    }
    return { text, is_minus, IsStopWord(text) };
}

SearchServer::Query SearchServer::ParseQuery(string_view text, bool unique_) const {
    Query query;
    auto words = SplitIntoWords(text);
    query.plus_words.reserve(words.size());
    query.minus_words.reserve(words.size());
     for (string_view word : words) {  
        const QueryWord query_word = ParseQueryWord(word);
        IsValidQueryWord(query_word.data);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.push_back(move(query_word.data));
            }
            else {
                query.plus_words.push_back(move(query_word.data));
            }
        }
    }
    if (unique_) {
        sort(query.minus_words.begin(), query.minus_words.end());
        auto minus_words_end = unique(query.minus_words.begin(), query.minus_words.end());
        query.minus_words.erase(minus_words_end, query.minus_words.end());

        sort(query.plus_words.begin(), query.plus_words.end());
        auto plus_words_end = unique(query.plus_words.begin(), query.plus_words.end());
        query.plus_words.erase(plus_words_end, query.plus_words.end());
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

const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    if(id_to_string_freq_.count(document_id)) {
        return id_to_string_freq_.at(document_id);
    }
    return {};
}

void SearchServer::RemoveDocument(execution::sequenced_policy policy, int document_id) {
    RemoveDocument(document_id);
}

void SearchServer::RemoveDocument(execution::parallel_policy policy, int document_id) {
    if (!documents_.count(document_id)){
        return;
        }

	auto& items = id_to_string_freq_.at(document_id);

	vector<string> words(items.size());
    
	transform(policy, items.begin(), items.end(), words.begin(), [](auto& p) { return p.first; });

	for_each(policy, words.begin(), words.end(),
		[&](auto word) {
			word_to_document_freqs_.at(word).erase(document_id);
		}
	);

    id_to_string_freq_.erase(document_id);
    ids_.erase(document_id);
    documents_.erase(document_id);
    id_words.erase(document_id);
}

void SearchServer::RemoveDocument(int document_id) {
    if (!documents_.count(document_id)){
        return;
        }

    vector<string_view> words;

    auto words_for_erase = id_to_string_freq_.at(document_id); 

    for (auto [word, freq]: words_for_erase) {
        words.push_back(word);
    }

    for (auto word : words) {
        word_to_document_freqs_.at(string(word)).erase(document_id);
    }

    id_to_string_freq_.erase(document_id);
    ids_.erase(document_id);
    documents_.erase(document_id);
    id_words.erase(document_id);
}

set<string_view> SearchServer::GetWordsById(int doc_id) const {
    if(id_words.count(doc_id)) {
        return id_words.at(doc_id);
    }
    return {};
}