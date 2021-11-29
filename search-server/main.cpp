ostream& operator<<(ostream& out, const DocumentStatus& status) {
	out << "Status: " << static_cast<int>(status);
	return out;
}


template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
    const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;\
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
    const string& hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl((expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl((expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))


template <typename F>
void RunTestImpl(const F& func, const string& func_str) {
	func();
	cerr << func_str << " OK" << endl;
}

#define RUN_TEST(func)  RunTestImpl((func), #func)


void TestAddingDocuments() {
	const int doc_id = 2;
	const string content = "cat in the city"s;
	const vector<int> ratings = { 1, 2, 3 };

	SearchServer server;
	server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
	const auto found = server.FindTopDocuments("cat"s);
	ASSERT_EQUAL(found.size(), 1u);
}

void TestExcludeStopWordsFromAddedDocumentContent() {
	const int doc_id = 42;
	const string content = "cat in the city"s;
	const vector<int> ratings = { 1, 2, 3 };
	{
		SearchServer server;
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		const auto found_docs = server.FindTopDocuments("in"s);
		ASSERT_EQUAL(found_docs.size(), 1u);
		const Document& doc0 = found_docs[0];
		ASSERT_EQUAL(doc0.id, doc_id);
	}

	{
		SearchServer server;
		server.SetStopWords("in the"s);
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
	}
}

void TestMinusWords() {
	const int doc_id = 42;
	const string content1 = "cat in the city"s;
	const string content2 = "cat in city"s;
	const vector<int> ratings = { 1, 2, 3 };

	SearchServer server;
	server.AddDocument(doc_id, content1, DocumentStatus::ACTUAL, ratings);
	server.AddDocument(doc_id + 1, content2, DocumentStatus::ACTUAL, ratings);
	const auto found_docs = server.FindTopDocuments("in -the city"s);
	ASSERT_EQUAL(found_docs.size(), 1u);
}

void TestMatching() {
	const int doc_id = 42;
	const string content1 = "cat in the city"s;
	const string content2 = "cat in city"s;
	const vector<int> ratings = { 1, 2, 3 };


	SearchServer server;
	server.SetStopWords("the a b"s);
	server.AddDocument(doc_id, content1, DocumentStatus::ACTUAL, ratings);
	server.AddDocument(doc_id + 1, content2, DocumentStatus::ACTUAL, ratings);
	const auto [matched, status] = server.MatchDocument("in the city"s, doc_id);

	vector<string> words{ "city", "in" };
	ASSERT_EQUAL(matched, words);
}

void TestSortByRelevance() {
	SearchServer server;
	server.AddDocument(0, "one two three"s, DocumentStatus::ACTUAL, { 8, -3 });
	server.AddDocument(1, "three five four"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
	server.AddDocument(2, "six one two seven"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });

	vector<int> id_order{ 0, 2, 1 };
	vector<Document> found = server.FindTopDocuments("two three one"s);
	for (size_t i = 0; i < found.size(); ++i) {
		ASSERT_EQUAL(id_order[i], found[i].id);
	}
}

void TestRating() {
	SearchServer server;
	server.AddDocument(0, "three five four"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
	ASSERT_EQUAL(server.FindTopDocuments("two three one"s)[0].rating, (7 + 2 + 7) / 3);
}

void TestPredicate() {
	SearchServer server;
	server.AddDocument(0, "three five four"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
	server.AddDocument(1, "six seven five four"s, DocumentStatus::BANNED, { 17, 22, 7 });

	const auto predicate = [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::BANNED; };
	vector<Document> found = server.FindTopDocuments("three five four"s, predicate);
	ASSERT_EQUAL(found.size(), 1u);
	ASSERT_EQUAL(found[0].id, 1);
}

void TestSearchByStatus() {
	SearchServer server;
	server.AddDocument(0, "three five four"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
	server.AddDocument(1, "six seven five four"s, DocumentStatus::BANNED, { 17, 22, 7 });

	vector<Document> found = server.FindTopDocuments("three five four"s, DocumentStatus::BANNED);
	ASSERT_EQUAL(found.size(), 1u);
	ASSERT_EQUAL(found[0].id, 1);
}

void TestRelevanceComputing() {
	SearchServer server;
	server.AddDocument(0, "three five four"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
	server.AddDocument(1, "three eight nine"s, DocumentStatus::ACTUAL, { 6, 2, 7 });

	double relevance = server.FindTopDocuments("three one four"s)[0].relevance;
	double relevanse0 = (log(2 / 2) / 3. + log(2) / 3);
	ASSERT((relevance - relevanse0) < 1e6);
}

void TestSearchServer() {
	RUN_TEST(TestAddingDocuments);
	RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
	RUN_TEST(TestMinusWords);
	RUN_TEST(TestMatching);
	RUN_TEST(TestSortByRelevance);
	RUN_TEST(TestRating);
	RUN_TEST(TestPredicate);
	RUN_TEST(TestSearchByStatus);
	RUN_TEST(TestRelevanceComputing);
}