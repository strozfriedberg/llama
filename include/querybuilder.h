#include <parser.h>

class QueryBuilder {
public:
  QueryBuilder(const LlamaParser& parser) : Parser(parser) {}

  std::string getSqlClause(std::shared_ptr<Node> n);
  std::string getSqlClause(std::shared_ptr<PropertyNode> pn);
  std::string getSqlClause(std::shared_ptr<BoolNode> bn);

private:
  const LlamaParser& Parser;
};