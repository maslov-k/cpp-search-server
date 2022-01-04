#include "request_queue.h"

using namespace std;

void RequestQueue::AddResponseToDeque(const vector<Document>& response)
{
	++current_time_;
	QueryResult query_result;
	query_result.is_empty = response.empty();
	query_result.request_time = current_time_;
	requests_.push_back(query_result);
	if (query_result.is_empty)
	{
		++no_result_responses_;
	}
	while (!requests_.empty() && requests_.front().request_time <= (current_time_ - min_in_day_))
	{
		if (requests_.front().is_empty)
		{
			--no_result_responses_;
		}
		requests_.pop_front();
	}
}

RequestQueue::RequestQueue(const SearchServer& search_server)
	: search_server_(search_server)
{
}

vector<Document> RequestQueue::AddFindRequest(const string& raw_query, DocumentStatus status)
{
	const vector<Document> response = search_server_.FindTopDocuments(raw_query, status);
	AddResponseToDeque(response);
	return response;
}

vector<Document> RequestQueue::AddFindRequest(const string& raw_query)
{
	const vector<Document> response = search_server_.FindTopDocuments(raw_query);
	AddResponseToDeque(response);
	return response;
}

int RequestQueue::GetNoResultRequests() const
{
	return no_result_responses_;
}