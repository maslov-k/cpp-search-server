#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <cmath>
#include <execution>
#include <stdexcept>

using namespace std;

const size_t MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine()
{
	string s;
	getline(cin, s);
	return s;
}

int ReadLineWithNumber()
{
	int result;
	cin >> result;
	ReadLine();
	return result;
}

vector<int> ReadRatingsLine()
{
	int n;
	cin >> n;
	vector<int> result(n, 0);
	for (int& rating : result)
	{
		cin >> rating;
	}
	ReadLine();
	return result;
}

struct Document
{
	Document() = default;
	Document(int id0, double relevance0, int rating0)
		: id(id0), relevance(relevance0), rating(rating0)
	{
	}

	int id = 0;
	double relevance = 0;
	int rating = 0;
};

enum class DocumentStatus
{
	ACTUAL,
	IRRELEVANT,
	BANNED,
	REMOVED
};

class SearchServer
{
private:
	struct Document_params
	{
		int rating;
		DocumentStatus status;
	};

	map<string, map<int, double>> word_to_documents_freqs_;
	set<string> stop_words_;

	map<int, Document_params> documents_;
	vector<int> docs_id_order_;

	static int ComputeAverageRating(const vector<int>& ratings)
	{
		return reduce(execution::par, ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
	}

	static bool IsValidWord(const string& word)
	{
		return none_of(word.begin(), word.end(), [](char c)
			{
			return c >= '\0' && c < ' ';
			});
	}

	template <typename StringCollection>
	static bool IsValidWordCollection(const StringCollection& words)
	{
		for (const string& word : words)
		{
			if (!IsValidWord(word))
			{
				return false;
			}
		}
		return true;
	}

	static bool IsValidQuery(const string& query)
	{
		return (IsValidWord(query) &&
			query.find("--"s) == std::string::npos &&
			query.find("- "s) == std::string::npos &&
			query[query.size() - 1] != '-');
	}

	static vector<string> SplitIntoWords(const string& text)
	{
		vector<string> words;
		string word;
		for (const char c : text)
		{
			if (c == ' ' && !word.empty())
			{
				words.push_back(word);
				word.clear();
			}
			else
			{
				if (c != ' ')
				{
					word += c;
				}
			}
		}
		if (!word.empty())
		{
			words.push_back(word);
		}

		return words;
	}

	vector<string> SplitIntoWordsNoStop(const string& text) const
	{
		vector<string> words;
		for (const string& word : SplitIntoWords(text))
		{
			if (stop_words_.count(word) == 0)
			{
				words.push_back(word);
			}
		}
		return words;
	}

	struct Query
	{
		set<string> plus_words;
		set<string> minus_words;
	};

	Query ParseQuery(const string& query) const
	{
		Query query_words;
		for (const string& word : SplitIntoWordsNoStop(query))
		{
			if (word[0] == '-')
			{
				query_words.minus_words.insert(word.substr(1));
			}
			else
			{
				query_words.plus_words.insert(word);
			}
		}

		return query_words;
	}

	static void SortAndResizeTopDocuments(vector<Document>& matched_documents, const size_t max_count)
	{
		sort(
			matched_documents.begin(),
			matched_documents.end(),
			[](const Document& a, const Document& b)
			{
				return ((a.relevance - b.relevance) > 1e-6) || ((abs(a.relevance - b.relevance) < 1e-6) && (a.rating > b.rating));
			}
		);
		if (matched_documents.size() > max_count)
		{
			matched_documents.resize(max_count);
		}
	}

	double ComputeWordIDF(const string& word) const
	{
		return log(static_cast<double>(GetDocumentCount()) / word_to_documents_freqs_.at(word).size());
	}

	template <typename DocumentsFilter>
	vector<Document> FindAllDocuments(const string& query, DocumentsFilter documents_filter) const
	{
		const Query query_words = ParseQuery(query);

		set<int> documents_with_minus_words;

		for (const string& minus_word : query_words.minus_words)
		{
			for (const auto& [id, tf] : word_to_documents_freqs_.at(minus_word))
			{
				documents_with_minus_words.insert(id);
			}
		}

		map<int, double> document_to_relevance;

		for (const string& word : query_words.plus_words)
		{
			if (word_to_documents_freqs_.count(word) == 0)
			{
				continue;
			}
			const double idf = ComputeWordIDF(word);
			for (const auto& [id, tf] : word_to_documents_freqs_.at(word))
			{
				const Document_params document = documents_.at(id);
				if (documents_with_minus_words.count(id) == 0 && documents_filter(id, document.status, document.rating))
				{
					document_to_relevance[id] += tf * idf;
				}
			}
		}

		vector<Document> matched_documents;
		for (auto [document_id, relevance] : document_to_relevance)
		{
			matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
		}

		return matched_documents;
	}

public:
	SearchServer() = default;

	explicit SearchServer(const string& stop_words)
	{
		if (!IsValidWord(stop_words))
		{
			throw invalid_argument("invalid characters");
		}
		for (const string& word : SplitIntoWords(stop_words))
		{
			stop_words_.insert(word);
		}
	}

	template <typename StringCollection>
	explicit SearchServer(const StringCollection& stop_words)
	{
		if (!IsValidWordCollection(stop_words))
		{
			throw invalid_argument("invalid characters");
		}
		for (const string& word : stop_words)
		{
			if (!word.empty())
			{
				stop_words_.insert(word);
			}
		}
	}

	int GetDocumentCount() const
	{
		return documents_.size();
	}

	int GetDocumentId(int index) const
	{
		return docs_id_order_.at(index);
	}

	void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings)
	{
		if (document_id < 0)
		{
			throw invalid_argument("id can't be negative");
		}
		if (documents_.count(document_id) > 0)
		{
			throw invalid_argument("id already exists");
		}
		if (!IsValidWord(document))
		{
			throw invalid_argument("invalid characters");
		}
		const vector<string> document_words = SplitIntoWordsNoStop(document);
		const double document_size = document_words.size();
		for (const string& word : document_words)
		{
			word_to_documents_freqs_[word][document_id] += 1. / document_size;
		}

		documents_.emplace(document_id, Document_params{ ComputeAverageRating(ratings), status });
		docs_id_order_.push_back(document_id);
	}

	vector<Document> FindTopDocuments(const string& query) const
	{
		return FindTopDocuments(query, DocumentStatus::ACTUAL);
	}

	vector<Document> FindTopDocuments(const string& query, DocumentStatus status) const
	{
		return FindTopDocuments(query, [status](int document_id, DocumentStatus status1, int rating) { return status1 == status; });
	}

	template <typename DocumentsFilter>
	vector<Document> FindTopDocuments(const string& query, DocumentsFilter documents_filter) const
	{
		if (!IsValidQuery(query))
		{
			throw invalid_argument("invalid query");
		}
		vector<Document> result = FindAllDocuments(query, documents_filter);
		SortAndResizeTopDocuments(result, MAX_RESULT_DOCUMENT_COUNT);
		return result;
	}

	tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const
	{
		if (!IsValidQuery(raw_query))
		{
			throw invalid_argument("invalid query");
		}
		vector<string> matched_words;
		Query query_words = ParseQuery(raw_query);
		for (const string& word : query_words.minus_words)
		{
			if (word_to_documents_freqs_.at(word).count(document_id))
			{
				return tuple{ vector<string>(), documents_.at(document_id).status };
			}
		}
		for (const string& word : query_words.plus_words)
		{
			if (word_to_documents_freqs_.at(word).count(document_id) && !count(matched_words.begin(), matched_words.end(), word))
			{
				matched_words.push_back(word);
			}
		}
		sort(matched_words.begin(), matched_words.end());
		return tuple{ matched_words, documents_.at(document_id).status };
	}
};
