#include "querybuilder.h"

const static std::unordered_map<std::string_view, std::string> FileMetadataPropertySqlLookup {
  {"created", "Created"},
  {"modified", "Modified"},
  {"filesize", "Filesize"},
  {"filepath", "Path"},
  {"filename", "Name"}
};

std::string QueryBuilder::buildSqlClause(const Node* n) {
  std::string clause;
  switch (n->Type) {
    case NodeType::PROP: {
      clause = buildSqlClause(static_cast<const PropertyNode*>(n));
      break;
    }
    case NodeType::BOOL: {
      clause = buildSqlClause(static_cast<const BoolNode*>(n));
      break;
    }
    default: {
      throw std::runtime_error("Invalid node type " + std::to_string(static_cast<int>(n->Type)));
    }
  }
  return clause;
}

std::string QueryBuilder::buildSqlClause(const PropertyNode* pn) {
  std::string clause;
  std::string_view propertyName = Parser.lexemeAt(pn->Value.Name);
  clause += FileMetadataPropertySqlLookup.find(propertyName)->second;
  clause += " ";
  clause += Parser.lexemeAt(pn->Value.Op);
  clause += " ";
  std::string_view val = Parser.lexemeAt(pn->Value.Val);
  if (Parser.Tokens[pn->Value.Val].Type == LlamaTokenType::DOUBLE_QUOTED_STRING) {
    clause += "'";
    clause += val;
    clause += "'";
  }
  else {
    clause += val;
  }
  return clause;
}

std::string QueryBuilder::buildSqlClause(const BoolNode* bn) {
  std::string clause = "(";
  clause += buildSqlClause(bn->Left);
  clause += bn->Operation == BoolNode::Op::AND ? " AND " : " OR ";
  clause += buildSqlClause(bn->Right);
  clause += ")";
  return clause;
}

std::string QueryBuilder::buildSqlQuery(const Rule& rule) {
  std::string query = "SELECT '";
  query += rule.getHash(Parser).to_string();
  query += "', Path, Name, Addr FROM dirent, inode WHERE dirent.Metaaddr == inode.Addr";

  if (rule.FileMetadata) {
    query += " AND ";
    query += buildSqlClause(rule.FileMetadata);
  }

  return query;
}

