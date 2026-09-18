#include "aiws/retrieval_engine.hpp"

#include "aiws/text_processor.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_set>

namespace aiws {

double RetrievalEngine::canonical_score(double value) {
    return std::round(value * 1.0e12) / 1.0e12;
}

std::vector<SearchResult> RetrievalEngine::search(
    const std::string& query, int k, const std::vector<Chunk>& chunks,
    const CorpusIndex& index) const {
    if (k < 0) throw std::invalid_argument("negative result count");
    if (k == 0) return {};
    const auto query_terms = TextProcessor::terms(query);
    std::vector<std::string> unique_terms;
    std::unordered_set<std::string> seen;
    for (const auto& term : query_terms) {
        if (seen.insert(term).second) unique_terms.push_back(term);
    }
    if (unique_terms.empty()) return {};

    struct Scored { std::size_t chunk_index; double score; std::size_t matched; };
    std::vector<Scored> scored;
    const double chunk_count = static_cast<double>(chunks.size());
    for (std::size_t chunk_index = 0; chunk_index < chunks.size(); ++chunk_index) {
        double base = 0.0;
        std::size_t matched = 0;
        for (const auto& term : unique_terms) {
            const std::size_t frequency = index.term_frequency(term, chunks[chunk_index].id);
            if (frequency == 0) continue;
            ++matched;
            const double tf = 1.0 + std::log(static_cast<double>(frequency));
            const double idf = std::log((chunk_count + 1.0) /
                                        (index.document_frequency(term) + 1.0)) + 1.0;
            base += tf * idf;
        }
        if (matched != 0) {
            const double coverage = 1.0 + 0.10 * static_cast<double>(matched) /
                                              static_cast<double>(unique_terms.size());
            scored.push_back({chunk_index, canonical_score(base * coverage), matched});
        }
    }
    std::sort(scored.begin(), scored.end(), [&chunks](const Scored& left, const Scored& right) {
        if (left.score != right.score) return left.score > right.score;
        const Chunk& a = chunks[left.chunk_index];
        const Chunk& b = chunks[right.chunk_index];
        if (a.document_order != b.document_order) return a.document_order < b.document_order;
        return a.sequence < b.sequence;
    });
    if (scored.size() > static_cast<std::size_t>(k)) scored.resize(static_cast<std::size_t>(k));

    std::vector<SearchResult> result;
    for (const auto& item : scored) {
        const auto& chunk = chunks[item.chunk_index];
        result.push_back({chunk.id, chunk.document_id, chunk.sequence, chunk.text,
                          item.score, item.matched});
    }
    return result;
}

}  // namespace aiws

