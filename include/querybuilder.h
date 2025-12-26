#pragma once

#include <parser.h>

class QueryBuilder {
public:
  QueryBuilder(const LlamaParser& parser) : Parser(parser) {}

  std::string buildSqlClause(const Node* n);
  std::string buildSqlClause(const PropertyNode* pn);
  std::string buildSqlClause(const BoolNode* bn);

  std::string buildSqlQuery(const Rule& rule);

private:
  const LlamaParser& Parser;
};
