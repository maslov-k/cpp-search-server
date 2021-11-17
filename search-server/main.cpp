#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <cmath>
#include <execution>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
	string s;
	getline(cin, s);
	return s;
}

int ReadLineWithNumber() {
	int result;
	cin >> result;
	ReadLine();
	return result;
}

vector<int> ReadRatingsLine() {
	int n;
	cin >> n;
	vector<int> result(n, 0);
	for (int& rating : result) {
		cin >> rating;
	}
	ReadLine();
	return result;
}

struct Document {
	int id;
	double relevance;
	int rating;
};

enum class DocumentStatus {
	ACTUAL,
	IRRELEVANT,
	BANNED,
	REMOVED
};

class SearchServer {
private:
	struct Document_params {
		int rating;
		DocumentStatus status;
	};

	map<string, map<int, double>> word_to_documents_freqs_;
	set<string> stop_words_;

	map<int, Document_params> documents_params_;

	static int ComputeAverageRating(const vector<int>& ratings) {
		return reduce(execution::par, ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
	}

	static vector<string> SplitIntoWords(const string& text) {
		vector<string> words;
		string word;
		for (const char c : text) {
			if (c == ' ') {
				words.push_back(word);
				word = "";
			}
			else {
				word += c;
			}
		}
		words.push_back(word);

		return words;
	}

	vector<string> SplitIntoWordsNoStop(const string& text) const {
		vector<string> words;
		for (const string& word : SplitIntoWords(text)) {
			if (stop_words_.count(word) == 0) {
				words.push_back(word);
			}
		}
		return words;
	}

	struct Query {
		set<string> plus_words;
		set<string> minus_words;
	};

	Query ParseQuery(const string& query) const {
		Query query_words;
		for (const string& word : SplitIntoWordsNoStop(query)) {
			if (word.rfind("-", 0) == 0) {
				query_words.minus_words.insert(word.substr(1));
			} else {
				query_words.plus_words.insert(word);
			}
		}

		return query_words;
	}

	void SortAndResizeTopDocuments(vector<Document>& matched_documents) const{
		sort(
			matched_documents.begin(),
			matched_documents.end(),
			[](const Document& a, const Document& b) {
				return ((a.relevance - b.relevance) > 10e-6) || ((abs(a.relevance - b.relevance) < 10e-6) && (a.rating > b.rating));
			}
		);
		if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
			matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
		}
	}

	template <typename DocumentsFilter>
	vector<Document> FindAllDocuments(const string& query, DocumentsFilter documents_filter) const {
		const Query query_words = ParseQuery(query);

		set<int> documents_with_minus_words;

		for (const string& minus_word : query_words.minus_words) {
			for (const auto& [id, tf] : word_to_documents_freqs_.at(minus_word)) {
				documents_with_minus_words.insert(id);
			}
		}

		map<int, double> document_to_relevance;

		for (const string& word : query_words.plus_words) {
			if (word_to_documents_freqs_.count(word) == 0) {
				continue;
			}
			for (const auto& [id, tf] : word_to_documents_freqs_.at(word)) {
				if (documents_with_minus_words.count(id) == 0 && documents_filter(id, documents_params_.at(id).status, documents_params_.at(id).rating)) {
					document_to_relevance[id] += tf * log(static_cast<double>(GetDocumentCount()) / word_to_documents_freqs_.at(word).size());
				}
			}
		}

		vector<Document> matched_documents;
		for (auto [document_id, relevance] : document_to_relevance) {
			matched_documents.push_back({ document_id, relevance, documents_params_.at(document_id).rating });
		}

		return matched_documents;
	}

public:
	int GetDocumentCount() const {
		return documents_params_.size();
	}

	void SetStopWords(const string& text) {
		for (const string& word : SplitIntoWords(text)) {
			stop_words_.insert(word);
		}
	}

	void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
		const vector<string> document_words = SplitIntoWordsNoStop(document);
		const double document_size = document_words.size();
		for (const string& word : document_words) {
			word_to_documents_freqs_[word][document_id] = count(document_words.begin(), document_words.end(), word) / document_size;
		}

		documents_params_[document_id].rating = ComputeAverageRating(ratings);
		documents_params_[document_id].status = status;
	}

	vector<Document> FindTopDocuments(const string& query) const {
		auto matched_documents = FindAllDocuments(query, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::ACTUAL; });

		SortAndResizeTopDocuments(matched_documents);
		return matched_documents;
	}

	vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
		return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus status1, int rating) { return status1 == status; });
	}

	template <typename DocumentsFilter>
	vector<Document> FindTopDocuments(const string& query, DocumentsFilter documents_filter) const {
		auto matched_documents = FindAllDocuments(query, documents_filter);

		SortAndResizeTopDocuments(matched_documents);
		return matched_documents;
	}

	tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
		vector<string> result;
		Query query_words = ParseQuery(raw_query);
		for (const string& word : query_words.minus_words) {
			if (word_to_documents_freqs_.at(word).count(document_id)) {
				return { vector<string>(), documents_params_.at(document_id).status };
			}
		}
		for (const string& word : query_words.plus_words) {
			if (word_to_documents_freqs_.at(word).count(document_id) && !count(result.begin(), result.end(), word)) {
				result.push_back(word);
			}
		}
		sort(result.begin(), result.end());
		return { result, documents_params_.at(document_id).status };
	}
};




void PrintDocument(const Document& document) {
	cout << "{ "s
		<< "document_id = "s << document.id << ", "s
		<< "relevance = "s << document.relevance << ", "s
		<< "rating = "s << document.rating
		<< " }"s << endl;
}

int main() {
	SearchServer search_server;
	search_server.SetStopWords("и в на"s);

	search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
	search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
	search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
	search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });

	cout << "ACTUAL by default:"s << endl;
	for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
		PrintDocument(document);
	}

	cout << "BANNED:"s << endl;
	for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED)) {
		PrintDocument(document);
	}

	cout << "Even ids:"s << endl;
	for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
		PrintDocument(document);
	}

	return 0;
}