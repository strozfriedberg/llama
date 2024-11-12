#pragma once

#include "token.h"

#include <hasher/common.h>
#include <lightgrep/api.h>
#include <lightgrep/util.h>

#include <boost/functional/hash.hpp>

#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#include <fieldhasher.h>

class ParserError : public UnexpectedInputError {
public:
  ParserError(const std::string_view& message, LineCol pos) : UnexpectedInputError(message, pos) {}
};

class LlamaParser;

struct Atom {};

enum class NodeType {
  AND, OR, FUNC, SIG, META, PROP
};

// Holds information about expressions under the `file_metadata`, `signature`, and `condition`
// sections.
struct Node {
  virtual ~Node() = default;
  virtual std::string getSqlQuery(const LlamaParser& parser) const = 0;

  Node(NodeType type) : Type(type) {}
  Node() = default;

  NodeType Type;
  std::shared_ptr<Node> Left;
  std::shared_ptr<Node> Right;
};

// Reserved for AND and OR nodes.
struct BoolNode : public Node {
  std::string getSqlQuery(const LlamaParser& parser) const override;
};

/************************************ FILE_METADATA SECTION ***************************************/

const static std::unordered_map<std::string_view, std::string> FileMetadataPropertySqlLookup {
  {"created", "created"},
  {"modified", "modified"},
  {"filesize", "filesize"},
  {"filepath", "path"},
  {"filename", "name"}
};

/*************************************** FUNCTIONS ************************************************/

// Holds information about expressions in the `condition` section under the `grep` section.
struct Function : public Atom {
  Function() = default;
  Function(LineCol pos, std::string_view name, const std::vector<std::string_view>&& args, size_t op, size_t val)
                  : Args(args), Name(name), Operator(op), Value(val), Pos(pos) { validate(); }

  // Used to validate that the function is called with the right number of arguments
  // and that its return value is compared to a value if the function type demands it.
  void validate();

  std::vector<std::string_view> Args;
  std::string_view Name;
  size_t Operator = SIZE_MAX;
  size_t Value = SIZE_MAX;
  LineCol Pos;
};

// Defines a hash function for a Function object.
template<>
struct std::hash<Function>
{
    std::size_t operator()(const Function& func) const noexcept {
      std::size_t hash = 0, h2 = 0;
      for (const auto& arg : func.Args) {
        const std::size_t h = std::hash<std::string_view>{}(arg);
        boost::hash_combine(h2, h);
      }
      boost::hash_combine(hash, std::hash<std::string_view>{}(func.Name));
      boost::hash_combine(hash, h2);
      boost::hash_combine(hash, std::hash<size_t>{}(func.Operator));
      boost::hash_combine(hash, std::hash<size_t>{}(func.Value));
      return hash;
    }
};

// Expression node for expressions under the `condition` section under the `grep` section.
struct FuncNode : public Node {
  FuncNode() : Node(NodeType::FUNC) {}
  FuncNode(Function&& value) : Node(NodeType::FUNC) { Value = value; }
  Function Value;

  std::string getSqlQuery(const LlamaParser&) const override { return ""; };
};

// Holds information about minimum and maximum number of arguments in a function and whether
// or not its return value should be compared to a value in the expression.
struct FunctionProperties {
  size_t MinArgs;
  size_t MaxArgs;
  bool IsCompFunc;
};

// Holds the valid FunctionProperties for each function type. Used in Functions's validate().
static const std::unordered_map<std::string_view, FunctionProperties> FunctionValidProperties {
  {"all",            FunctionProperties{0, SIZE_MAX, false}},
  {"any",            FunctionProperties{0, SIZE_MAX, false}},
  {"offset",         FunctionProperties{1, 2, true}},
  {"count",          FunctionProperties{1, 1, true}},
  {"count_has_hits", FunctionProperties{0, SIZE_MAX, true}},
  {"length",         FunctionProperties{1, 2, true}}
};

/********************************** PATTERNS SECTION **********************************************/

static const std::string_view ASCII("ASCII");

// a range into Tokens vector, holds encodings separated by comma tokens
using Encodings = std::pair<size_t, size_t>;

// Holds information about each pattern defined in the `patterns` section under the `grep` section.
struct PatternDef {
  LG_KeyOptions Options = {0,0,0};
  Encodings Enc;
  std::string Pattern;
};

// Holds a mapping from the user-defined name of each pattern to the rest of its information.
struct PatternSection {
  std::map<std::string_view, PatternDef> Patterns;
};

struct GrepSection {
  PatternSection Patterns;
  std::shared_ptr<Node> Condition;
};

/************************************ META SECTION ************************************************/

struct MetaSection {
  std::unordered_map<std::string_view, std::string_view> Fields;
};

/************************************ HASH SECTION ************************************************/

using FileHashRecord = std::vector<std::pair<SFHASH_HashAlgorithm, std::string_view>>;

FileHashRecord::const_iterator findKey(const FileHashRecord& container, SFHASH_HashAlgorithm alg);

struct HashSection {
  std::vector<FileHashRecord> FileHashRecords;
  uint64_t HashAlgs = 0;
};

/************************************ RULE ********************************************************/

struct Rule {
  std::string getSqlQuery(const LlamaParser&) const;

  // Used for unique rule ID in the database.
  FieldHash getHash(const LlamaParser&) const;

  std::string_view      Name;
  MetaSection           Meta;
  HashSection           Hash;
  std::shared_ptr<Node> Signature;
  std::shared_ptr<Node> FileMetadata;
  GrepSection           Grep;

  // Relative input offset where the Meta section ends and the first "real" section begins.
  uint64_t Start = 0;
  // Relative input offset right before the rule's closing brace.
  uint64_t End = 0;
};

enum LlamaOp {
  EQUAL_EQUAL        = 1 << 0,
  NOT_EQUAL          = 1 << 1,
  GREATER_THAN       = 1 << 2,
  GREATER_THAN_EQUAL = 1 << 3,
  LESS_THAN          = 1 << 4,
  LESS_THAN_EQUAL    = 1 << 5
};

constexpr uint64_t AllLlamaOps = LlamaOp::EQUAL_EQUAL
                               | LlamaOp::NOT_EQUAL
                               | LlamaOp::GREATER_THAN
                               | LlamaOp::GREATER_THAN_EQUAL
                               | LlamaOp::LESS_THAN
                               | LlamaOp::LESS_THAN_EQUAL;

enum class LlamaReturnType {
  NONE,
  STRING,
  NUMBER,
  BOOL
};

struct PropertyInfo {
  uint64_t ValidOperators;
  LlamaTokenType Type;
};

struct LlamaFunc {
  size_t MinArgs;
  size_t MaxArgs;
  uint64_t ValidOperators;
  LlamaReturnType ReturnType;
};

struct Section {
  std::unordered_map<std::string_view, PropertyInfo> Props;
  std::unordered_map<std::string_view, LlamaFunc> Funcs;
};

const std::unordered_map<LlamaTokenType, Section> SectionDefs {
  {
    LlamaTokenType::FILE_METADATA,
    Section{
      {
        {std::string_view("created"),  PropertyInfo{AllLlamaOps, LlamaTokenType::DOUBLE_QUOTED_STRING}},
        {std::string_view("modified"), PropertyInfo{AllLlamaOps, LlamaTokenType::DOUBLE_QUOTED_STRING}},
        {std::string_view("filesize"), PropertyInfo{AllLlamaOps, LlamaTokenType::NUMBER}},
        {std::string_view("filepath"), PropertyInfo{LlamaOp::EQUAL_EQUAL | LlamaOp::NOT_EQUAL, LlamaTokenType::DOUBLE_QUOTED_STRING}},
        {std::string_view("filename"), PropertyInfo{LlamaOp::EQUAL_EQUAL | LlamaOp::NOT_EQUAL, LlamaTokenType::DOUBLE_QUOTED_STRING}}
      },
      {}
    }
  },
  {
    LlamaTokenType::SIGNATURE,
    Section{
      {
        {std::string_view("name"), PropertyInfo{LlamaOp::EQUAL_EQUAL, LlamaTokenType::DOUBLE_QUOTED_STRING}},
        {std::string_view("id"),   PropertyInfo{LlamaOp::EQUAL_EQUAL, LlamaTokenType::DOUBLE_QUOTED_STRING}}
      },
      {}
    }
  },
  {
    LlamaTokenType::CONDITION,
    Section{
      {},
      {
        {std::string_view("all"),            LlamaFunc{0, SIZE_MAX, 0, LlamaReturnType::BOOL}},
        {std::string_view("any"),            LlamaFunc{0, SIZE_MAX, AllLlamaOps, LlamaReturnType::BOOL}},
        {std::string_view("offset"),         LlamaFunc{1, 2, AllLlamaOps, LlamaReturnType::NUMBER}},
        {std::string_view("count"),          LlamaFunc{1, 1, AllLlamaOps, LlamaReturnType::NUMBER}},
        {std::string_view("count_has_hits"), LlamaFunc{0, SIZE_MAX, AllLlamaOps, LlamaReturnType::NUMBER}},
        {std::string_view("length"),         LlamaFunc{1, 2, AllLlamaOps, LlamaReturnType::NUMBER}}
      }
    }
  }
};

uint64_t toLlamaOp(LlamaTokenType t);

struct Property {
  size_t Name;
  size_t Op;
  size_t Val;
};

struct PropertyNode : public Node {
  PropertyNode() = default;
  PropertyNode(Property&& value) : Node(NodeType::PROP) { Value = value; }
  Property Value;

  std::string getSqlQuery(const LlamaParser& parser) const override;
};

/************************************ PARSER ******************************************************/

class LlamaParser {
public:
  LlamaParser() = default;
  LlamaParser(const std::string& input, const std::vector<Token>& tokens) : Tokens(tokens), Input(input) {}

  Token previous() const { return Tokens[CurIdx - 1]; }

  // Peek at the current Token without consuming it.
  Token peek() const { return Tokens[CurIdx]; }
  Token advance() { if (!isAtEnd()) ++CurIdx; return previous();}

  // Increments CurIdx if match.
  template <class... TokenTypes>
  bool matchAny(TokenTypes... types);

  // Does not increment CurIdx if match.
  template <class... TokenTypes>
  bool checkAny(TokenTypes... types) const { return ((peek().Type == types) || ...);};

  // Throws if CurIdx is not pointing to the given LlamaTokenType.
  std::string_view expect(LlamaTokenType);

  bool isAtEnd() const { return peek().Type == LlamaTokenType::END_OF_FILE; }

  // Increments CurIdx if match. Otherwise throws exception with given errMsg.
  template <class... LlamaTokenTypes>
  void mustParse(const std::string_view& errMsg, LlamaTokenTypes... types);

  std::string_view getPreviousLexeme() const { return previous().Lexeme; }
  std::string_view getCurrentLexeme() const { return peek().Lexeme; }
  std::string_view getLexemeAt(size_t idx) const { return Tokens[idx].Lexeme; }

  // Clear Input and Tokens, and reset CurIdx and CurRuleIdx counters.
  void clear();

  // Reset CurIdx and CurRuleIdx counters.
  void resetCounters();

  bool checkFunctionName() {
    std::string_view curLex = getCurrentLexeme();
    return (
      curLex == "any"            ||
      curLex == "all"            ||
      curLex == "offset"         ||
      curLex == "count"          ||
      curLex == "count_has_hits" ||
      curLex == "length"
    );
  }

  bool checkSignatureProperty() {
    std::string_view curLex = getCurrentLexeme();
    return (curLex == "name" || curLex == "id");
  }

  bool checkFileMetadataProperty() {
    std::string_view curLex = getCurrentLexeme();
    return (
      curLex == "created"  ||
      curLex == "modified" ||
      curLex == "filesize" ||
      curLex == "filepath" ||
      curLex == "filename"
    );
  }
  // This does not return anything because we have no use for the operator itself,
  // and if we did, we could just get it from `previous().Type`.
  void parseOperator();

  SFHASH_HashAlgorithm parseHash();
  FileHashRecord       parseFileHashRecord();
  std::string_view     parseHashValue();

  PatternDef parsePatternDef();
  PatternDef parsePatternMod();
  PatternDef parseHexString();
  Encodings  parseEncodings();

  std::shared_ptr<Node> parseFactor(LlamaTokenType section);
  std::shared_ptr<Node> parseTerm(LlamaTokenType section);
  std::shared_ptr<Node> parseExpr(LlamaTokenType section);

  FuncNode         parseFuncCall();
  PropertyNode     parseProperty(LlamaTokenType);

  MetaSection    parseMetaSection();
  HashSection    parseHashSection();
  GrepSection    parseGrepSection();
  PatternSection parsePatternsSection();

  Rule parseRuleDecl();

  std::vector<Rule> parseRules(const std::vector<size_t>& ruleIndices);

  const std::vector<ParserError>& getErrors() const { return Errors; }

  std::vector<Token>       Tokens;
  std::vector<ParserError> Errors;
  std::string              Input;
  uint64_t                 CurIdx     = 0;
  uint64_t                 CurRuleIdx = 0;
};

template <class... TokenTypes>
bool LlamaParser::matchAny(TokenTypes... types) {
  if (checkAny(types...)) {
    advance();
    return true;
  }
  return false;
}

template <class... LlamaTokenTypes>
void LlamaParser::mustParse(const std::string_view& errMsg, LlamaTokenTypes... types) {
  if (!matchAny(types...)) {
    throw ParserError(errMsg, peek().Pos);
  }
}
