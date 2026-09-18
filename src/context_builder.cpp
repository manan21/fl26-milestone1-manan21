#include "aiws/context_builder.hpp"
#include "aiws/text_processor.hpp"

#include <unordered_set>

namespace aiws {

std::vector<ContextItem> ContextBuilder::build(
    const std::vector<SearchResult>& ranked, std::size_t token_budget) const {
    std::vector<ContextItem> result;
    std::unordered_set<std::string> included;
    std::size_t used = 0;
    for (const auto& item : ranked) {
        if (used == token_budget) break;
        if (!included.insert(item.chunk_id).second) continue;
        const auto terms = TextProcessor::terms(item.text);
        if (terms.empty()) continue;
        const std::size_t available = token_budget - used;
        if (terms.size() <= available) {
            result.push_back({item.chunk_id, item.document_id, item.chunk_sequence,
                              item.text, terms.size(), item.score, false});
            used += terms.size();
        } else if (available != 0) {
            result.push_back({item.chunk_id, item.document_id, item.chunk_sequence,
                              TextProcessor::join(terms, 0, available), available,
                              item.score, true});
            break;
        }
    }
    return result;
}

}  // namespace aiws
