#include "remove_duplicates.h"
#include "search_server.h"

using namespace std;

void RemoveDuplicates(SearchServer& search_server)
{
	set<vector<string_view>> documents;
	set<int> ids_to_remove;
	for (const int document_id : search_server)
	{
		vector<string_view> words;
		for (const auto& [word, _freq] : search_server.GetWordFrequencies(document_id))
		{
			words.push_back(word);
		}
		if (documents.find(words) != documents.end())
		{
			ids_to_remove.insert(document_id);
		}
		else
		{
			documents.insert(words);
		}
	}

	for (int id : ids_to_remove)
	{
		search_server.RemoveDocument(id);
		cout << "Found duplicate document id " << id << "\n";
	}
}