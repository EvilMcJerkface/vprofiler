#include <string>
#include <sstream>
#include <vector>

// Credit to Evan Teran on stackoverflow for this solution. Find it at
// https://www.stackoverflow.com/questions/236129/split-a-string-in-c.
template<typename Out>
void SplitString(const std::string &s, char delim, Out result) {
    std::stringstream ss;
    ss.str(s);
    std::string item;

    while (std::getline(ss, item, delim)) {
        if (!item.empty()) {
            *(result++) = item;
        }
    }
}

// Credit to Evan Teran on stackoverflow for this solution. Find it at
// https://www.stackoverflow.com/questions/236129/split-a-string-in-c.
std::vector<std::string> SplitString(const std::string &s, char delim) {
    std::vector<std::string> elems;
    SplitString(s, delim, std::back_inserter(elems));
    
    return elems;
}
