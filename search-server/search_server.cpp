#include "search_server.h"
#include "string_processing.h"
#include "document.h"
#include "log_duration.h"
#include <algorithm>
#include <execution>
#include <stdexcept>
#include <cmath>

using namespace std;

int SearchServer::ComputeAverageRating(const vector<int>& ratings)
{
	return reduce(execution::par, ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
}

bool SearchServer::IsValidWord(const string& word)
{
	return none_of(word.begin(), word.end(), [](char c)
		{
			return c >= '\0' && c < ' ';
		});
}

bool SearchServer::IsValidQuery(const string& query)
{
	return (query.find("--"s) == string::npos &&
		query.back() != '-');
}

bool SearchServer::IsStopWord(const string& word) const
{
	return (stop_words_.count(word) > 0);
}

vector<string> SearchServer::SplitIntoWordsNoStop(const string& text) const
{
	vector<string> words;
	for (const string& word : SplitIntoWords(text))
	{
		if (!IsStopWord(word))
		{
			words.push_back(word);
		}
	}
	return words;
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string word) const
{
	if (!IsValidQuery(word))
	{
		throw invalid_argument("invalid query");
	}
	if (!IsValidWord(word))
	{
		throw invalid_argument("invalid word: "s + word);
	}
	bool is_minus = false;
	if (word[0] == '-') {
		is_minus = true;
		word = word.substr(1);
	}
	return { word, is_minus };
}

SearchServer::Query SearchServer::ParseQuery(const string& query) const
{
	Query query_words;
	for (const string& word : SplitIntoWordsNoStop(query))
	{
		const QueryWord query_word = ParseQueryWord(word);
		if (query_word.is_minus)
		{
			query_words.minus_words.insert(query_word.word);
		}
		else
		{
			query_words.plus_words.insert(query_word.word);
		}
	}
	return query_words;
}

double SearchServer::ComputeWordIDF(const string& word) const
{
	return log(static_cast<double>(GetDocumentCount()) / word_to_documents_freqs_.at(word).size());
}

SearchServer::SearchServer() = default;

template <typename StringCollection>
SearchServer::SearchServer(const StringCollection & stop_words)
{
	if (!all_of(stop_words.begin(), stop_words.end(), IsValidWord))
	{
		throw invalid_argument("invalid characters");
	}
	SetStopWords(stop_words);
}
template SearchServer::SearchServer(const vector<string>& stop_words);
template SearchServer::SearchServer(const set<string>& stop_words);


SearchServer::SearchServer(const string & stop_words)
	: SearchServer(SplitIntoWords(stop_words))
{
}

int SearchServer::GetDocumentCount() const
{
	return documents_.size();
}

set<int>::const_iterator SearchServer::begin() const
{
	return docs_ids_.cbegin();
}

set<int>::const_iterator SearchServer::end() const
{
	return docs_ids_.cend();
}

const map<string, double>& SearchServer::GetWordFrequencies(int document_id) const
{
	if (document_to_words_freqs_.find(document_id) == document_to_words_freqs_.end())
	{
		static const map<string, double> dummy;
		return dummy;
	}
	else
	{
		return document_to_words_freqs_.at(document_id);
	}
}

void SearchServer::AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings)
{
	if (document_id < 0 || documents_.count(document_id) > 0 || !IsValidWord(document))
	{
		throw invalid_argument("invalid document");
	}
	const vector<string> document_words = SplitIntoWordsNoStop(document);
	const double document_size = document_words.size();
	for (const string& word : document_words)
	{
		word_to_documents_freqs_[word][document_id] += 1. / document_size;
		document_to_words_freqs_[document_id][word] += 1. / document_size;
	}

	documents_.emplace(document_id, DocumentParams{ ComputeAverageRating(ratings), status });
	docs_ids_.insert(document_id);
}

void SearchServer::RemoveDocument(int document_id)
{
	document_to_words_freqs_.erase(document_id);
	documents_.erase(document_id);
	docs_ids_.erase(document_id);

	vector<string> words_to_remove;
	for (auto& [word, docs_freqs] : word_to_documents_freqs_)
	{
		docs_freqs.erase(document_id);
		if (word.empty())
		{
			words_to_remove.push_back(word);
		}
	}
	for (const string& word : words_to_remove)
	{
		word_to_documents_freqs_.erase(word);
	}
}

vector<Document> SearchServer::FindTopDocuments(const string& query) const
{
	return FindTopDocuments(query, DocumentStatus::ACTUAL);
}

vector<Document> SearchServer::FindTopDocuments(const string& query, DocumentStatus status) const
{
	return FindTopDocuments(query, [status](int document_id, DocumentStatus document_status, int rating) { return document_status == status; });
}

tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(const string& raw_query, int document_id) const
{
	vector<string> matched_words;
	Query query_words = ParseQuery(raw_query);
	for (const string& word : query_words.minus_words)
	{
		if (word_to_documents_freqs_.find(word) == word_to_documents_freqs_.end())
		{
			continue;
		}
		if (word_to_documents_freqs_.at(word).count(document_id))
		{
			return tuple{ vector<string>(), documents_.at(document_id).status };
		}
	}
	for (const string& word : query_words.plus_words)
	{
		if (word_to_documents_freqs_.find(word) == word_to_documents_freqs_.end())
		{
			continue;
		}
		if (word_to_documents_freqs_.at(word).count(document_id) && !count(matched_words.begin(), matched_words.end(), word))
		{
			matched_words.push_back(word);
		}
	}
	sort(matched_words.begin(), matched_words.end());
	return tuple{ matched_words, documents_.at(document_id).status };
}