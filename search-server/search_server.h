#pragma once

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string_view>
#include <string>
#include <utility>
#include <vector>
#include <execution>

#include "document.h"
#include "string_processing.h"
#include "read_input_functions.h"
#include "concurrent_map.h"

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double CORRECTION = 1e-6;

class SearchServer {
public:
    SearchServer() = default;

    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);

    explicit SearchServer(const std::string& stop_words_text);

    explicit SearchServer(const std::string_view stop_words_text);
    
    void AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query) const;

    template <typename Execution, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(Execution policy, std::string_view raw_query, DocumentPredicate document_predicate) const;

    template <typename Execution>
    std::vector<Document> FindTopDocuments(Execution policy, std::string_view raw_query, DocumentStatus status) const;

    template <typename Execution>
    std::vector<Document> FindTopDocuments(Execution policy, std::string_view raw_query) const;

    int GetDocumentCount() const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view raw_query, int document_id) const;
    
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::sequenced_policy policy, 
                                                                       const std::string_view raw_query, int document_id) const;
    
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::parallel_policy policy, 
                                                                       const std::string_view raw_query, int document_id) const;

    std::set<int>::const_iterator begin() const;

    std::set<int>::const_iterator end() const;

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    std::set<std::string, std::less<>> GetWordsById(int doc_id) const;

    void RemoveDocument(int document_id);

    void RemoveDocument(std::execution::parallel_policy policy, int document_id);

    void RemoveDocument(std::execution::sequenced_policy policy, int document_id);
private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    const std::set<std::string, std::less<>> stop_words_;
    std::map<std::string, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> ids_;
    std::map<int, std::map<std::string, double>> id_to_string_freq_;
    std::map<int, std::set<std::string, std::less<>>> id_words;

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    static bool IsValidWord(std::string_view word);

    bool IsStopWord(std::string_view word) const;

    void IsValidQueryWord(std::string_view word) const;

    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    QueryWord ParseQueryWord(std::string_view& text) const;

    Query ParseQuery(std::string_view& text, bool need_unique) const;
    // Existence required
    double ComputeWordInverseDocumentFreq(const std::string& word) const;

    template <typename Predictor>
    std::vector<Document> FindAllDocuments(const Query& query, Predictor filter) const;

    template <typename Predictor>
    std::vector<Document> FindAllDocuments(std::execution::sequenced_policy policy, const Query& query, Predictor filter) const; 

    template <typename Predictor>
    std::vector<Document> FindAllDocuments(std::execution::parallel_policy policy, const Query& query, Predictor filter) const;  
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {

        if(!(all_of(stop_words.begin(), stop_words.end(), IsValidWord))){
            throw std::invalid_argument("stop words contain unavailable characters");
        }
    }

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query,
                                      DocumentPredicate document_predicate) const {
    const Query query = ParseQuery(raw_query, true);
    auto matched_documents = FindAllDocuments(query, document_predicate);
    sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (std::abs(lhs.relevance - rhs.relevance) < CORRECTION) {
                    return lhs.rating > rhs.rating;
                } else {
                    return lhs.relevance > rhs.relevance;
                }
            });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <typename Predictor>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, Predictor filter) const {
    std::map<int, double> document_to_relevance;

    for (auto word : query.plus_words) {
        if (word_to_document_freqs_.count(static_cast<std::string>(word)) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(static_cast<std::string>(word));
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(static_cast<std::string>(word))) {
            if (filter(document_id, documents_.at(document_id).status, documents_.at(document_id).rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (auto word : query.minus_words) {
        if (word_to_document_freqs_.count(static_cast<std::string>(word)) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(static_cast<std::string>(word))) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back(
            { document_id, relevance, documents_.at(document_id).rating });
    }
        return matched_documents;
}

template <typename Predictor>
std::vector<Document> SearchServer::FindAllDocuments(std::execution::sequenced_policy policy, const Query& query, Predictor filter) const {
    return FindAllDocuments(query, filter);
}

template <typename Predictor>
std::vector<Document> SearchServer::FindAllDocuments(std::execution::parallel_policy policy, const Query& query, Predictor filter) const {
    ConcurrentMap<int, double> document_to_relevance(7);
    
    std::for_each(std::execution::par, query.plus_words.begin(), query.plus_words.end(), [&](const auto& word){
        if (word_to_document_freqs_.count(static_cast<std::string>(word)) != 0) {
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(static_cast<std::string>(word));
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(static_cast<std::string>(word))) {
                if (filter(document_id, documents_.at(document_id).status, documents_.at(document_id).rating)) {
                    document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                }
            }            
        }    
    });

    std::for_each(std::execution::par, query.minus_words.begin(), query.minus_words.end(), [&](const auto& word){
        if (word_to_document_freqs_.count(static_cast<std::string>(word)) != 0) {
            for (const auto [document_id, _] : word_to_document_freqs_.at(static_cast<std::string>(word))) {
                document_to_relevance.Erase(document_id);
            }
        }        
    });

    std::vector<Document> matched_documents;

    for (const auto [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
        matched_documents.push_back(
            { document_id, relevance, documents_.at(document_id).rating });
    }
        return matched_documents;
}

template <typename Execution, typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(Execution policy, std::string_view raw_query,
                                      DocumentPredicate document_predicate) const {
    const Query query = ParseQuery(raw_query, true);
    auto matched_documents = FindAllDocuments(policy, query, document_predicate);
    sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (std::abs(lhs.relevance - rhs.relevance) < CORRECTION) {
                    return lhs.rating > rhs.rating;
                } else {
                    return lhs.relevance > rhs.relevance;
                }
            });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <typename Execution>
std::vector<Document> SearchServer::FindTopDocuments(Execution policy, std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(
        policy, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        });
}

template <typename Execution>
std::vector<Document> SearchServer::FindTopDocuments(Execution policy, std::string_view raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}