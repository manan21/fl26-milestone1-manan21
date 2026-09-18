#include "aiws/corpus_index.hpp"

#include "aiws/text_processor.hpp"

#include <stdexcept>

namespace aiws {

CorpusIndex::CorpusIndex(const std::vector<Chunk>& chunks) {
    build(chunks);
}

void CorpusIndex::build(const std::vector<Chunk>& chunks) {
    postings_.clear();
    chunk_by_id_.clear();
    for (std::size_t index = 0; index < chunks.size(); ++index) {
        if (!chunk_by_id_.emplace(chunks[index].id, index).second) {
            throw std::invalid_argument("duplicate chunk ID");
        }
        const auto terms = TextProcessor::terms(chunks[index].text);
        std::unordered_map<std::string, std::size_t> frequencies;
        for (const auto& term : terms) ++frequencies[term];
        for (const auto& entry : frequencies) {
            postings_[entry.first].push_back({index, entry.second});
        }
    }
}

std::size_t CorpusIndex::document_frequency(
    const std::string& normalized_term) const noexcept {
    const auto found = postings_.find(normalized_term);
    return found == postings_.end() ? 0 : found->second.size();
}

std::size_t CorpusIndex::term_frequency(
    const std::string& normalized_term,
    const std::string& chunk_id) const noexcept {
    const auto found = postings_.find(normalized_term);
    if (found == postings_.end()) return 0;
    const auto chunk = chunk_by_id_.find(chunk_id);
    if (chunk == chunk_by_id_.end()) return 0;
    for (const auto& posting : found->second) {
        if (posting.chunk_index == chunk->second) return posting.frequency;
    }
    return 0;
}

const std::vector<CorpusIndex::Posting>* CorpusIndex::postings(
    const std::string& normalized_term) const noexcept {
    const auto found = postings_.find(normalized_term);
    return found == postings_.end() ? nullptr : &found->second;
}

const Chunk* CorpusIndex::find_chunk(
    const std::vector<Chunk>& chunks,
    const std::string& chunk_id) const noexcept {
    const auto found = chunk_by_id_.find(chunk_id);
    if (found == chunk_by_id_.end() || found->second >= chunks.size()) return nullptr;
    return &chunks[found->second];
}

std::size_t CorpusIndex::chunk_index(const std::string& chunk_id) const {
    const auto found = chunk_by_id_.find(chunk_id);
    if (found == chunk_by_id_.end()) throw std::out_of_range("unknown chunk ID");
    return found->second;
}

}  // namespace aiws
