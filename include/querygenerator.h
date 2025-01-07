#include <parser.h>

class QueryGenerator {
public:
  QueryGenerator(const LlamaParser& parser) : Parser(parser) {}

  // std::string getSqlClause(const Node* n) {

  // }

  std::string getSqlClause(const PropertyNode& pn) {
    std::string query = "";
    std::string_view propertyName = Parser.lexemeAt(pn.Value.Name);
    query += FileMetadataPropertySqlLookup.find(propertyName)->second;
    query += " ";
    query += Parser.lexemeAt(pn.Value.Op);
    query += " ";
    std::string_view val = Parser.lexemeAt(pn.Value.Val);
    if (Parser.Tokens[pn.Value.Val].Type == LlamaTokenType::DOUBLE_QUOTED_STRING) {
      query += "'";
      query += val;
      query += "'";
    }
    else {
      query += val;
    }
    return query;
  }

  // std::string getSqlClause(const BoolNode& bn) {
  //   std::string query = "(";
  //   query += getSqlClause(*bn.Left);
  //   query += Type == NodeType::AND ? " AND " : " OR ";
  //   query += Right->getSqlQuery(parser);
  //   query += ")";
  //   return query;
  // }

private:
  const LlamaParser& Parser;
};