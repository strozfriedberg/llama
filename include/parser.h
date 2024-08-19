#include "token.h"

#include <hasher/common.h>
#include <lightgrep/api.h>
#include <lightgrep/util.h>

#include <memory>
#include <unordered_map>
#include <vector>

class ParserError : public UnexpectedInputError {
public:
  ParserError(const std::string& message, LineCol pos) : UnexpectedInputError(message, pos) {}
};

struct MetaSection {
  std::unordered_map<std::string, std::string> Fields;
};

using FileHashRecord = std::unordered_map<SFHASH_HashAlgorithm, std::string>;

struct HashSection {
  std::vector<FileHashRecord> FileHashRecords;
  uint64_t HashAlgs = 0;
};

struct Atom {};

struct SignatureDef : public Atom {
  TokenType Attr;
  std::string Val;
};


struct FileMetadataDef : public Atom {
  TokenType Property;
  TokenType Operator;
  std::string Value;
};

struct PatternDef {
  std::string Pattern;
  LG_KeyOptions Options = {0,0,0};
  int Encoding;
};

struct PatternSection {
  std::unordered_map<std::string, std::vector<PatternDef>> Patterns;
};

struct ConditionFunction : public Atom {
  ConditionFunction() = default;
  ConditionFunction(LineCol pos) : Pos(pos) {}
  ~ConditionFunction() = default;

  void assignValidators();
  void validate();

  LineCol Pos;
  TokenType Name;
  std::vector<std::string> Args;
  TokenType Operator = TokenType::NONE;
  std::string Value;

  // validators
  size_t MinArgs;
  size_t MaxArgs;
  bool IsCompFunc;
};

enum class NodeType {
  AND, OR, FUNC, SIG, META
};

struct Node {
  virtual ~Node() = default;

  NodeType Type;
  std::shared_ptr<Node> Left;
  std::shared_ptr<Node> Right;
};

struct SigDefNode : public Node {
  SigDefNode() { Type = NodeType::SIG; }
  SignatureDef Value;
};

template<>
struct std::hash<SignatureDef>
{
    std::size_t operator()(const SignatureDef& sig) const noexcept
    {
        const std::size_t h1 = std::hash<TokenType>{}(sig.Attr);
        const std::size_t h2 = std::hash<std::string>{}(sig.Val);
        return h1 ^ ((h2 << 1) >> 1);
    }
};

struct FuncNode : public Node {
  FuncNode() { Type = NodeType::FUNC; }
  ConditionFunction Value;
};

template<>
struct std::hash<ConditionFunction>
{
    std::size_t operator()(const ConditionFunction& func) const noexcept
    {
        const std::size_t h1 = std::hash<TokenType>{}(func.Name);
        std::size_t h2 = 0;
        for (const auto& arg : func.Args) {
          const std::size_t h = std::hash<std::string>{}(arg);
          h2 ^= ((h << 1) >> 1);
        }
        const std::size_t h3 = std::hash<TokenType>{}(func.Operator);
        const std::size_t h4 = std::hash<std::string>{}(func.Value);
        return h1 ^ ((h2 << 1) >> 1) ^ ((h3 << 1) >> 1) ^ ((h4 << 1) >> 1);
    }
};

struct FileMetadataNode : public Node {
  FileMetadataNode() { Type = NodeType::META; }
  FileMetadataDef Value;
};

template<>
struct std::hash<FileMetadataDef>
{
    std::size_t operator()(const FileMetadataDef& meta) const noexcept
    {
        const std::size_t h1 = std::hash<TokenType>{}(meta.Property);
        const std::size_t h2 = std::hash<TokenType>{}(meta.Operator);
        const std::size_t h3 = std::hash<std::string>{}(meta.Value);
        return h1 ^ ((h2 << 1) >> 1) ^ ((h3 << 1) >> 1);
    }
};

struct GrepSection {
  PatternSection Patterns;
  std::shared_ptr<Node> Condition;
};

struct Rule {
  std::string Name;
  MetaSection Meta;
  HashSection Hash;
  std::shared_ptr<Node> Signature;
  std::shared_ptr<Node> FileMetadata;
  GrepSection Grep;
};

class LlamaParser {
public:
  LlamaParser(const std::string& input, const std::vector<Token>& tokens) : Input(input), Tokens(tokens) {}

  Token previous() const { return Tokens.at(CurIdx - 1); }
  Token peek() const { return Tokens.at(CurIdx); }
  Token advance() { if (!isAtEnd()) ++CurIdx; return previous();}

  template <class... TokenTypes>
  bool matchAny(TokenTypes... types);

  template <class... TokenTypes>
  bool checkAny(TokenTypes... types) { return ((peek().Type == types) || ...);};

  bool isAtEnd() const { return peek().Type == TokenType::END_OF_FILE; }

  template <class... TokenTypes>
  void mustParse(const std::string& errMsg, TokenTypes... types);

  std::string getPreviousLexeme() const { return Input.substr(previous().Start, previous().length()); }

  HashSection parseHashSection();
  SFHASH_HashAlgorithm parseHash();
  FileHashRecord parseFileHashRecord();
  std::string parseHashValue();
  TokenType parseOperator();
  std::vector<PatternDef> parsePatternMod();
  std::vector<int> parseEncodings();
  int parseEncoding();
  std::vector<PatternDef> parsePatternDef();
  PatternSection parsePatternsSection();
  std::string parseNumber();
  std::vector<PatternDef> parseHexString();
  ConditionFunction parseFuncCall();
  std::shared_ptr<Node> parseFactor();
  std::shared_ptr<Node> parseTerm();
  std::shared_ptr<Node> parseExpr();
  SignatureDef parseSignatureDef();
  GrepSection parseGrepSection();
  FileMetadataDef parseFileMetadataDef();
  MetaSection parseMetaSection();
  Rule parseRuleDecl();
  std::vector<Rule> parseRules();

  std::string Input;
  std::vector<Token> Tokens;
  std::unordered_map<std::string, std::string> Patterns;
  std::unordered_map<std::size_t, Atom> Atoms;
  uint64_t CurIdx = 0;
};

template <class... TokenTypes>
bool LlamaParser::matchAny(TokenTypes... types) {
  if (checkAny(types...)) {
    advance();
    return true;
  }
  return false;
}

template <class... TokenTypes>
void LlamaParser::mustParse(const std::string& errMsg, TokenTypes... types) {
  if (!matchAny(types...)) {
    throw ParserError(errMsg, peek().Pos);
  }
}
