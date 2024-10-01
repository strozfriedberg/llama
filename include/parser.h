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
  ParserError(const std::string& message, LineCol pos) : UnexpectedInputError(message, pos) {}
};

struct MetaSection {
  std::unordered_map<std::string_view, std::string_view> Fields;
};

using FileHashRecord = std::unordered_map<SFHASH_HashAlgorithm, std::string>;

struct HashSection {
  std::vector<FileHashRecord> FileHashRecords;
  uint64_t HashAlgs = 0;
};

class LlamaParser;

struct Atom {};

struct SignatureDef : public Atom {
  size_t Attr;
  size_t Val;
};


struct FileMetadataDef : public Atom {
  size_t Property;
  size_t Operator;
  size_t Value;
};

struct Function : public Atom {
  Function() = default;
  Function(LineCol pos, LlamaTokenType name, const std::vector<std::string_view>&& args, size_t op, size_t val)
                  : Pos(pos), Name(name), Args(args), Operator(op), Value(val) { validate(); }
  ~Function() = default;

  void validate();

  LineCol Pos;
  LlamaTokenType Name;
  std::vector<std::string_view> Args;
  size_t Operator = SIZE_MAX;
  size_t Value = SIZE_MAX;
};

struct FunctionProperties {
  size_t MinArgs;
  size_t MaxArgs;
  bool IsCompFunc;
};

static const std::unordered_map<LlamaTokenType, FunctionProperties> FunctionValidProperties {
  {LlamaTokenType::ALL,            FunctionProperties{0, SIZE_MAX, false}},
  {LlamaTokenType::ANY,            FunctionProperties{0, SIZE_MAX, false}},
  {LlamaTokenType::OFFSET,         FunctionProperties{1, 2, true}},
  {LlamaTokenType::COUNT,          FunctionProperties{1, 1, true}},
  {LlamaTokenType::COUNT_HAS_HITS, FunctionProperties{0, SIZE_MAX, true}},
  {LlamaTokenType::LENGTH,         FunctionProperties{1, 2, true}}
};

struct PatternDef {
  std::string Pattern;
  LG_KeyOptions Options = {0,0,0};
  std::string Encoding;
};

struct PatternSection {
  std::map<std::string_view, std::vector<PatternDef>> Patterns;
};

enum class NodeType {
  AND, OR, FUNC, SIG, META
};

struct Node {
  virtual ~Node() = default;
  virtual std::string getSqlQuery(const LlamaParser& parser) const = 0;

  Node(NodeType type) : Type(type) {}
  Node() = default;

  NodeType Type;
  std::shared_ptr<Node> Left;
  std::shared_ptr<Node> Right;
};

struct BoolNode : public Node {
  std::string getSqlQuery(const LlamaParser& parser) const override;
};

struct SigDefNode : public Node {
  SigDefNode() : Node(NodeType::SIG) {}
  SigDefNode(SignatureDef&& value) : Node(NodeType::SIG) { Value = value; }
  SignatureDef Value;

  std::string getSqlQuery(const LlamaParser& parser) const override { return ""; };
};

struct FileMetadataNode : public Node {
  FileMetadataNode() : Node(NodeType::META) {}
  FileMetadataNode(FileMetadataDef&& value) : Node(NodeType::META) { Value = value; }
  FileMetadataDef Value;

  std::string getSqlQuery(const LlamaParser& parser) const override;
};

struct FuncNode : public Node {
  FuncNode() : Node(NodeType::FUNC) {}
  FuncNode(Function&& value) : Node(NodeType::FUNC) { Value = value; }
  Function Value;

  std::string getSqlQuery(const LlamaParser& parser) const override { return ""; };
};

template<>
struct std::hash<SignatureDef>
{
    std::size_t operator()(const SignatureDef& sig) const noexcept {
      std::size_t hash = 0;
      boost::hash_combine(hash, std::hash<size_t>{}(sig.Attr));
      boost::hash_combine(hash, std::hash<size_t>{}(sig.Val));
      return hash;
    }
};

template<>
struct std::hash<FileMetadataDef>
{
    std::size_t operator()(const FileMetadataDef& meta) const noexcept {
      std::size_t hash = 0;
      boost::hash_combine(hash, std::hash<size_t>{}(meta.Property));
      boost::hash_combine(hash, std::hash<size_t>{}(meta.Operator));
      boost::hash_combine(hash, std::hash<size_t>{}(meta.Value));
      return hash;
    }
};

template<>
struct std::hash<Function>
{
    std::size_t operator()(const Function& func) const noexcept {
      std::size_t hash = 0, h2 = 0;
      for (const auto& arg : func.Args) {
        const std::size_t h = std::hash<std::string_view>{}(arg);
        boost::hash_combine(h2, h);
      }
      boost::hash_combine(hash, std::hash<LlamaTokenType>{}(func.Name));
      boost::hash_combine(hash, h2);
      boost::hash_combine(hash, std::hash<size_t>{}(func.Operator));
      boost::hash_combine(hash, std::hash<size_t>{}(func.Value));
      return hash;
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

  uint64_t Start = 0;
  uint64_t End = 0;

  std::string getSqlQuery(const LlamaParser&) const;
  FieldHash getHash(const LlamaParser&) const;
};

class LlamaParser {
public:
  LlamaParser() = default;
  LlamaParser(const std::string& input, const std::vector<Token>& tokens) : Input(input), Tokens(tokens) {}

  Token previous() const { return Tokens.at(CurIdx - 1); }
  Token peek() const { return Tokens.at(CurIdx); }
  Token advance() { if (!isAtEnd()) ++CurIdx; return previous();}

  template <class... TokenTypes>
  bool matchAny(TokenTypes... types);

  template <class... TokenTypes>
  bool checkAny(TokenTypes... types) { return ((peek().Type == types) || ...);};

  bool isAtEnd() const { return peek().Type == LlamaTokenType::END_OF_FILE; }

  template <class... LlamaTokenTypes>
  void mustParse(const std::string& errMsg, LlamaTokenTypes... types);

  std::string_view getPreviousLexeme() const { return std::string_view(Input).substr(previous().Start, previous().length()); }
  std::string getLexemeAt(size_t idx) const { return Input.substr(Tokens.at(idx).Start, Tokens.at(idx).length()); }

  void clear() { Patterns.clear(); Atoms.clear(); Tokens.clear(); Input.clear(); CurIdx = 0; }

  std::string_view expect(LlamaTokenType);
  HashSection parseHashSection();
  SFHASH_HashAlgorithm parseHash();
  FileHashRecord parseFileHashRecord();
  std::string parseHashValue();
  void parseOperator();
  std::vector<PatternDef> parsePatternMod();
  std::vector<std::string> parseEncodings();
  std::vector<PatternDef> parsePatternDef();
  PatternSection parsePatternsSection();
  std::vector<PatternDef> parseHexString();
  std::shared_ptr<Node> parseFactor(LlamaTokenType section);
  std::shared_ptr<Node> parseTerm(LlamaTokenType section);
  std::shared_ptr<Node> parseExpr(LlamaTokenType section);
  FuncNode parseFuncCall();
  SigDefNode parseSignatureDef();
  GrepSection parseGrepSection();
  FileMetadataNode parseFileMetadataDef();
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

template <class... LlamaTokenTypes>
void LlamaParser::mustParse(const std::string& errMsg, LlamaTokenTypes... types) {
  if (!matchAny(types...)) {
    throw ParserError(errMsg, peek().Pos);
  }
}
