#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <sstream>
#include <vector>
#include <array>
#include <memory>

template<typename Out>
void SplitString(const std::string &s, char delim, Out result);

std::vector<std::string> SplitString(const std::string &s, char delim); 

std::string execute(const std::string &command); 

#endif
