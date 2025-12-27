# Optimization Opportunities by Source File

Date: 2025-12-27
Based on profiling of Parser-100rules benchmark (177 samples, 18.1 µs)

## Overall Runtime Breakdown

**RuleReader-100rules total**: 59.23 µs (100%)
- Lexer: 28.46 µs (48%)
- Parser: 18.10 µs (31%)
- Other RuleReader overhead: 12.67 µs (21%)

**Parser-100rules breakdown** (177 samples):
- Parsing control flow: 55 samples (31.1%)
- Memory management: 38 samples (21.5%)
  - Function::Args vectors: 26 samples (14.7%)
  - Hash table allocations: 14 samples (7.9%)
  - Hash table hashing/lookup: 15 samples (8.5%)
- Rule cleanup (destructors): 22 samples (12.4%)
- Token operations: 15 samples (8.5%)
- String copying: 12 samples (6.8%)
- Other overhead: 20 samples (11.3%)

---

## src/parser.cpp

**Current time spent**: ~18.1 µs (31% of RuleReader total)

### Memory Management Optimizations

1. **Function::Args vector allocation/deallocation**
   - **Current time**: 26 samples (14.7% of parser time, ~2.7 µs)
   - **Issue**: parseFuncCall() allocates std::vector<string_view> for each function call
   - **Lines affected**: 273-300 (parseFuncCall)
   - **Details**: Each function creates temporary vector, moves it into Function constructor, then into FuncNode
   - **Potential approaches**:
     - Small-buffer optimization (store Args inline up to N args)
     - Store token indices instead of copying string_views
     - Use a single reusable Args buffer per parser instance
     - Use std::array for common case (most functions have 1-3 args)

2. **MetaSection hash table allocations**
   - **Current time**: 14 samples hash allocations (7.9%), 8 samples hashing (4.5%), total ~11.4%
   - **Issue**: std::unordered_map rehashes as meta fields are added
   - **Lines affected**: 344-354 (parseMetaSection)
   - **Details**: Most rules have 1-5 meta fields, but map starts with default capacity
   - **Potential approaches**:
     - Reserve initial capacity based on typical rule metadata
     - Use flat_map or sorted vector instead
     - Use std::array + linear search for small counts

3. **PatternSection hash table allocations**
   - **Current time**: 6 samples (3.4% of parser time)
   - **Issue**: std::unordered_map rehashes as patterns are added
   - **Lines affected**: 182-192 (parsePatternsSection)
   - **Details**: Most rules have 1-10 patterns
   - **Potential approaches**:
     - Reserve initial capacity (e.g., 8 patterns)
     - Use flat_map or sorted vector
     - Consider whether hash table is needed vs linear search

4. **Rule::~Rule() cleanup overhead**
   - **Current time**: 22 samples (12.4% of parser time, ~2.2 µs)
   - **Issue**: Destructor frees MetaSection and PatternSection hash map nodes
   - **Details**: Hash table node deallocation is expensive
   - **Potential approaches**:
     - Arena-allocate hash table nodes
     - Use simpler data structures (vectors) that don't have per-node overhead
     - Batch-delete rules instead of individual destructors

### Token Processing Optimizations

5. **mustParse token validation**
   - **Current time**: 12 samples (6.8% of parser time, ~1.2 µs)
   - **Issue**: Multiple token type checks and error message construction
   - **Lines affected**: Throughout (called from expect(), parsing functions)
   - **Details**: Frequent calls with multiple variadic token type arguments
   - **Potential approaches**:
     - Inline hot paths
     - Reduce error message construction overhead
     - Use bit flags for token type sets instead of variadic templates

6. **expect() function overhead**
   - **Current time**: Included in mustParse overhead (~6.8%)
   - **Issue**: Large switch statement with error message strings
   - **Lines affected**: 48-72 (expect)
   - **Details**: Called frequently, constructs error messages even when not needed
   - **Potential approaches**:
     - Split into hot path (no error) and cold path (error construction)
     - Use computed goto or jump table
     - Inline common cases

7. **checkFunctionName string comparisons**
   - **Current time**: 2 samples (1.1% of parser time)
   - **Issue**: String comparisons against function name constants
   - **Lines affected**: parser.h (checkFunctionName implementation)
   - **Details**: Compares against "any", "all", "count", "offset" on every check
   - **Potential approaches**:
     - Use FNV-1a or similar hash for O(1) lookup (tried before, may be slower for small set)
     - Use trie or first-character dispatch
     - Sort function names and binary search

### Expression Parsing Optimizations

8. **parseExpr/parseTerm/parseFactor recursion**
   - **Current time**: 33 samples (18.6% of parser time, ~3.4 µs)
   - **Issue**: Recursive descent with function call overhead
   - **Lines affected**: 226-271 (parseFactor, parseTerm, parseExpr)
   - **Details**: Core parsing logic, many function calls for nested expressions
   - **Potential approaches**:
     - Iterative parsing with explicit stack
     - Inline parseFactor into parseTerm for simple cases
     - Reduce BoolNode allocations (already using arena)

9. **parseProperty hash table lookups**
   - **Current time**: Included in hash overhead (8.5%)
   - **Issue**: Multiple hash lookups in SectionDefs and Props maps
   - **Lines affected**: 313-342 (parseProperty)
   - **Details**: Lookup section info, then property info, every property parse
   - **Potential approaches**:
     - Cache section lookup results
     - Use perfect hash for known properties
     - Flatten section/property lookup into single map

### Pattern Parsing Optimizations

10. **parseHexString character validation loop**
    - **Current time**: Unknown, not prominent in profile
    - **Issue**: Character-by-character hex digit validation
    - **Lines affected**: 194-224 (parseHexString)
    - **Details**: Loop with isxdigit() calls and string concatenation
    - **Potential approaches**:
      - Validate entire span at once
      - Use lookup table instead of isxdigit()
      - Reserve hex string capacity

11. **parsePatternDef string copying**
    - **Current time**: Included in string copying overhead (6.8%)
    - **Issue**: Creates std::string copy of pattern
    - **Lines affected**: 133-153 (parsePatternMod), 169-180 (parsePatternDef)
    - **Details**: Pattern strings are copied into PatternDef
    - **Potential approaches**:
      - Use string_view throughout instead of std::string
      - Store token indices instead of copies

### Control Flow Optimizations

12. **parseRuleDecl section dispatch**
    - **Current time**: 5 samples overhead (2.8%)
    - **Issue**: Multiple if statements to check for optional sections
    - **Lines affected**: 356-387 (parseRuleDecl)
    - **Details**: Checks for meta, hash, file_metadata, signature, grep in sequence
    - **Potential approaches**:
      - Use switch or computed goto based on token type
      - Reorder checks based on frequency (grep is most common)
      - Combine section checks

13. **parseRules vector operations**
    - **Current time**: 3 samples (1.7%)
    - **Issue**: Rules vector growth and error handling
    - **Lines affected**: 389-422 (parseRules)
    - **Details**: Already reserves capacity, but still has overhead
    - **Potential approaches**:
      - Inline parseRuleDecl
      - Reduce exception handling overhead
      - Use output iterator instead of vector::emplace_back

### Hash Section Optimizations

14. **parseHashSection vector operations**
    - **Current time**: Unknown, small portion
    - **Issue**: FileHashRecords vector growth
    - **Lines affected**: 80-94 (parseHashSection)
    - **Details**: Most rules have 1-3 hash records
    - **Potential approaches**:
      - Reserve capacity (e.g., 3 records)
      - Use std::array for common case

15. **parseFileHashRecord duplicate checking**
    - **Current time**: Unknown, small portion
    - **Issue**: Linear search via findKey for duplicates
    - **Lines affected**: 114-126 (parseFileHashRecord)
    - **Details**: Checks for duplicate hash algorithms in same record
    - **Potential approaches**:
      - Use bitset to track seen algorithms (already using HashAlgs, could check before adding)
      - Skip duplicate check if not needed

### String/Data Copying

16. **General memmove overhead**
    - **Current time**: 8 samples (4.5% of parser time)
    - **Issue**: Various string_view and data copying operations
    - **Lines affected**: Throughout
    - **Details**: Copying string_views, moving objects
    - **Potential approaches**:
      - Reduce intermediate copies
      - Pass by reference more aggressively
      - Use move semantics more carefully

---

## src/lexer.cpp

**Current time spent**: ~28.46 µs (48% of RuleReader total)

### String Parsing Optimizations

17. **parseString escape handling**
    - **Current time**: Unknown, but parseString is called for every quoted string
    - **Issue**: Complex nested loop for escape sequence handling
    - **Lines affected**: 121-152 (parseString)
    - **Details**: Uses memchr repeatedly to find quotes and backslashes
    - **Potential approaches**:
      - SIMD string scanning for quotes
      - Single pass that handles escapes incrementally
      - Pre-scan to determine if escapes exist

18. **parseString memchr calls**
    - **Current time**: Unknown
    - **Issue**: Multiple memchr calls in loop
    - **Lines affected**: 127, 132, 143
    - **Details**: Searches for quotes and backslashes separately
    - **Potential approaches**:
      - SIMD to find both characters at once
      - Manual loop with early exit
      - Compute both positions in single pass

### Identifier Parsing Optimizations

19. **parseIdentifier character checking loop**
    - **Current time**: Unknown, called for every identifier/keyword
    - **Issue**: Loop checking IdentifierChars bitset
    - **Lines affected**: 98-119 (parseIdentifier)
    - **Details**: Character-by-character advance with bitset lookup
    - **Potential approaches**:
      - SIMD scan for non-identifier characters
      - Unroll loop
      - Use memchr variant to find span end

20. **Keyword lookup in hash map**
    - **Current time**: Unknown, but LlamaKeywords lookup happens per identifier
    - **Issue**: Hash table lookup for every identifier
    - **Lines affected**: 108 (LlamaKeywords.find)
    - **Details**: Most identifiers are not keywords
    - **Potential approaches**:
      - Perfect hash for keywords
      - Trie structure
      - First-character dispatch before full lookup
      - Check identifier length before lookup (keywords have specific lengths)

### Number Parsing Optimizations

21. **parseNumber digit checking loop**
    - **Current time**: Unknown, less frequent than identifiers
    - **Issue**: Character-by-character isdigit loop
    - **Lines affected**: 154-164 (parseNumber)
    - **Details**: Simple loop advancing while isdigit
    - **Potential approaches**:
      - SIMD to find non-digit
      - Unroll loop
      - Manual bounds checking instead of isdigit

### Token Stream Optimizations

22. **Token vector growth**
    - **Current time**: Unknown
    - **Issue**: Vector reserve estimates may be off
    - **Lines affected**: 23 (Tokens.reserve)
    - **Details**: Reserves InputSize, but actual token count is much smaller
    - **Potential approaches**:
      - Better size estimate (InputSize/6 is typical)
      - Use fixed-size allocator with arena
      - Measure actual growth patterns

23. **addToken calls**
    - **Current time**: Unknown, called for every token
    - **Issue**: Function call overhead for every token
    - **Lines affected**: Throughout (many addToken calls)
    - **Details**: Could be inlined or reduced
    - **Potential approaches**:
      - Force inline addToken
      - Batch token additions
      - Reduce Token struct size

### Comment Parsing Optimizations

24. **parseSingleLineComment loop**
    - **Current time**: Unknown, depends on comment frequency
    - **Issue**: Character-by-character scan for newline
    - **Lines affected**: 166-170 (parseSingleLineComment)
    - **Details**: Simple loop looking for '\n'
    - **Potential approaches**:
      - memchr to find newline
      - SIMD scan

25. **parseMultiLineComment recursion**
    - **Current time**: Unknown, rare
    - **Issue**: Recursive call for nested /* within comment
    - **Lines affected**: 172-191 (parseMultiLineComment)
    - **Details**: Recursion could overflow on pathological input
    - **Potential approaches**:
      - Iterative implementation
      - memchr for '*' search
      - Track nesting with counter instead of recursion

### Whitespace/Control Flow Optimizations

26. **scanToken switch statement**
    - **Current time**: Unknown, executed for every character
    - **Issue**: Large switch on character value
    - **Lines affected**: 35-96 (scanToken)
    - **Details**: Jump table may not be optimal
    - **Potential approaches**:
      - Lookup table for character classes
      - Separate hot paths (whitespace, alpha, digit) from cold
      - Use computed goto

27. **Position tracking overhead**
    - **Current time**: Unknown
    - **Issue**: LineCol updated on every newline and in comments
    - **Lines affected**: Throughout (Pos updates)
    - **Details**: Line/column tracking adds overhead
    - **Potential approaches**:
      - Lazy position computation (compute only on demand)
      - Batch position updates
      - Use byte offset only, compute line/col when needed

28. **RuleIndices vector updates**
    - **Current time**: Unknown, once per rule
    - **Issue**: push_back on RuleIndices when RULE keyword found
    - **Lines affected**: 113 (RuleIndices.push_back)
    - **Details**: Small overhead, but could reserve capacity
    - **Potential approaches**:
      - Reserve RuleIndices capacity upfront
      - Estimate rule count from input size

---

## src/querybuilder.cpp

**Current time spent**: Not profiled separately, used after parsing

### String Building Optimizations

29. **buildSqlClause string concatenation**
    - **Current time**: Unknown, but called recursively for AST traversal
    - **Issue**: Multiple string concatenations per clause
    - **Lines affected**: 11-55 (buildSqlClause functions)
    - **Details**: Creates intermediate strings, concatenates repeatedly
    - **Potential approaches**:
      - Pre-calculate total size and reserve
      - Use std::ostringstream or fmt library
      - Single-pass traversal with size calculation first
      - Write directly to output buffer

30. **buildSqlQuery string building**
    - **Current time**: Unknown
    - **Issue**: String concatenation for query construction
    - **Lines affected**: 57-68 (buildSqlQuery)
    - **Details**: Fixed prefix + hash string + conditional metadata clause
    - **Potential approaches**:
      - Reserve capacity based on rule complexity
      - Use string_view for constant parts
      - Pre-compute query template

31. **FileMetadataPropertySqlLookup hash lookups**
    - **Current time**: Unknown, once per property
    - **Issue**: Hash map lookup for property name translation
    - **Lines affected**: 32 (FileMetadataPropertySqlLookup.find)
    - **Details**: Small map (5 entries), hash overhead may exceed benefit
    - **Potential approaches**:
      - Use switch on first character + strcmp
      - Perfect hash
      - Linear search (only 5 entries)

32. **Recursive AST traversal**
    - **Current time**: Unknown
    - **Issue**: Virtual dispatch/switch for node types
    - **Lines affected**: 11-27, 48-54 (buildSqlClause overloads)
    - **Details**: Traverses tree recursively with type checks
    - **Potential approaches**:
      - Iterative traversal with explicit stack
      - Visitor pattern with less overhead
      - Flatten tree to linear representation during parsing

---

## src/rulereader.cpp

**Current time spent**: ~12.67 µs overhead (21% of RuleReader total, beyond lexer+parser)

### Memory/Copying Optimizations

33. **LlamaParser construction**
    - **Current time**: Unknown, happens once per read() call
    - **Issue**: Copies input string and token vector into parser
    - **Lines affected**: 7 (Parser = LlamaParser(...))
    - **Details**: Parser constructor copies data
    - **Potential approaches**:
      - Move tokens instead of copy
      - Use references/pointers instead of copies
      - Construct parser in-place

34. **Rules vector copying**
    - **Current time**: Unknown, happens once per read() call
    - **Issue**: parseRules returns vector, then inserted into Rules
    - **Lines affected**: 8-10
    - **Details**: Rules are copied/moved from parser result to RuleReader.Rules
    - **Potential approaches**:
      - Parse directly into RuleReader.Rules
      - Use move semantics more carefully
      - Return reference instead of value

35. **Rules.reserve() sizing**
    - **Current time**: Unknown, minor
    - **Issue**: Reserve adds old size + new size
    - **Lines affected**: 9
    - **Details**: May over-allocate if called multiple times
    - **Potential approaches**:
      - Better capacity estimation
      - Reserve once at start based on input size

36. **Error counting overhead**
    - **Current time**: Unknown, minor
    - **Issue**: Calls .size() on error vectors
    - **Lines affected**: 11
    - **Details**: Could track error count directly
    - **Potential approaches**:
      - Track error count with flag instead of size()
      - Early exit on first error if errors are rare

---

## Cross-cutting/Architectural Optimizations

These affect multiple files:

37. **Token representation size**
    - **Current time**: Affects lexer memory, parser access patterns
    - **Issue**: Token struct may be larger than necessary
    - **Details**: Contains Type, Start, End, Pos (line/col), Lexeme
    - **Potential approaches**:
      - Reduce Token size (better cache locality)
      - Separate hot fields (Type, Start, End) from cold (Pos)
      - Use token index everywhere instead of passing Tokens around

38. **String_view copies throughout**
    - **Current time**: Part of 6.8% string copying overhead
    - **Issue**: string_view (16 bytes) copied frequently
    - **Details**: Passed by value in many places
    - **Potential approaches**:
      - Pass by const reference
      - Use token indices instead
      - Inline more aggressively to eliminate passing

39. **Error handling overhead**
    - **Current time**: Part of parser overhead
    - **Issue**: Exception throwing/catching in hot paths
    - **Details**: try/catch in parseRules loop, throws in many parsing functions
    - **Potential approaches**:
      - Return error codes instead of exceptions
      - Use expected<T, Error> pattern
      - Separate validation pass before parsing

40. **Allocator strategy**
    - **Current time**: Affects all allocation overhead
    - **Issue**: Uses default allocator throughout
    - **Details**: Each allocation goes to malloc/free
    - **Potential approaches**:
      - Custom allocator for parser (already have arenas for nodes)
      - Pool allocator for fixed-size objects
      - Stack-based temporary allocations

41. **Memory layout/cache effects**
    - **Current time**: Affects overall performance
    - **Issue**: Struct layouts may not be cache-friendly
    - **Details**: Frequent access to Token array, Rule structs
    - **Potential approaches**:
      - Reorder struct fields for better packing
      - Separate hot/cold fields
      - Array-of-structs vs struct-of-arrays analysis

42. **Input string ownership**
    - **Current time**: Unknown, happens at setup
    - **Issue**: Input string copied into parser and lexer
    - **Details**: Large input strings duplicated
    - **Potential approaches**:
      - Use string_view throughout
      - Single shared ownership
      - Memory-map large inputs

---

## Summary of Highest Impact Opportunities

Based on profiling data, these are likely highest impact:

1. **Function::Args vectors** (14.7%, ~2.7 µs) - src/parser.cpp:273-300
2. **Hash table operations total** (16.4%, ~3.0 µs) - src/parser.cpp (MetaSection, PatternSection)
3. **Rule cleanup** (12.4%, ~2.2 µs) - Destructor overhead
4. **Expression parsing** (18.6%, ~3.4 µs) - src/parser.cpp:226-271
5. **Lexer overall** (48%, ~28.5 µs) - src/lexer.cpp (all operations)
6. **Token operations** (8.5%, ~1.5 µs) - src/parser.cpp (mustParse, expect)
7. **String copying** (6.8%, ~1.2 µs) - Throughout

The lexer accounts for nearly half the total runtime, making it the highest-value optimization target overall.
