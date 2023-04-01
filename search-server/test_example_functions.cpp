#include "test_example_functions.h"

using namespace std;

void MatchDocuments(SearchServer& search_server, string& text) {
    LOG_DURATION("Matching documents on request:");
    search_server.MatchDocument(text, 0);
}

void FindTopDocuments(SearchServer& search_server, string& text) {
    LOG_DURATION("Findings top documents on request:");
    search_server.FindTopDocuments(text);
}

void AddDocument(SearchServer& search_server, string text, vector<int> ratings, int id, DocumentStatus status) {
    LOG_DURATION("Addings documents to server:");
    search_server.AddDocument(id, text, status, ratings);
}