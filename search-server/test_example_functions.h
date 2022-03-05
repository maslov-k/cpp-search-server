#pragma once

#include "search_server.h"
#include "document.h"
#include <vector>
#include <string>

void AddDocument(SearchServer& search_server, int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

void FindTopDocuments(const SearchServer& search_server, std::string_view raw_query);

void MatchDocuments(const SearchServer& search_server, std::string_view query);
