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

vector<string> SplitIntoWords(const string& text)
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
	struct DocumentParams
	{
		int rating;
		DocumentStatus status;
	};

	map<string, map<int, double>> word_to_documents_freqs_;
	set<string> stop_words_;

	map<int, DocumentParams> documents_;
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
		return all_of(words.begin(), words.end(), IsValidWord);
	}

	static bool IsValidQuery(const string& query)
	{
		return (query.find("--"s) == std::string::npos &&
			query.back() != '-');
	}

	bool IsStopWord(const string& word) const
	{
		return (stop_words_.count(word) > 0);
	}

	vector<string> SplitIntoWordsNoStop(const string& text) const
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

	template <typename StringCollection>
	void SetStopWords(const StringCollection& stop_words)
	{
		for (const string& word : stop_words)
		{
			if (!word.empty())
			{
				stop_words_.insert(word);
			}
		}
	}

	struct QueryWord {
		string word;
		bool is_minus;
	};

	struct Query
	{
		set<string> plus_words;
		set<string> minus_words;
	};

	QueryWord ParseQueryWord(string word) const
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

	Query ParseQuery(const string& query) const
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
				const DocumentParams& document = documents_.at(id);
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

	template <typename StringCollection>
	explicit SearchServer(const StringCollection& stop_words)
	{
		if (!IsValidWordCollection(stop_words))
		{
			throw invalid_argument("invalid characters");
		}
		SetStopWords(stop_words);
	}

	explicit SearchServer(const string& stop_words)
		: SearchServer(SplitIntoWords(stop_words))
	{
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
		if (document_id < 0 || documents_.count(document_id) > 0 || !IsValidWord(document))
		{
			throw invalid_argument("invalid document");
		}
		const vector<string> document_words = SplitIntoWordsNoStop(document);
		const double document_size = document_words.size();
		for (const string& word : document_words)
		{
			word_to_documents_freqs_[word][document_id] += 1. / document_size;
		}

		documents_.emplace(document_id, DocumentParams{ ComputeAverageRating(ratings), status });
		docs_id_order_.push_back(document_id);
	}

	vector<Document> FindTopDocuments(const string& query) const
	{
		return FindTopDocuments(query, DocumentStatus::ACTUAL);
	}

	vector<Document> FindTopDocuments(const string& query, DocumentStatus status) const
	{
		return FindTopDocuments(query, [status](int document_id, DocumentStatus document_status, int rating) { return document_status == status; });
	}

	template <typename DocumentsFilter>
	vector<Document> FindTopDocuments(const string& query, DocumentsFilter documents_filter) const
	{
		vector<Document> result = FindAllDocuments(query, documents_filter);
		SortAndResizeTopDocuments(result, MAX_RESULT_DOCUMENT_COUNT);
		return result;
	}

	tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const
	{
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
