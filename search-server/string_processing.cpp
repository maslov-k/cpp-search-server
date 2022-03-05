#include "string_processing.h"

using namespace std;

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

vector<string_view> SplitIntoWordsView(string_view text)
{
    vector<string_view> result;
    const int64_t pos_end = text.npos;
    while (true)
    {
        int64_t space = text.find(' ', 0);
        string_view word = space == pos_end ? text.substr(0) : text.substr(0, space);
        if (!word.empty())
        {
            result.push_back(word);
        }
        if (space == pos_end)
        {
            break;
        }
        text.remove_prefix(space + 1);
    }
    return result;
}
