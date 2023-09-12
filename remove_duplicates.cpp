#include "remove_duplicates.h"
#include <iterator>
#include <execution>

void RemoveDuplicates(SearchServer& search_server)
{
    //Проверка на дубликаты на этапе создания БД
    std::set <std::set<std::string>> set_words;
    std::set<int> ids_to_delete; //перечень ИД, которые надо удалить. Set - потому что уникальные по-любому не пройдут, а так зато будет красивая сортировка
    for (int id : search_server)
    {
        std::set<std::string> words;     //оставляем только слова, отсекаем частоты
        for (const auto& element : search_server.GetWordFrequencies(id))
        {
            words.emplace(element.first);
        }
        //Дальше проверяем на дубликат через count
        //как только находим дубликат, вызываем RemoveDocument для этого ИД и тупо не добавляем его в id_to_words + оформить std::cout
        if (set_words.count(words))
        {
            ids_to_delete.emplace(id);
        }
        else
        {
            set_words.emplace(words);
        }
    }

    for (int id : ids_to_delete)
    {
        std::cout << "Found duplicate document id " << id << std::endl;
        search_server.RemoveDocument(std::execution::par, id);
    }
}