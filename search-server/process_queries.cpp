#include "process_queries.h"

#include <algorithm>
#include <execution>

using namespace std;

vector<vector<Document>> ProcessQueries(const SearchServer& search_server, const vector<string>& queries)
{
	vector<vector<Document>> result(queries.size());
	transform(execution::par, queries.begin(), queries.end(), result.begin(),
		[&search_server](const string& query) { return move(search_server.FindTopDocuments(query)); });
	return result;
}

std::list<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string>& queries)
{
	list<Document> result = transform_reduce(
		execution::par,
		queries.begin(), queries.end(),
		list<Document>(),
		[](list<Document> lhs, list<Document> rhs)
		{
			lhs.splice(lhs.end(), move(rhs));
			return lhs;
		},
		[&search_server](const string& query)
		{
			vector<Document> v_response = search_server.FindTopDocuments(query);
			return list<Document>(make_move_iterator(v_response.begin()), make_move_iterator(v_response.end()));
		});
	return result;
}
