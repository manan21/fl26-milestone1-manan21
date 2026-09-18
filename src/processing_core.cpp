#include "aiws/processing_core.hpp"
#include "aiws/chunker.hpp"
#include "aiws/context_builder.hpp"
#include "aiws/corpus_index.hpp"
#include "aiws/retrieval_engine.hpp"
#include "aiws/text_processor.hpp"

#include <stdexcept>
#include <unordered_set>

namespace aiws {

struct ProcessingCore::Impl {
    std::vector<Chunk> chunks;
    CorpusIndex index;
};

ProcessingCore::ProcessingCore() : impl_(std::make_unique<Impl>()) { }

ProcessingCore::~ProcessingCore() = default;

ProcessingCore::ProcessingCore(ProcessingCore&&) noexcept = default;

ProcessingCore& ProcessingCore::operator=(ProcessingCore&&) noexcept = default;

std::string ProcessingCore::normalize(const std::string& text) {
    return TextProcessor::normalize(text);
}

void ProcessingCore::rebuild(const Workspace& workspace) {
    std::vector<Chunk> next_chunks;
    std::unordered_set<std::string> document_ids;
    Chunker chunker({kMaxChunkTokens, kChunkOverlap, kParagraphPreferenceWindow});
    for (std::size_t order = 0; order < workspace.documents().size(); ++order) {
        const auto& document = workspace.documents()[order];
        if (!document_ids.insert(document.id()).second) {
            throw std::invalid_argument("duplicate document ID");
        }
        auto document_chunks = chunker.chunk(document, order);
        next_chunks.insert(next_chunks.end(), document_chunks.begin(), document_chunks.end());
    }
    CorpusIndex next_index(next_chunks);
    impl_->chunks = std::move(next_chunks);
    impl_->index = std::move(next_index);
}

const std::vector<Chunk>& ProcessingCore::chunks() const noexcept {
    return impl_->chunks;
}

std::size_t ProcessingCore::chunk_count() const noexcept {
    return impl_->chunks.size();
}

std::size_t ProcessingCore::document_frequency(const std::string& term) const {
    const auto terms = TextProcessor::terms(term);
    if (terms.size() > 1) throw std::invalid_argument("term contains multiple tokens");
    return terms.empty() ? 0 : impl_->index.document_frequency(terms.front());
}

std::size_t ProcessingCore::term_frequency(const std::string& term,
                                           const std::string& chunk_id) const {
    const auto terms = TextProcessor::terms(term);
    if (terms.size() > 1) throw std::invalid_argument("term contains multiple tokens");
    return terms.empty() ? 0 : impl_->index.term_frequency(terms.front(), chunk_id);
}

std::vector<SearchResult> ProcessingCore::search(const std::string& query, int k) const {
    return RetrievalEngine{}.search(query, k, impl_->chunks, impl_->index);
}

std::vector<ContextItem> ProcessingCore::build_context(const std::string& query,
                                                       int k,
                                                       std::size_t token_budget) const {
    return ContextBuilder{}.build(search(query, k), token_budget);
}

}  // namespace aiws
