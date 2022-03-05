#pragma once

#include "document.h"
#include "log_duration.h"
#include "concurrent_map.h"

#include <map>
#include <string>
#include <string_view>
#include <vector>
#include <set>
#include <tuple>
#include <execution>
#include <stdexcept>
#include <algorithm>
#include <type_traits>

const size_t MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer
{
private:
	struct DocumentParams
	{
		int rating;
		DocumentStatus status;
		std::string text;
	};

	struct QueryWord
	{
		std::string_view word;
		bool is_minus;
	};

	struct Query
	{
		std::vector<std::string_view> plus_words;
		std::vector<std::string_view> minus_words;
	};

	std::map<std::string_view, std::map<int, double>> word_to_documents_freqs_;
	std::map<int, std::map<std::string_view, double>> document_to_words_freqs_;
	std::set<std::string, std::less<>> stop_words_;

	std::map<int, DocumentParams> documents_;
	std::set<int> docs_ids_;

	static int ComputeAverageRating(const std::vector<int>& ratings);

	static bool IsValidWord(std::string_view word);

	static bool IsValidQuery(std::string_view query);

	bool IsStopWord(std::string_view word) const;

	std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;

	template <typename StringCollection>
	void SetStopWords(const StringCollection& stop_words);

	QueryWord ParseQueryWord(std::string_view word) const;

	Query ParseQuery(std::string_view query, bool do_unique = true) const;

	double ComputeWordIDF(std::string_view word) const;

	template <typename DocumentsFilter, typename ExecutionPolicy>
	std::vector<Document> FindAllDocuments(ExecutionPolicy&& policy, std::string_view query, DocumentsFilter documents_filter) const;

public:
	SearchServer();

	template <typename StringCollection>
	explicit SearchServer(const StringCollection& stop_words);

	explicit SearchServer(const std::string& stop_words);

	explicit SearchServer(std::string_view stop_words);

	int GetDocumentCount() const;

	std::set<int>::const_iterator begin() const;

	std::set<int>::const_iterator end() const;

	const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

	void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

	void RemoveDocument(int document_id);

	void RemoveDocument(const std::execution::sequenced_policy& seq, int document_id);

	void RemoveDocument(const std::execution::parallel_policy& par, int document_id);

	std::vector<Document> FindTopDocuments(std::string_view query) const;

	std::vector<Document> FindTopDocuments(std::string_view query, DocumentStatus status) const;

	template <typename DocumentsFilter>
	std::vector<Document> FindTopDocuments(std::string_view query, DocumentsFilter documents_filter) const;

	template <typename ExecutionPolicy>
	std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, std::string_view query) const;

	template <typename ExecutionPolicy>
	std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, std::string_view query, DocumentStatus status) const;

	template <typename DocumentsFilter, typename ExecutionPolicy>
	std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, std::string_view query, DocumentsFilter documents_filter) const;

	std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::string_view raw_query, int document_id) const;

	template <typename ExecutionPolicy>
	std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(ExecutionPolicy&& policy, std::string_view raw_query, int document_id) const;
};

template <typename StringCollection>
void SearchServer::SetStopWords(const StringCollection& stop_words)
{
	for (std::string_view word : stop_words)
	{
		if (!word.empty())
		{
			stop_words_.insert(std::string{ word });
		}
	}
}

template<typename DocumentsFilter>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view query, DocumentsFilter documents_filter) const
{
	return FindTopDocuments(std::execution::seq, query, documents_filter);
}

template<typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view query) const
{
	return FindTopDocuments(policy, query, DocumentStatus::ACTUAL);
}

template<typename ExecutionPolicy>
inline std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view query, DocumentStatus status) const
{
	return FindTopDocuments(policy, query, [status](int document_id, DocumentStatus document_status, int rating) { return document_status == status; });
}

template <typename DocumentsFilter, typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view query, DocumentsFilter documents_filter) const
{
	std::vector<Document> result = FindAllDocuments(policy, query, documents_filter);
	
	sort(policy,
		result.begin(),
		result.end(),
		[](const Document& a, const Document& b)
		{
			if (std::abs(a.relevance - b.relevance) < 1e-6)
			{
				return a.rating > b.rating;
			}
			else
			{
				return a.relevance > b.relevance;
			}
		});
	if (result.size() > MAX_RESULT_DOCUMENT_COUNT)
	{
		result.resize(MAX_RESULT_DOCUMENT_COUNT);
	}

	return result;
}

template <typename DocumentsFilter, typename ExecutionPolicy>
std::vector<Document> SearchServer::FindAllDocuments(ExecutionPolicy&& policy, std::string_view query, DocumentsFilter documents_filter) const
{
	const Query query_words = ParseQuery(query);

	std::set<int> documents_with_minus_words;

	for (std::string_view minus_word : query_words.minus_words)
	{
		for (const auto& [id, tf] : word_to_documents_freqs_.at(minus_word))
		{
			documents_with_minus_words.insert(id);
		}
	}

	std::map<int, double> document_to_relevance;
	
	bool constexpr is_parallel = std::is_same_v<ExecutionPolicy, const std::execution::parallel_policy&>;
	ConcurrentMap<int, double> cm_document_to_relevance(50);

	std::for_each(
		policy, query_words.plus_words.begin(), query_words.plus_words.end(),
		[&](std::string_view word)
		{
			if (word_to_documents_freqs_.count(word) != 0)
			{
				const double idf = ComputeWordIDF(word);
				for (const auto& [id, tf] : word_to_documents_freqs_.at(word))
				{
					const DocumentParams& document = documents_.at(id);
					if (documents_with_minus_words.count(id) == 0 && documents_filter(id, document.status, document.rating))
					{
						if (is_parallel)
						{
							cm_document_to_relevance[id].ref_to_value += tf * idf;
						}
						else
						{
							document_to_relevance[id] += tf * idf;
						}
						
					}
				}
			}
		});
	
	if (is_parallel)
	{
		document_to_relevance = cm_document_to_relevance.BuildOrdinaryMap();
	}
	std::vector<Document> matched_documents;

	for (auto [document_id, relevance] : document_to_relevance)
	{
		matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
	}

	return matched_documents;
}

template <typename ExecutionPolicy>
std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(ExecutionPolicy&& policy, std::string_view raw_query, int document_id) const
{
	using namespace std::string_view_literals;

	Query query_words = ParseQuery(raw_query, false);
	std::vector<std::string_view> matched_words(query_words.plus_words.size());

	auto word_checker = [this, document_id](std::string_view word)
	{
		return (word_to_documents_freqs_.find(word) != word_to_documents_freqs_.end() &&
			word_to_documents_freqs_.at(word).count(document_id));
	};

	if (std::any_of(policy, query_words.minus_words.begin(), query_words.minus_words.end(), word_checker))
	{
		return std::tuple{ std::vector<std::string_view>(), documents_.at(document_id).status };
	}

	auto words_end = std::copy_if(policy, query_words.plus_words.begin(), query_words.plus_words.end(), matched_words.begin(), word_checker);

	matched_words.erase(remove(matched_words.begin(), matched_words.end(), ""sv), matched_words.end());

	std::sort(matched_words.begin(), words_end);
	words_end = unique(matched_words.begin(), words_end);
	matched_words.erase(words_end, matched_words.end());
	return std::tuple{ matched_words, documents_.at(document_id).status };
}

template <typename StringCollection>
SearchServer::SearchServer(const StringCollection& stop_words)
{
	if (!all_of(stop_words.begin(), stop_words.end(), IsValidWord))
	{
		throw std::invalid_argument("invalid characters");
	}
	SetStopWords(stop_words);
}
