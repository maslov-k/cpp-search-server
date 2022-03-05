#include "search_server.h"
#include "string_processing.h"
#include "document.h"
#include "log_duration.h"
#include <algorithm>
#include <execution>
#include <stdexcept>
#include <cmath>
#include <string_view>

using namespace std;

int SearchServer::ComputeAverageRating(const vector<int>& ratings)
{
	return reduce(execution::par, ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
}

bool SearchServer::IsValidWord(string_view word)
{
	return none_of(word.begin(), word.end(), [](char c)
		{
			return c >= '\0' && c < ' ';
		});
}

bool SearchServer::IsValidQuery(string_view query)
{
	return (query.find("--"s) == string::npos &&
		query.back() != '-');
}

bool SearchServer::IsStopWord(string_view word) const
{
	return stop_words_.count(word);
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(string_view text) const
{
	vector<string_view> words;
	for (string_view word : SplitIntoWordsView(text))
	{
		if (!IsStopWord(word))
		{
			words.push_back(word);
		}
	}
	return words;
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view word) const
{
	if (!IsValidQuery(word))
	{
		throw invalid_argument("invalid query");
	}
	if (!IsValidWord(word))
	{
		throw invalid_argument("invalid word: "s + string{ word });
	}
	bool is_minus = false;
	if (word[0] == '-') {
		is_minus = true;
		word = word.substr(1);
	}
	return { word, is_minus };
}

SearchServer::Query SearchServer::ParseQuery(string_view query, bool do_unique) const
{
	Query query_words;
	for (string_view word : SplitIntoWordsNoStop(query))
	{
		const QueryWord query_word = ParseQueryWord(word);
		if (query_word.is_minus)
		{
			query_words.minus_words.push_back(query_word.word);
		}
		else
		{
			query_words.plus_words.push_back(query_word.word);
		}
	}
	if (do_unique)
	{
		std::sort(query_words.minus_words.begin(), query_words.minus_words.end());
		query_words.minus_words.erase(unique(query_words.minus_words.begin(), query_words.minus_words.end()), query_words.minus_words.end());
		std::sort(query_words.plus_words.begin(), query_words.plus_words.end());
		query_words.plus_words.erase(unique(query_words.plus_words.begin(), query_words.plus_words.end()), query_words.plus_words.end());
	}
	return query_words;
}

double SearchServer::ComputeWordIDF(string_view word) const
{
	return log(static_cast<double>(GetDocumentCount()) / word_to_documents_freqs_.at(word).size());
}

SearchServer::SearchServer() = default;

SearchServer::SearchServer(const string& stop_words)
	: SearchServer(string_view{stop_words})
{
}

SearchServer::SearchServer(std::string_view stop_words)
	: SearchServer(SplitIntoWordsView(stop_words))
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

const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const
{
	if (document_to_words_freqs_.find(document_id) == document_to_words_freqs_.end())
	{
		static const map<string_view, double> dummy;
		return dummy;
	}
	else
	{
		return document_to_words_freqs_.at(document_id);
	}
}

void SearchServer::AddDocument(int document_id, string_view document, DocumentStatus status, const vector<int>& ratings)
{

	if (document_id < 0 || documents_.count(document_id) > 0 || !IsValidWord(document))
	{
		throw invalid_argument("invalid document");
	}

	const auto [it, _is_inserted] = documents_.emplace(document_id, DocumentParams{ ComputeAverageRating(ratings), status , string{document} });
	docs_ids_.insert(document_id);
	const vector<string_view> document_words = SplitIntoWordsNoStop(it->second.text);
	const double document_size = document_words.size();
	for (const string_view word : document_words)
	{
		word_to_documents_freqs_[word][document_id] += 1. / document_size;
		document_to_words_freqs_[document_id][word] += 1. / document_size;
	}
}

void SearchServer::RemoveDocument(int document_id)
{
	RemoveDocument(execution::seq, document_id);
}

void SearchServer::RemoveDocument(const execution::sequenced_policy&, int document_id)
{
	if (!docs_ids_.erase(document_id))
	{
		return;
	}
	documents_.erase(document_id);

	for (auto& [word, freq] : document_to_words_freqs_[document_id])
	{
		word_to_documents_freqs_[word].erase(document_id);
	}

	document_to_words_freqs_.erase(document_id);
}

void SearchServer::RemoveDocument(const execution::parallel_policy&, int document_id)
{
	if (!docs_ids_.erase(document_id))
	{
		return;
	}
	documents_.erase(document_id);

	const map<string_view, double>& words_freqs = document_to_words_freqs_[document_id];
	vector<string_view> words_to_remove(words_freqs.size());

	transform(words_freqs.begin(), words_freqs.end(), words_to_remove.begin(),
		[](const auto& word_and_freq) { return word_and_freq.first; });

	for_each(execution::par, words_to_remove.begin(), words_to_remove.end(),
		[this, document_id](string_view word)
		{
			word_to_documents_freqs_[word].erase(document_id);
		});

	document_to_words_freqs_.erase(document_id);
}

vector<Document> SearchServer::FindTopDocuments(string_view query) const
{
	return FindTopDocuments(execution::seq, query, DocumentStatus::ACTUAL);
}

vector<Document> SearchServer::FindTopDocuments(string_view query, DocumentStatus status) const
{
	return FindTopDocuments(execution::seq, query, [status](int document_id, DocumentStatus document_status, int rating) { return document_status == status; });
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(string_view raw_query, int document_id) const
{
	return MatchDocument(execution::seq, raw_query, document_id);
}
