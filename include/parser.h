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

struct SignatureSection {
  std::vector<std::string> Signatures;
};

struct FileMetadataDef {
  TokenType Property;
  TokenType Operator;
  std::string Value;
};

struct FileMetadataSection {
  std::vector<FileMetadataDef> Fields;
};

struct PatternDef {
  std::string Pattern;
  LG_KeyOptions Options = {0,0,0};
  int Encoding;
};

struct PatternSection {
  std::unordered_map<std::string, std::vector<PatternDef>> Patterns;
};

struct ConditionFunction {
  TokenType Name;
  std::vector<std::string> Args;
  TokenType Operator = TokenType::NONE;
  std::string Value;
};

enum class NodeType {
  AND, OR, FUNC, VAL
};

struct Node {
  ConditionFunction Value;
  NodeType Type;
  std::shared_ptr<Node> Left;
  std::shared_ptr<Node> Right;
};

struct ConditionSection {
  std::shared_ptr<Node> Tree;
};

struct GrepSection {
  PatternSection Patterns;
  ConditionSection Condition;
};

struct Rule {
  std::string Name;
  MetaSection Meta;
  HashSection Hash;
  SignatureSection Signature;
  FileMetadataSection FileMetadata;
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

  HashSection parseHashSection();
  SFHASH_HashAlgorithm parseHash();
  FileHashRecord parseFileHashRecord();
  TokenType parseOperator();
  std::vector<PatternDef> parsePatternMod();
  std::vector<int> parseEncodings();
  std::vector<PatternDef> parsePatternDef();
  PatternSection parsePatternsSection();
  std::string parseNumber();
  std::vector<PatternDef> parseHexString();
  ConditionFunction parseFuncCall();
  std::shared_ptr<Node> parseFactor();
  std::shared_ptr<Node> parseTerm();
  std::shared_ptr<Node> parseExpr();
  ConditionSection parseConditionSection();
  SignatureSection parseSignatureSection();
  GrepSection parseGrepSection();
  FileMetadataDef parseFileMetadataDef();
  FileMetadataSection parseFileMetadataSection();
  MetaSection parseMetaSection();
  Rule parseRuleDecl();
  std::vector<Rule> parseRules();

  std::string Input;
  std::vector<Token> Tokens;
  std::unordered_map<std::string, std::string> Patterns;
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
