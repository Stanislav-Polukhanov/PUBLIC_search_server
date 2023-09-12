#pragma once
#include "document.h"
#include "string_processing.h"
#include "log_duration.h"
#include "concurrent_map.h"

#include <execution>
#include <map>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <deque>
#include <type_traits>

const int MAX_RESULT_DOCUMENT_COUNT = 5;

//чувствительность поиска по рейтингу
constexpr double COMPARISON_TOLERANCE = 1e-6;

using namespace std::string_literals;
class SearchServer {
public://========================================================================
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);
    explicit SearchServer(const std::string& stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text))
    {}
    explicit SearchServer(const std::string_view& stop_words_sv)
        : SearchServer(SplitIntoWords(stop_words_sv))
    {}

    void AddDocument(int document_id, const std::string_view& document, DocumentStatus status,
        const std::vector<int>& ratings);

    //обычная версия
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query_sv, DocumentPredicate document_predicate) const;
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query) const;
    //с передачей execution::
    template <typename ExecPolicy, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const ExecPolicy& policy, const std::string_view& raw_query_sv, DocumentPredicate document_predicate) const;
    template <typename ExecPolicy>
    std::vector<Document> FindTopDocuments(const ExecPolicy& policy, const std::string_view& raw_query, DocumentStatus status) const;
    template <typename ExecPolicy>
    std::vector<Document> FindTopDocuments(const ExecPolicy& policy, const std::string_view& raw_query) const;

    int GetDocumentCount() const;

    using MatchedWords_Status = std::tuple<std::vector<std::string_view>, DocumentStatus>;
    MatchedWords_Status MatchDocument(const std::string_view& raw_query_sv, int document_id) const;
    MatchedWords_Status MatchDocument(const std::execution::parallel_policy&, const std::string_view& raw_query_sv, int document_id) const;
    MatchedWords_Status MatchDocument(const std::execution::sequenced_policy&, const std::string_view& raw_query_sv, int document_id) const;

    std::vector<int>::const_iterator begin() const;

    std::vector<int>::const_iterator end() const;

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    int GetDocumentId(int index) const;

    void RemoveDocument(int document_id);

    void RemoveDocument(const std::execution::parallel_policy&, int document_id);

    void RemoveDocument(const std::execution::sequenced_policy&, int document_id);

private://========================================================================
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    const std::set<std::string, std::less<>> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;
    std::map<int, DocumentData> documents_;
    std::vector<int> document_ids_;
    std::deque<std::string> doc_string_storage_;

    bool IsStopWord(const std::string_view& word) const {  return stop_words_.count(word) > 0;  }

    static bool IsValidWord(const std::string_view& word);

    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view& text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(const std::string_view& text) const;

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    Query ParseQuery(const std::string_view& text, bool is_parallel = false) const;

    double ComputeWordInverseDocumentFreq(const std::string_view& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments( //sequenced
        const Query& query, 
        DocumentPredicate document_predicate
    ) const;

    template <typename ExecPolicy, typename DocumentPredicate>
    std::vector<Document> FindAllDocuments( //ExecPolicy
        const ExecPolicy& policy,
        const Query& query, 
        DocumentPredicate document_predicate
    ) const;
};

///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////
template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words))
{
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw std::invalid_argument("Some of stop words are invalid"s);
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view& raw_query_sv,
    DocumentPredicate document_predicate) const
{
    const Query query = ParseQuery(raw_query_sv);

    std::vector<Document> matched_documents = FindAllDocuments(query, document_predicate);

    sort(matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            return lhs.relevance > rhs.relevance
                || (std::abs(lhs.relevance - rhs.relevance) < COMPARISON_TOLERANCE && lhs.rating > rhs.rating);
        });

    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <typename ExecPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const ExecPolicy& policy, const std::string_view& raw_query_sv,
    DocumentPredicate document_predicate) const
{
    if constexpr (std::is_same_v<ExecPolicy, std::execution::sequenced_policy>) {
        return FindTopDocuments(raw_query_sv, document_predicate);
    }

    const Query query = ParseQuery(raw_query_sv);
    std::vector<Document> matched_documents = FindAllDocuments(policy, query, document_predicate);

    sort(policy,
        matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            return lhs.relevance > rhs.relevance
                || (std::abs(lhs.relevance - rhs.relevance) < COMPARISON_TOLERANCE && lhs.rating > rhs.rating);
        });

    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <typename ExecPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecPolicy& policy, const std::string_view& raw_query, DocumentStatus status) const {
    if constexpr (std::is_same_v<ExecPolicy, std::execution::sequenced_policy>) {
        return FindTopDocuments(raw_query, status);
    }
    return SearchServer::FindTopDocuments(
        policy,
        raw_query,
        [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        }
    );
}
template <typename ExecPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecPolicy& policy, const std::string_view& raw_query) const {
    if constexpr (std::is_same_v<ExecPolicy, std::execution::sequenced_policy>) {
        return FindTopDocuments(raw_query);
    }
    return SearchServer::FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query,
    DocumentPredicate document_predicate) const 
{
    std::map<int, double> document_to_relevance;
    for (const std::string_view& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }
    for (const std::string_view& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto& [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto& [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}

template <typename ExecPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(
    const ExecPolicy& policy,
    const Query& query,
    DocumentPredicate document_predicate) const
{
    //LOG_DURATION_STREAM("FindAllDocuments"s, std::cout);
    if constexpr (std::is_same_v<ExecPolicy, std::execution::sequenced_policy >) {
        return FindAllDocuments(query, document_predicate);
    }

    ConcurrentMap<int, double> document_to_relevance(8);
    for_each(
        policy,
        query.plus_words.begin(),
        query.plus_words.end(),
        [this, &document_to_relevance, document_predicate](const std::string_view& word) {
            if (word_to_document_freqs_.count(word)) {
                const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                    const DocumentData& document_data = documents_.at(document_id);
                    if (document_predicate(document_id, document_data.status, document_data.rating)) {
                        document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                    }
                }
            }
        }
    );

    for_each( 
        policy,
        query.minus_words.begin(),
        query.minus_words.end(),
        [this, &document_to_relevance](const std::string_view& word) {
            if (word_to_document_freqs_.count(word)) {
                for (const auto& [document_id, _] : word_to_document_freqs_.at(word)) {
                    document_to_relevance.erase(document_id);
                }
            }
        }
    );

    std::vector<Document> matched_documents;
    std::map<int, double> document_to_relevance_map = document_to_relevance.BuildOrdinaryMap();
    matched_documents.reserve(document_to_relevance_map.size());

    for_each(
        policy,
        document_to_relevance_map.begin(), document_to_relevance_map.end(),
        [this, &matched_documents](const std::pair<int, double>& id_relevance) {
            matched_documents.push_back({ id_relevance.first, id_relevance.second, documents_.at(id_relevance.first).rating });
        }
    );

    return matched_documents;
}


void PrintDocument(const Document& document);
void PrintMatchDocumentResult(int document_id, const std::vector<std::string>& words, DocumentStatus status);

void AddDocument(SearchServer& search_server, int document_id, const std::string& document,
    DocumentStatus status, const std::vector<int>& ratings);

void FindTopDocuments(const SearchServer& search_server, const std::string& raw_query);

void MatchDocuments(const SearchServer& search_server, const std::string& query);