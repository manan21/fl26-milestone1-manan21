#include "aiws/chunker.hpp"
#include "aiws/context_builder.hpp"
#include "aiws/corpus_index.hpp"
#include "aiws/processing_core.hpp"
#include "aiws/retrieval_engine.hpp"
#include "aiws/text_processor.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

std::string numbered_words(int count) {
    std::string result;
    for (int index = 0; index < count; ++index) {
        if (!result.empty()) result += ' ';
        result += "word" + std::to_string(index);
    }
    return result;
}

void test_text_processing() {
    using aiws::TextProcessor;

    check(TextProcessor::normalize("HeLLo, R2-D2! 42") == "hello r2 d2 42",
          "text processor applies ASCII normalization rules");
    check(TextProcessor::normalize("\xFF\t---").empty(),
          "non-ASCII bytes and punctuation are separators");

    const std::string text = "First paragraph.\r\n\r\nSecond\tparagraph.";
    const auto tokens = TextProcessor::tokenize(text);
    check(tokens.size() == 4, "tokenizer returns every normalized token");
    check(tokens[0].token == "first" && tokens[0].begin == 0 && tokens[0].end == 5,
          "tokenizer records original source span");
    check(tokens[2].paragraph == 1 && tokens[3].paragraph == 1,
          "tokenizer recognizes CRLF paragraph boundaries");
    check(TextProcessor::join(tokens, 1, 3) == "paragraph second",
          "token join preserves normalized token order");
}

void test_chunking() {
    using aiws::Chunker;
    using aiws::Document;

    const std::string text = numbered_words(100) + "\n\n" + numbered_words(30);
    const Document document{"doc", "Title", text};
    const auto chunks = Chunker{}.chunk(document, 4);
    check(chunks.size() == 2, "chunker creates two chunks for 130 tokens");
    check(chunks[0].token_count == 100,
          "chunker prefers a paragraph boundary in the final preference window");
    check(chunks[1].token_count == 50,
          "chunker uses overlap after a paragraph-selected boundary");
    check(chunks[0].id == "doc#0" && chunks[1].id == "doc#1",
          "chunk IDs contain document ID and sequence");
    check(chunks[0].document_order == 4 && chunks[0].source_begin == 0,
          "chunks retain document order and source start");
      check(chunks[1].source_begin < chunks[0].source_end &&
                    chunks[1].source_end <= document.text().size(),
              "overlapping chunk spans remain tied to original document text");

    const auto hard_limit = Chunker{}.chunk(Document{"long", "", numbered_words(121)}, 0);
    check(hard_limit.size() == 2 && hard_limit[0].token_count == 120 &&
              hard_limit[1].token_count == 21,
          "chunker applies maximum size and fixed overlap");
    check(Chunker{}.chunk(Document{"empty", "", "!!!"}, 0).empty(),
          "chunker omits effectively empty documents");
}

void test_corpus_index() {
    using aiws::Chunker;
    using aiws::CorpusIndex;
    using aiws::Document;

    const auto chunks = Chunker{}.chunk(Document{"doc", "", "alpha alpha beta"}, 0);
    const CorpusIndex index{chunks};
    check(index.document_frequency("alpha") == 1,
          "index document frequency counts chunks, not occurrences");
    check(index.term_frequency("alpha", "doc#0") == 2,
          "index stores term frequency for a chunk");
    check(index.term_frequency("missing", "doc#0") == 0 &&
              index.term_frequency("alpha", "missing#0") == 0,
          "index returns zero for unknown terms and chunks");
    check(index.postings("alpha") != nullptr && index.postings("alpha")->size() == 1,
          "index exposes postings for an indexed term");
    check(index.find_chunk(chunks, "doc#0") == &chunks[0] && index.chunk_index("doc#0") == 0,
          "index resolves chunk IDs to stored chunks");
}

void test_retrieval_and_context() {
    using aiws::Chunker;
    using aiws::ContextBuilder;
    using aiws::CorpusIndex;
    using aiws::Document;
    using aiws::RetrievalEngine;
    using aiws::SearchResult;

    std::vector<aiws::Chunk> chunks;
    const auto first = Chunker{}.chunk(Document{"first", "", "common rare"}, 0);
    const auto second = Chunker{}.chunk(Document{"second", "", "common common"}, 1);
    chunks.insert(chunks.end(), first.begin(), first.end());
    chunks.insert(chunks.end(), second.begin(), second.end());
    const CorpusIndex index{chunks};

    const auto ranked = RetrievalEngine{}.search("rare common rare", 10, chunks, index);
    check(ranked.size() == 2 && ranked[0].document_id == "first",
          "retrieval deduplicates query terms and ranks coverage");
    check(ranked[0].matched_terms == 2 && ranked[1].matched_terms == 1,
          "retrieval reports distinct matched query terms");
    check(RetrievalEngine{}.search("unknown", 10, chunks, index).empty(),
          "unknown query terms produce no candidates");

    const std::vector<SearchResult> manual{
        {"a#0", "a", 0, "one two three", 3.0, 1},
        {"a#0", "a", 0, "one two three", 3.0, 1},
        {"b#0", "b", 0, "four five", 2.0, 1}};
    const auto context = ContextBuilder{}.build(manual, 4);
    check(context.size() == 2 && context[0].chunk_id == "a#0" &&
              context[1].chunk_id == "b#0",
          "context skips duplicate chunks and preserves ranking order");
    check(context[1].truncated && context[1].token_count == 1 &&
              context[1].text == "four",
          "context truncates the final chunk to the remaining budget");
    check(ContextBuilder{}.build(ranked, 0).empty(),
          "zero context budget returns no items");
}

void test_rebuild_and_end_to_end() {
    using aiws::Document;
    using aiws::ProcessingCore;
    using aiws::Workspace;

    Workspace workspace;
    workspace.add_document(Document{"guide", "Guide", "install compiler"});
    workspace.add_document(Document{"api", "API", "compiler API reference"});
    ProcessingCore core;
    core.rebuild(workspace);
    const auto before = core.chunks();
    const auto results = core.search("compiler", 10);
    check(results.size() == 2 && results[0].document_id == "guide" &&
              results[1].document_id == "api",
          "multi-document end-to-end search preserves insertion order on ties");
    check(core.build_context("compiler", 10, 2).size() == 1,
          "end-to-end context honors its token budget");

    Workspace invalid;
    invalid.add_document(Document{"new", "New", "replacement"});
    invalid.add_document(Document{"new", "Duplicate", "bad"});
    bool threw = false;
    try {
        core.rebuild(invalid);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "rebuild rejects duplicate document IDs");
    check(core.chunks().size() == before.size() &&
              core.term_frequency("compiler", "guide#0") == 1,
          "failed rebuild preserves the previous valid corpus");

    bool multi_term_threw = false;
    try {
        (void)core.term_frequency("compiler API", "guide#0");
    } catch (const std::invalid_argument&) {
        multi_term_threw = true;
    }
    check(multi_term_threw, "term frequency rejects multi-token terms");
}
}

int main() {
    test_text_processing();
    test_chunking();
    test_corpus_index();
    test_retrieval_and_context();
    test_rebuild_and_end_to_end();

    if (failures == 0) {
        std::cout << "All student M1 tests passed.\n";
        return 0;
    }
    std::cerr << failures << " student M1 test(s) failed.\n";
    return 1;
}
