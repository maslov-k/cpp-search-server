#pragma once

#include <string>
#include <string_view>
#include <vector>

std::vector<std::string> SplitIntoWords(const std::string& text);

std::vector<std::string_view> SplitIntoWordsView(std::string_view text);
