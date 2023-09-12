#pragma once

#include <string>
#include <vector>
#include "search_server.h"
#include <cassert>
#include <iostream>
#include "remove_duplicates.h"

using namespace std;

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str, const std::string& u_str, const std::string& file,
    const std::string& func, unsigned line, const std::string& hint) {
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

//=========================================================================================
// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::string raw_query = "cat"s;
    const std::vector<int> ratings = { 1, 2, 3 };
    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments(raw_query);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    //{
    //    SearchServer server;
    //    server.SetStopWords("in the"s); //<<<==========больше нет такой функции==========
    //    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    //    assert(server.FindTopDocuments("in"s).empty());
    //}
}

//=========================================================================================
void TestMinusWords() {
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = { 1, 2, 3 };

    const int doc_id2 = 13;
    const std::string content2 = "cat in the country"s;
    const std::vector<int> ratings2 = { 4, 5, 6 };
    SearchServer server("in the"s);
    server.AddDocument(doc_id, "cat in the city"s, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id2, "cat in the country"s, DocumentStatus::ACTUAL, ratings2);
    //должен найтись только один документ - второй отсеится минус-словом
    const auto found_docs = server.FindTopDocuments("cat -country"s);
    ASSERT_EQUAL(found_docs.size(), 1);
    const Document doc0 = found_docs[0];
    ASSERT_EQUAL(doc0.id, doc_id);
    //без минуса должны найтись оба документа
    const auto found_docs2 = server.FindTopDocuments("cat"s);
    ASSERT_EQUAL(found_docs2.size(), 2);
}

//=========================================================================================
void TestMatchDocuments() {
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::string query = "cat in the"s;
    const std::vector<int> ratings = { 1, 2, 3 };

    SearchServer server("in the"s);
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    std::tuple<std::vector<std::string_view>, DocumentStatus> result = server.MatchDocument(query, doc_id);
    ASSERT_EQUAL((get<0>(result)).size(), 1);

    result = server.MatchDocument("-cat in the"s, doc_id);
    ASSERT_EQUAL((get<0>(result)).size(), 0);
}

//=========================================================================================
void Test_SortByRelevance_RelCalc_RatingCalc() {
    const int doc_id = 1;
    const std::string content = "cat flying in the space"s;
    const std::vector<int> ratings = { 1, 2, 3 };

    const int doc_id2 = 2;
    const std::string content2 = "random cat in the country"s;
    const std::vector<int> ratings2 = { 4, 5, 6 };

    const int doc_id3 = 3;
    const std::string content3 = "old cat in the country"s;
    const std::vector<int> ratings3 = { 1, 3, 5 };

    SearchServer server("hell"s);
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings2);
    server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings3);

    std::vector<Document> search_results = server.FindTopDocuments("old country cat"s);

    double idf_old = log(3.0 / 1);
    double idf_country = log(3.0 / 2);
    double idf_cat = log(3.0 / 3);

    double tf_old_1 = 0.0 / 5;
    double tf_old_2 = 0.0 / 5;
    double tf_old_3 = 1.0 / 5;

    double tf_country_1 = 0.0 / 5;
    double tf_country_2 = 1.0 / 5;
    double tf_country_3 = 1.0 / 5;

    double tf_cat_1 = 1.0 / 5;
    double tf_cat_2 = 1.0 / 5;
    double tf_cat_3 = 1.0 / 5;

    double tf_idf_1 = idf_old * tf_old_1 +
        idf_country * tf_country_1 +
        idf_cat * tf_cat_1;

    double tf_idf_2 = idf_old * tf_old_2 +
        idf_country * tf_country_2 +
        idf_cat * tf_cat_2;

    double tf_idf_3 = idf_old * tf_old_3 +
        idf_country * tf_country_3 +
        idf_cat * tf_cat_3;

    //тестовый вывод на всякий случай
    cout << tf_idf_1 << endl
        << tf_idf_2 << endl
        << tf_idf_3 << endl
        << search_results[2].relevance << endl
        << search_results[1].relevance << endl
        << search_results[0].relevance << endl;

    assert(search_results[0].id == 3 && search_results[1].id == 2 && search_results[2].id == 1);
    assert(search_results[0].rating == 3 && search_results[1].rating == 5 && search_results[2].rating == 2);
    assert(search_results[0].relevance > search_results[1].relevance && search_results[1].relevance > search_results[2].relevance);

    assert(tf_idf_1 == search_results[2].relevance);
    assert(tf_idf_2 == search_results[1].relevance);
    assert(tf_idf_3 == search_results[0].relevance);
}

//=========================================================================================
void TestStatusFiltering() {
    SearchServer server("in the"s);
    const std::string word = "word"s;
    server.AddDocument(1, word, DocumentStatus::ACTUAL, { 1,2,3 });
    server.AddDocument(2, word, DocumentStatus::BANNED, { 1,2,3 });
    server.AddDocument(3, word, DocumentStatus::IRRELEVANT, { 1,2,3 });
    server.AddDocument(4, word, DocumentStatus::REMOVED, { 1,2,3 });

    const auto search_result1 = server.FindTopDocuments("word"s, DocumentStatus::ACTUAL);
    const auto search_result2 = server.FindTopDocuments("word"s, DocumentStatus::BANNED);
    const auto search_result3 = server.FindTopDocuments("word"s, DocumentStatus::IRRELEVANT);
    const auto search_result4 = server.FindTopDocuments("word"s, DocumentStatus::REMOVED);

    assert(search_result1.size() == 1 && search_result1[0].id == 1);
    assert(search_result2.size() == 1 && search_result2[0].id == 2);
    assert(search_result3.size() == 1 && search_result3[0].id == 3);
    assert(search_result4.size() == 1 && search_result4[0].id == 4);
}

//=========================================================================================
void TestPredicateFiltering() {
    SearchServer server("in the"s);
    const std::string word = "word"s;
    server.AddDocument(1, word, DocumentStatus::ACTUAL, { 1,2,3 });
    server.AddDocument(2, word, DocumentStatus::ACTUAL, { 2,3,4 });
    server.AddDocument(3, word, DocumentStatus::ACTUAL, { 3,4,5 });
    server.AddDocument(4, word, DocumentStatus::ACTUAL, { 4,5,6 });

    const auto search_result1 = server.FindTopDocuments("word"s, [](int document_id, DocumentStatus status, int rating) { return document_id == 1; });
    const auto search_result2 = server.FindTopDocuments("word"s, [](int document_id, DocumentStatus status, int rating) { return rating == 3; });
    //const auto search_result3 = server.FindTopDocuments("word"s, DocumentStatus::IRRELEVANT);
    //const auto search_result4 = server.FindTopDocuments("word"s, DocumentStatus::REMOVED);

    assert(search_result1.size() == 1 && search_result1[0].id == 1);
    assert(search_result2.size() == 1 && search_result2[0].id == 2);
    //assert(search_result3.size() == 1 && search_result3[0].id == 3);
    //assert(search_result4.size() == 1 && search_result4[0].id == 4);
}

//=========================================================================================
void TestDublicates() {
    SearchServer search_server("and with"s);

    AddDocument(search_server, 1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    AddDocument(search_server, 2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    // дубликат документа 2, будет удалён
    AddDocument(search_server, 3, "funny pet with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    // отличие только в стоп-словах, считаем дубликатом
    AddDocument(search_server, 4, "funny pet and curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    // множество слов такое же, считаем дубликатом документа 1
    AddDocument(search_server, 5, "funny funny pet and nasty nasty rat"s, DocumentStatus::ACTUAL, { 1, 2 });

    // добавились новые слова, дубликатом не является
    AddDocument(search_server, 6, "funny pet and not very nasty rat"s, DocumentStatus::ACTUAL, { 1, 2 });

    // множество слов такое же, как в id 6, несмотря на другой порядок, считаем дубликатом
    AddDocument(search_server, 7, "very nasty rat and not very funny pet"s, DocumentStatus::ACTUAL, { 1, 2 });

    // есть не все слова, не является дубликатом
    AddDocument(search_server, 8, "pet with rat and rat and rat"s, DocumentStatus::ACTUAL, { 1, 2 });

    // слова из разных документов, не является дубликатом
    AddDocument(search_server, 9, "nasty rat with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });
    AddDocument(search_server, 9, "nasty rat with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 }); //копия документа, не должен добавиться


    cout << "Before duplicates removed: "s << search_server.GetDocumentCount() << endl;
    RemoveDuplicates(search_server);
    cout << "After duplicates removed: "s << search_server.GetDocumentCount() << endl;
}


//=========================================================================================
void PrintMatchDocumentResultUTest(int document_id, const std::vector<std::string_view>& words,
    DocumentStatus status) {
    std::cout << "{ "
        << "document_id = " << document_id << ", "
        << "status = " << static_cast<int>(status) << ", "
        << "words =";
    for (const string_view& word : words) {
        std::cout << ' ' << word;
    }
    std::cout << "}" << std::endl;
}

void PrintDocumentUTest(const Document& document) {
    std::cout << "{ "
        << "document_id = " << document.id << ", "
        << "relevance = " << document.relevance << ", "
        << "rating = " << document.rating << " }" << std::endl;
}

void TestMatch() {
    const std::vector<int> ratings1 = { 1, 2, 3, 4, 5 };
    const std::vector<int> ratings2 = { -1, -2, 30, -3, 44, 5 };
    const std::vector<int> ratings3 = { 12, -20, 80, 0, 8, 0, 0, 9, 67 };
    const std::vector<int> ratings4 = { 7, 0, 3, -49, 5 };
    const std::vector<int> ratings5 = { 81, -6, 7, 94, -7 };
    const std::vector<int> ratings6 = { 41, 8, -7, 897, 5 };
    const std::vector<int> ratings7 = { 543, 0, 43, 4, -5 };
    const std::vector<int> ratings8 = { 91, 7, 3, -88, 56 };
    const std::vector<int> ratings9 = { 0, -87, 93, 66, 5 };
    const std::vector<int> ratings10 = { 11, 2, -43, 4, 895 };
    std::string stop_words = "and in on";
    SearchServer search_server(stop_words);

    const std::vector<std::string> documents = {
        "white cat and stylish collar", //-
        "fluffy cat fluffy tail",
        "fine dog big eyes",
        "white stylish cat",
        "fluffy cat dog",
        "fine collar big eyes",         //-
        "cat and collar",               //-
        "dog and tail",
        "stylish dog fluffy tail",
        "cat fluffy collar",            //-
        "fine cat and dog",
        "tail and big eyes"
    };

    search_server.AddDocument(0, documents[0],   DocumentStatus::ACTUAL,     ratings1);
    search_server.AddDocument(1, documents[1],   DocumentStatus::ACTUAL,     ratings2);
    search_server.AddDocument(2, documents[2],   DocumentStatus::ACTUAL,     ratings3);
    search_server.AddDocument(3, documents[3],   DocumentStatus::IRRELEVANT, ratings1);
    search_server.AddDocument(4, documents[4],   DocumentStatus::ACTUAL,     ratings2);
    search_server.AddDocument(5, documents[5],   DocumentStatus::IRRELEVANT, ratings3);
    search_server.AddDocument(6, documents[6],   DocumentStatus::BANNED,     ratings1);
    search_server.AddDocument(7, documents[7],   DocumentStatus::BANNED,     ratings2);
    search_server.AddDocument(8, documents[8],   DocumentStatus::ACTUAL,     ratings3);
    search_server.AddDocument(9, documents[9],   DocumentStatus::REMOVED,    ratings1);
    search_server.AddDocument(10, documents[10], DocumentStatus::ACTUAL,     ratings2);
    search_server.AddDocument(11, documents[11], DocumentStatus::REMOVED,    ratings3);

    const std::string query = "fluffy fine cat -collar";
    const auto result = search_server.FindTopDocuments(query);

    std::cout << std::endl << "Top documents for query:" << std::endl;
    for (const Document& document : result) {
        PrintDocumentUTest(document);
    }

    std::cout << std::endl << "Documents' statuses:" << std::endl;
    const int document_count = search_server.GetDocumentCount();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        const auto [words, status] = search_server.MatchDocument(query, document_id);
        PrintMatchDocumentResultUTest(document_id, words, status);
    }
}



// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    TestExcludeStopWordsFromAddedDocumentContent();
    TestMinusWords();
    TestMatchDocuments();
    Test_SortByRelevance_RelCalc_RatingCalc();
    TestStatusFiltering();
    TestPredicateFiltering();
    TestDublicates();

    cout << "tests.h: All old tests OK"s << endl;

    TestMatch();
}