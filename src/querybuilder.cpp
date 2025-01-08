#include "querybuilder.h"

std::string QueryBuilder::getSqlClause(std::shared_ptr<Node> n) {
  std::string clause = "";
    switch (n->Type) {
      case NodeType::PROP: {
        auto pn = std::static_pointer_cast<PropertyNode>(n);
        clause = getSqlClause(pn); 
        break;
      }
      case NodeType::BOOL: {
        auto bn = std::static_pointer_cast<BoolNode>(n);
      }
    }
    return clause;
}

std::string QueryBuilder::getSqlClause(std::shared_ptr<PropertyNode> pn) {
  std::string clause = "";
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

std::string QueryBuilder::getSqlClause(std::shared_ptr<BoolNode> bn) {
  std::string clause = "(";
  clause += getSqlClause(bn->Left);
  clause += bn->Operation == BoolNode::Op::AND ? " AND " : " OR ";
  clause += getSqlClause(bn->Right);
  clause += ")";
  return clause;
}