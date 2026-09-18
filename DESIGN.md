# M1 Design

## 1. System Structure

The program turns workspace documents into searchable text. The main steps are:

1. `TextProcessor` cleans the text and separates it into lowercase words and
	numbers. It uses the same rules for documents and search queries.
2. `Chunker` divides each document into smaller pieces. Each piece has at most
	120 normalized tokens, and neighboring pieces share 20 tokens. Each chunk
	remembers which document and part of the document it came from.
3. `CorpusIndex` creates a lookup table for the chunks. For every word, it
	records which chunks contain the word and how many times it appears there.
4. `RetrievalEngine` uses that lookup table to find and rank chunks for a
	query. Chunks with more important matching words receive higher scores.
5. `ContextBuilder` takes the ranked chunks and places as much of them as
	possible into the caller's token budget.

`ProcessingCore` is the coordinator. Its `rebuild` function runs the first
three steps, while `search` and `build_context` run the later steps.

## 2. Design Decisions

The processing core owns a list of all current chunks and one index for those
chunks. The original `Document` objects remain in the `Workspace`; processing
creates new data without changing those documents.

Each `Chunk` stores its ID, document ID, position in the document, normalized
text, token count, and original character range. For example, the first chunk
from document `notes` has the ID `notes#0`.

The index stores a small record for each word and chunk. This record contains
the chunk's position in the chunk list and the word's frequency. It also keeps
a map from chunk IDs to their positions, which makes chunk lookup quick without
storing another copy of every chunk.

The core hides these implementation details behind a private internal object.
This keeps the public header simple and allows the internal containers to
change without changing the public interface.

## 3. Correctness and consistency

Documents and queries must use exactly the same text rules. ASCII letters are
lowercase, digits are kept, and punctuation becomes a separator. Empty input
must not create fake words.

The original document text is never modified. Chunk text is normalized, but
each chunk's `source_begin` and `source_end` still refer to positions in the
original text. Paragraph boundaries are used when choosing chunk endings.

Document IDs must be unique inside one workspace. A rebuild first creates a
new chunk list and a new index in temporary variables. It replaces the old
search data only after the new data is complete. Therefore, if duplicate IDs
cause an error, the previous valid search data is preserved instead of being
left half-rebuilt.

Search results are rounded to twelve decimal places before they are compared.
If two results have the same score, document order and then chunk order decide
which one comes first. This makes the output repeatable.

## 4. Testing strategy

The supplied public tests check the most important normal cases and boundaries:

- lowercase normalization and punctuation-only input;
- multiple documents and word frequencies;
- ranking results for a query;
- context truncation at a token budget;
- 120-token chunk limits and 20-token overlap;
- rejection of a negative result count.

The self-written `tests/m1_student_tests.cpp` adds direct tests for five
component boundaries: text processing, chunking, indexing, retrieval, and
context construction. It also tests paragraph boundaries, Windows-style CRLF
line endings, empty documents, unknown query terms, repeated query terms, zero
budgets, duplicate document IDs, repeated rebuilds, deterministic ranking
ties, and a multi-document end-to-end workflow.

## 5. Alternatives considered

One possible design was to scan every chunk from beginning to end for every
query. I chose an index instead because it stores word locations and
frequencies once during rebuilding. Searches can then focus on chunks that
actually contain query words.

Another possible design was to store only normalized chunk text and calculate
source positions later. I chose to save token positions during text processing
because this preserves the exact location in the original document and makes
paragraph-aware chunking easier.

A final alternative was to update the live index as each document was rebuilt.
I chose to build temporary data first. This prevents a failed rebuild from
destroying a previously working corpus.
