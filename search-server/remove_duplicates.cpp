#include "remove_duplicates.h"
#include "search_server.h"

using namespace std;

void RemoveDuplicates(SearchServer& search_server)
{
	map<set<string>, vector<int>> words_to_documents;
	set<int> ids_to_remove;
	for (const int document_id : search_server)
	{
		set<string> words;
		for (const auto [word, freq] : search_server.GetWordFrequencies(document_id))
		{
			words.insert(word);
		}
		words_to_documents[words].push_back(document_id);
	}

	for (const auto [words, ids] : words_to_documents)
	{
		for (auto it = ids.begin() + 1; it != ids.end(); ++it)
		{
			ids_to_remove.insert(*it);
		}
	}

	for (int id : ids_to_remove)
	{
		search_server.RemoveDocument(id);
		cout << "Found duplicate document id " << id << "\n";
	}
}