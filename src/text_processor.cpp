#include "aiws/text_processor.hpp"

#include <algorithm>
#include <utility>

namespace aiws {

namespace {
bool is_ascii_alnum(unsigned char value) {
    return (value >= 'a' && value <= 'z') ||
           (value >= 'A' && value <= 'Z') ||
           (value >= '0' && value <= '9');
}

char lowercase_ascii(unsigned char value) {
    if (value >= 'A' && value <= 'Z') {
        return static_cast<char>(value - 'A' + 'a');
    }
    return static_cast<char>(value);
}

bool is_newline(const std::string& text, std::size_t position) {
    return text[position] == '\n' ||
           (text[position] == '\r' && position + 1 < text.size() &&
            text[position + 1] == '\n');
}

bool has_blank_line(const std::string& text, std::size_t begin,
                   std::size_t end) {
    std::size_t newline_count = 0;
    for (std::size_t position = begin; position < end;) {
        if (is_newline(text, position)) {
            ++newline_count;
            if (newline_count >= 2) return true;
            position += text[position] == '\r' ? 2 : 1;
        } else {
            if (text[position] != ' ' && text[position] != '\t' &&
                text[position] != '\r') {
                newline_count = 0;
            }
            ++position;
        }
    }
    return false;
}
}

std::vector<TokenInfo> TextProcessor::tokenize(const std::string& text) {
    std::vector<TokenInfo> result;
    std::size_t position = 0;
    std::size_t paragraph = 0;
    std::size_t previous_end = 0;
    bool have_previous = false;
    while (position < text.size()) {
        if (!is_ascii_alnum(static_cast<unsigned char>(text[position]))) {
            ++position;
            continue;
        }
        if (have_previous && has_blank_line(text, previous_end, position)) {
            ++paragraph;
        }
        const std::size_t begin = position;
        std::string token;
        while (position < text.size() &&
               is_ascii_alnum(static_cast<unsigned char>(text[position]))) {
            token += lowercase_ascii(static_cast<unsigned char>(text[position]));
            ++position;
        }
        result.push_back({std::move(token), begin, position, paragraph});
        previous_end = position;
        have_previous = true;
    }
    return result;
}

std::vector<std::string> TextProcessor::terms(const std::string& text) {
    std::vector<std::string> result;
    for (const auto& token : tokenize(text)) result.push_back(token.token);
    return result;
}

std::string TextProcessor::normalize(const std::string& text) {
    const auto tokens = tokenize(text);
    return join(tokens, 0, tokens.size());
}

std::string TextProcessor::join(const std::vector<TokenInfo>& tokens,
                                std::size_t begin, std::size_t end) {
    begin = std::min(begin, tokens.size());
    end = std::min(end, tokens.size());
    if (begin >= end) return {};
    std::string result;
    for (std::size_t index = begin; index < end; ++index) {
        if (!result.empty()) result += ' ';
        result += tokens[index].token;
    }
    return result;
}

std::string TextProcessor::join(const std::vector<std::string>& tokens,
                                std::size_t begin, std::size_t end) {
    begin = std::min(begin, tokens.size());
    end = std::min(end, tokens.size());
    if (begin >= end) return {};
    std::string result;
    for (std::size_t index = begin; index < end; ++index) {
        if (!result.empty()) result += ' ';
        result += tokens[index];
    }
    return result;
}

}  // namespace aiws
