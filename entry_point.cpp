#include "flat_unordered_hash_map.hpp"

#include <iostream>

int main(int argc, char** argv)
{
    // #TODO better example(s) of usage

    Kablunk::util::container::flat_unordered_hash_map<std::string, int> map;

    map.insert({ "hello", 1 });
    map.insert({ "world", 2 });

    for (auto& [key, value] : map)
        std::cout << key << " " << value << " ";
    std::cout << std::endl;

    return 0;
}