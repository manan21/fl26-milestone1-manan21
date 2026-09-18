#include "aiws/chunker.hpp"
#include "aiws/text_processor.hpp"

#include <stdexcept>

namespace aiws {

Chunker::Chunker(ChunkingPolicy policy) : policy_(policy) {
    if (policy_.max_tokens == 0 || policy_.overlap >= policy_.max_tokens ||
        policy_.paragraph_window > policy_.max_tokens) {
        throw std::invalid_argument("invalid chunking policy");
    }
}

std::vector<Chunk> Chunker::chunk(const Document& document,
                                  std::size_t document_order) const {
    const auto tokens = TextProcessor::tokenize(document.text());
    std::vector<Chunk> result;
    if (tokens.empty()) return result;

    std::size_t begin = 0;
    std::size_t sequence = 0;
    while (begin < tokens.size()) {
        const std::size_t remaining = tokens.size() - begin;
        std::size_t end = tokens.size();
        if (remaining > policy_.max_tokens) {
            const std::size_t hard_end = begin + policy_.max_tokens;
            end = hard_end;
            const std::size_t preferred_begin = begin +
                policy_.max_tokens - policy_.paragraph_window;
            for (std::size_t candidate = preferred_begin; candidate <= hard_end; ++candidate) {
                if (candidate > begin && candidate < tokens.size() &&
                    tokens[candidate - 1].paragraph != tokens[candidate].paragraph) {
                    end = candidate;
                }
            }
        }

        Chunk chunk;
        chunk.document_id = document.id();
        chunk.document_order = document_order;
        chunk.sequence = sequence;
        chunk.id = document.id() + "#" + std::to_string(sequence);
        chunk.text = TextProcessor::join(tokens, begin, end);
        chunk.token_count = end - begin;
        chunk.source_begin = tokens[begin].begin;
        chunk.source_end = tokens[end - 1].end;
        result.push_back(std::move(chunk));
        ++sequence;
        if (end == tokens.size()) break;
        begin = end - policy_.overlap;
    }
    return result;
}

}  // namespace aiws
