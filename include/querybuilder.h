#include <parser.h>

class QueryBuilder {
public:
  QueryBuilder(const LlamaParser& parser) : Parser(parser) {}

  std::string buildSqlClause(std::shared_ptr<Node> n);
  std::string buildSqlClause(std::shared_ptr<PropertyNode> pn);
  std::string buildSqlClause(std::shared_ptr<BoolNode> bn);

private:
  const LlamaParser& Parser;
};