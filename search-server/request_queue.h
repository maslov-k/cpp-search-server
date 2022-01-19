#pragma once
#include "search_server.h"
#include "document.h"
#include <deque>
#include <vector>
#include <string>

class RequestQueue
{
private:
	struct QueryResult
	{
		bool is_empty;
		int request_time;
	};

	std::deque<QueryResult> requests_;
	const static int min_in_day_ = 1440;
	const SearchServer& search_server_;
	int current_time_ = 0;
	int no_result_responses_ = 0;

	void AddResponseToDeque(const std::vector<Document>& response);
public:
	explicit RequestQueue(const SearchServer& search_server);

	std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

	std::vector<Document> AddFindRequest(const std::string& raw_query);

	template <typename DocumentsFilter>
	std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentsFilter document_filter);

	int GetNoResultRequests() const;
};

template <typename DocumentsFilter>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentsFilter document_filter)
{
	const std::vector<Document> response = search_server_.FindTopDocuments(raw_query, document_filter);
	AddResponseToDeque(response);
	return response;
}