#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <sstream>
#include <vector>
#include <array>
#include <memory>

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


// Credit to stackoverflow user waqas. Code can be found at 
// https://www.stackoverflow.com/questions/478898/how-to-execute-a-command-and-get-output-of-command-within-c-using-posix
std::string exec(const std::string &command) {
    std::array<char, 128> buffer;
    std::string result;

    std::shared_ptr<FILE> pipe(popen(command.c_str(), "r"), pclose);

    if (!pipe) {
        throw std::runtime_error("popen(" + command + ", \"r\") failed.");
    }

    while (!feof(pipe.get())) {
        if (fgets(buffer.data(), 128, pipe.get()) != nullptr) {
                result += buffer.data();
        }
    }

    return result;
}

#endif
