#include "querybuilder.h"

const static std::unordered_map<std::string_view, std::string> FileMetadataPropertySqlLookup {
  {"created", "Created"},
  {"modified", "Modified"},
  {"filesize", "Filesize"},
  {"filepath", "Path"},
  {"filename", "Name"}
};

const static std::unordered_map<std::string_view, std::string> SignaturePropertySqlLookup {
  {"name", "Name"},
  {"id", "Id"}
};

void QueryBuilder::buildSqlClauseImpl(const Node* n, std::string& out) {
  switch (n->Type) {
    case NodeType::PROP: {
      buildSqlClauseImpl(static_cast<const PropertyNode*>(n), out);
      break;
    }
    case NodeType::BOOL: {
      buildSqlClauseImpl(static_cast<const BoolNode*>(n), out);
      break;
    }
    default: {
      throw std::runtime_error("Invalid node type " + std::to_string(static_cast<int>(n->Type)));
    }
  }
}

std::string QueryBuilder::buildSqlClause(const Node* n) {
  std::string result;
  result.reserve(128);
  buildSqlClauseImpl(n, result);
  return result;
}

void QueryBuilder::buildSqlClauseImpl(const PropertyNode* pn, std::string& out) {
  std::string_view propertyName = Parser.lexemeAt(pn->Value.Name);
  auto it = FileMetadataPropertySqlLookup.find(propertyName);
  if (it == FileMetadataPropertySqlLookup.end()) {
    throw std::runtime_error("Unknown file_metadata property: " + std::string(propertyName));
  }
  out += it->second;
  out += " ";
  out += Parser.lexemeAt(pn->Value.Op);
  out += " ";
  std::string_view val = Parser.lexemeAt(pn->Value.Val);
  if (Parser.Tokens[pn->Value.Val].Type == LlamaTokenType::DOUBLE_QUOTED_STRING) {
    out += "'";
    out += val;
    out += "'";
  }
  else {
    out += val;
  }
}

std::string QueryBuilder::buildSqlClause(const PropertyNode* pn) {
  std::string result;
  result.reserve(64);
  buildSqlClauseImpl(pn, result);
  return result;
}

void QueryBuilder::buildSqlClauseImpl(const BoolNode* bn, std::string& out) {
  out += "(";
  buildSqlClauseImpl(bn->Left, out);
  out += bn->Operation == BoolNode::Op::AND ? " AND " : " OR ";
  buildSqlClauseImpl(bn->Right, out);
  out += ")";
}

std::string QueryBuilder::buildSqlClause(const BoolNode* bn) {
  std::string result;
  result.reserve(128);
  buildSqlClauseImpl(bn, result);
  return result;
}

void QueryBuilder::buildSignaturePropertyImpl(const PropertyNode* pn, std::string& out) {
  std::string_view propertyName = Parser.lexemeAt(pn->Value.Name);
  auto it = SignaturePropertySqlLookup.find(propertyName);
  if (it == SignaturePropertySqlLookup.end()) {
    throw std::runtime_error("Unknown signature property: " + std::string(propertyName));
  }
  out += it->second;
  out += " ";
  out += Parser.lexemeAt(pn->Value.Op);
  out += " ";
  std::string_view val = Parser.lexemeAt(pn->Value.Val);
  if (Parser.Tokens[pn->Value.Val].Type == LlamaTokenType::DOUBLE_QUOTED_STRING) {
    out += "'";
    out += val;
    out += "'";
  }
  else {
    out += val;
  }
}

void QueryBuilder::buildSignatureClauseImpl(const Node* n, std::string& out) {
  switch (n->Type) {
    case NodeType::PROP: {
      buildSignaturePropertyImpl(static_cast<const PropertyNode*>(n), out);
      break;
    }
    case NodeType::BOOL: {
      const BoolNode* bn = static_cast<const BoolNode*>(n);
      out += "(";
      buildSignatureClauseImpl(bn->Left, out);
      out += bn->Operation == BoolNode::Op::AND ? " AND " : " OR ";
      buildSignatureClauseImpl(bn->Right, out);
      out += ")";
      break;
    }
    default: {
      throw std::runtime_error("Invalid node type " + std::to_string(static_cast<int>(n->Type)));
    }
  }
}

std::string QueryBuilder::buildSqlQuery(const FieldHash& hash, const Rule& rule) {
  std::string query;
  query.reserve(256);
  query = "SELECT '";
  query += hash.to_string();
  query += "', Path, Name, inode.Id FROM dirent, inode WHERE dirent.MetaId == inode.Id";

  if (rule.FileMetadata) {
    query += " AND ";
    buildSqlClauseImpl(rule.FileMetadata, query);
  }

  if (rule.Signature) {
    query += " AND inode.Id IN (SELECT h.InodeId FROM hash h"
             " JOIN file_signatures fs ON h.SHA256 = fs.FileHash"
             " JOIN signatures s ON fs.SigId = s.Id WHERE ";
    buildSignatureClauseImpl(rule.Signature, query);
    query += ")";
  }

  return query;
}

