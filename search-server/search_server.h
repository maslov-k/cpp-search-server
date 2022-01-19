#pragma once
#include "document.h"
#include "log_duration.h"
#include <map>
#include <string>
#include <vector>
#include <set>

using namespace std::string_literals;

const size_t MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer
{
private:
	struct DocumentParams
	{
		int rating;
		DocumentStatus status;
	};

	struct QueryWord
	{
		std::string word;
		bool is_minus;
	};

	struct Query
	{
		std::set<std::string> plus_words;
		std::set<std::string> minus_words;
	};

	std::map<std::string, std::map<int, double>> word_to_documents_freqs_;
	std::map<int, std::map<std::string, double>> document_to_words_freqs_;
	std::set<std::string> stop_words_;

	std::map<int, DocumentParams> documents_;
	std::set<int> docs_ids_;

	static int ComputeAverageRating(const std::vector<int>& ratings);

	static bool IsValidWord(const std::string& word);

	template <typename StringCollection>
	static bool IsValidWordCollection(const StringCollection& words);

	static bool IsValidQuery(const std::string& query);

	bool IsStopWord(const std::string& word) const;

	std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;

	template <typename StringCollection>
	void SetStopWords(const StringCollection& stop_words);

	QueryWord ParseQueryWord(std::string word) const;

	Query ParseQuery(const std::string& query) const;

	static void SortAndResizeTopDocuments(std::vector<Document>& matched_documents, const size_t max_count);

	double ComputeWordIDF(const std::string& word) const;

	template <typename DocumentsFilter>
	std::vector<Document> FindAllDocuments(const std::string& query, DocumentsFilter documents_filter) const;

public:
	SearchServer();

	template <typename StringCollection>
	explicit SearchServer(const StringCollection& stop_words);

	explicit SearchServer(const std::string& stop_words);

	int GetDocumentCount() const;

	std::set<int>::const_iterator begin() const;

	std::set<int>::const_iterator end() const;

	const std::map<std::string, double>& GetWordFrequencies(int document_id) const;

	void AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings);

	void RemoveDocument(int document_id);

	std::vector<Document> FindTopDocuments(const std::string& query) const;

	std::vector<Document> FindTopDocuments(const std::string& query, DocumentStatus status) const;

	template <typename DocumentsFilter>
	std::vector<Document> FindTopDocuments(const std::string& query, DocumentsFilter documents_filter) const;

	std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(const std::string& raw_query, int document_id) const;
};

template <typename DocumentsFilter>
std::vector<Document> SearchServer::FindTopDocuments(const std::string& query, DocumentsFilter documents_filter) const
{
	std::vector<Document> result = FindAllDocuments(query, documents_filter);
	SortAndResizeTopDocuments(result, MAX_RESULT_DOCUMENT_COUNT);
	return result;
}

template <typename DocumentsFilter>
std::vector<Document> SearchServer::FindAllDocuments(const std::string& query, DocumentsFilter documents_filter) const
{
	const Query query_words = ParseQuery(query);

	std::set<int> documents_with_minus_words;

	for (const std::string& minus_word : query_words.minus_words)
	{
		for (const auto& [id, tf] : word_to_documents_freqs_.at(minus_word))
		{
			documents_with_minus_words.insert(id);
		}
	}

	std::map<int, double> document_to_relevance;

	for (const std::string& word : query_words.plus_words)
	{
		if (word_to_documents_freqs_.count(word) == 0)
		{
			continue;
		}
		const double idf = ComputeWordIDF(word);
		for (const auto& [id, tf] : word_to_documents_freqs_.at(word))
		{
			const DocumentParams& document = documents_.at(id);
			if (documents_with_minus_words.count(id) == 0 && documents_filter(id, document.status, document.rating))
			{
				document_to_relevance[id] += tf * idf;
			}
		}
	}

	std::vector<Document> matched_documents;
	for (auto [document_id, relevance] : document_to_relevance)
	{
		matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
	}

	return matched_documents;
}