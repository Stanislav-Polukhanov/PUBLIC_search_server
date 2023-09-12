#include "string_processing.h"

using namespace std;

std::vector<std::string_view> SplitIntoWords(const std::string_view& text_sv) {
    std::vector<std::string_view> words;
    std::string_view word;

    //////////////// C++20 !!!!!!!!!!!!!!!!!!!!!!!
    //std::string_view::const_iterator it1 = text_sv.begin();
    //std::string_view::const_iterator it2 = text_sv.begin();

    //while (it2 != text_sv.end()) {
    //    if (*it2 == ' ') {
    //        std::string_view new_sv(it1, it2);
    //        if (!new_sv.empty()) { words.push_back(new_sv); }
    //        ++it2;
    //        it1 = it2;
    //    }
    //    else {
    //        ++it2;
    //    }
    //}

    //std::string_view new_sv(it1, it2);
    //if (!new_sv.empty()) { words.push_back(new_sv); }

    size_t begin = 0;
    size_t end = 0;
    while (end < text_sv.size()) {
        if (text_sv[end] == ' ') {
            if (begin != end) {
                words.push_back(text_sv.substr(begin, end - begin));
            }
            ++end;
            begin = end;
        }
        else { ++end; }
    }

    words.push_back(text_sv.substr(begin, end));

    return words;
}