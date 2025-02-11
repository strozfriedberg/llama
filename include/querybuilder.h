#include <parser.h>

class QueryBuilder {
public:
  QueryBuilder(const LlamaParser& parser) : Parser(parser) {}

  std::string buildSqlClause(Node* n);
  std::string buildSqlClause(PropertyNode* pn);
  std::string buildSqlClause(BoolNode* bn);

  std::string buildSqlQuery(const Rule& rule);

private:
  const LlamaParser& Parser;
};