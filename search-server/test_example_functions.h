#pragma once

#include "log_duration.h"
#include "search_server.h"

void MatchDocuments(SearchServer& search_server, std::string& text);

void FindTopDocuments(SearchServer& search_server, std::string& text);

void AddDocument(SearchServer& search_server, int id, std::string& text, DocumentStatus status, std::vector<int>& ratings);
