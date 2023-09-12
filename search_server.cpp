#include "search_server.h"
#include "log_duration.h"
#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <execution>

using namespace std;

bool SearchServer::IsValidWord(const std::string_view& word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {  return c >= '\0' && c < ' ';  });
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(const std::string_view& text) const {
    std::vector<std::string_view> words;
    for (const std::string_view& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            std::string s = ("Word "s + std::string(word) + " is invalid"s);
            throw std::invalid_argument(s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    int rating_sum = 0;
    for (const int rating : ratings) {
        rating_sum += rating;
    }
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(const std::string_view& text_sv) const {  
    if (text_sv.empty()) {
        throw std::invalid_argument("Query word is empty"s);
    }
    std::string_view word = text_sv;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word.remove_prefix(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw std::invalid_argument(("Query word "s + std::string(text_sv) + " is invalid"s));
    }
    return { word, is_minus, IsStopWord(word) };
}

SearchServer::Query SearchServer::ParseQuery(const std::string_view& raw_query, bool is_parallel) const {
    Query result;

    for (const std::string_view& word : SplitIntoWords(raw_query)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            }
            else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }

    if (!is_parallel)//параллельная версия без сортировки!!!
    {
        std::sort(result.minus_words.begin(), result.minus_words.end());
        result.minus_words.erase(std::unique(result.minus_words.begin(), result.minus_words.end()), result.minus_words.end());
        std::sort(result.plus_words.begin(), result.plus_words.end());
        result.plus_words.erase(std::unique(result.plus_words.begin(), result.plus_words.end()), result.plus_words.end());
    }

    return result;
}

void SearchServer::AddDocument(int document_id, const std::string_view& document, DocumentStatus status, const std::vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw std::invalid_argument("Invalid document_id"s);
    }
    const std::vector<std::string_view> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for (const std::string_view& word : words) {
        doc_string_storage_.push_back(std::string(word));
        word_to_document_freqs_[std::string_view(doc_string_storage_.back())][document_id] += inv_word_count;
        document_to_word_freqs_[document_id][std::string_view(doc_string_storage_.back())] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    document_ids_.push_back(document_id);
}

vector<Document> SearchServer::FindTopDocuments(const string_view& raw_query, DocumentStatus status) const {
    return SearchServer::FindTopDocuments(
        raw_query, 
        [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        });
}

vector<Document> SearchServer::FindTopDocuments(const std::string_view& raw_query) const 
{ return SearchServer::FindTopDocuments(raw_query, DocumentStatus::ACTUAL); }

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

using MatchedWords_Status = std::tuple<std::vector<std::string_view>, DocumentStatus>;
MatchedWords_Status SearchServer::MatchDocument(const std::string_view& raw_query_sv,
    int document_id) const //последовательная версия
{
    Query query = ParseQuery(raw_query_sv);

    std::vector<std::string_view> matched_words;

    for (const string_view& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            return { matched_words, documents_.at(document_id).status };
        }
    }

    for (const string_view& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    return { matched_words, documents_.at(document_id).status };
}

MatchedWords_Status SearchServer::MatchDocument(const std::execution::sequenced_policy&,
    const std::string_view& raw_query_sv,
    int document_id) const
{
    return MatchDocument(raw_query_sv, document_id);
}

MatchedWords_Status SearchServer::MatchDocument(const std::execution::parallel_policy& policy,
    const std::string_view& raw_query_sv, //параллельная версия
    int document_id) const
{
    Query query = ParseQuery(raw_query_sv, true);
    vector<string_view> matched_words(query.plus_words.size());

    if (std::any_of(policy,
                    query.minus_words.begin(), 
                    query.minus_words.end(), 
                    [&](const string_view& word) 
                        {
                            if (word_to_document_freqs_.count(word) > 0 && word_to_document_freqs_.at(word).count(document_id)) {
                                matched_words.clear();
                                return true;
                            }
                            return false;
                        }
                   )
        ) 
    {return { matched_words, documents_.at(document_id).status };}

    auto end = std::copy_if(policy,
                            query.plus_words.begin(), 
                            query.plus_words.end(), 
                            matched_words.begin(), 
                            [&](const std::string_view& word){
                                                                if (word_to_document_freqs_.count(word) == 0) {
                                                                    return false;
                                                                }
                                                                if (word_to_document_freqs_.at(word).count(document_id)) {
                                                                    return true;
                                                                }
                                                                return false;
                                                             }
    );

    //сортировка и повторы
    std::sort(policy, matched_words.begin(), end);
    matched_words.erase(std::unique(policy, matched_words.begin(), end), matched_words.end());

    return { matched_words, documents_.at(document_id).status };
}

std::vector<int>::const_iterator SearchServer::begin() const
{
    return document_ids_.begin();
}

std::vector<int>::const_iterator SearchServer::end() const
{
    return document_ids_.end();
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const
{
    static std::map<std::string_view, double> word_freqs;
    if (documents_.count(document_id) == 0) {
        return word_freqs;
    }
    word_freqs = document_to_word_freqs_.at(document_id);

    return word_freqs;
}

void SearchServer::RemoveDocument(int document_id) 
{
    if (!documents_.count(document_id)) { return; }
    document_to_word_freqs_.erase(document_id);
    documents_.erase(document_id);
    auto it0 = std::remove(document_ids_.begin(), document_ids_.end(), document_id);
    document_ids_.erase(it0, document_ids_.end());

    for (auto& element : word_to_document_freqs_)
    {
        auto it = element.second.find(document_id);
        if (it != element.second.end())
        {
            element.second.erase(it);
        }
    }
}

void SearchServer::RemoveDocument(const std::execution::parallel_policy& policy, int document_id)
{
    if (!documents_.count(document_id)) { return; }
    std::vector<std::string_view> word_ptrs(document_to_word_freqs_.at(document_id).size());
    std::transform(policy, document_to_word_freqs_.at(document_id).begin(), document_to_word_freqs_.at(document_id).end(), word_ptrs.begin(),
        [](const auto& pair) { return pair.first; });
    std::for_each(policy, word_ptrs.begin(), word_ptrs.end(),
        [this, document_id](auto word)
        { word_to_document_freqs_.at(word).erase(document_id); });

    document_to_word_freqs_.erase(document_id);
    documents_.erase(document_id);
    document_ids_.erase(std::remove(document_ids_.begin(), document_ids_.end(), document_id), document_ids_.end());
}

void SearchServer::RemoveDocument(const std::execution::sequenced_policy&, int document_id) {
    return RemoveDocument(document_id);
}

void PrintDocument(const Document& document) {
    std::cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s << std::endl;
}

void PrintMatchDocumentResult(int document_id, const std::vector<std::string_view>& words, DocumentStatus status) {
    std::cout << "{ "s
        << "document_id = "s << document_id << ", "s
        << "status = "s << static_cast<int>(status) << ", "s
        << "words ="s;
    for (const std::string_view& word : words) {
        std::cout << ' ' << word;
    }
    std::cout << "}"s << std::endl;
}

void AddDocument(SearchServer& search_server, int document_id, const std::string& document,
    DocumentStatus status, const std::vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    }
    catch (const std::exception& e) {
        std::cout << "Error in adding document "s << document_id << ": "s << e.what() << std::endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const std::string& raw_query) {
    std::cout << "Results for request: "s << raw_query << std::endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    }
    catch (const std::exception& e) {
        std::cout << "Error is seaching: "s << e.what() << std::endl;
    }
}

int SearchServer::GetDocumentId(int index) const {
    return document_ids_.at(index);
}

void MatchDocuments(const SearchServer& search_server, const std::string& query) {
    try {
        std::cout << "Matching for request: "s << query << std::endl;
        const int document_count = search_server.GetDocumentCount();
        for (int index = 0; index < document_count; ++index) {
            const int document_id = search_server.GetDocumentId(index);
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    }
    catch (const std::exception& e) {
        std::cout << "Error in matchig request "s << query << ": "s << e.what() << std::endl;
    }
}