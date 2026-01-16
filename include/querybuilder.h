#pragma once

#include <parser.h>
#include <fieldhash.h>

class QueryBuilder {
public:
  QueryBuilder(const LlamaParser& parser) : Parser(parser) {}

  std::string buildSqlClause(const Node* n);
  std::string buildSqlClause(const PropertyNode* pn);
  std::string buildSqlClause(const BoolNode* bn);

  std::string buildSqlQuery(const FieldHash& hash, const Rule& rule);

private:
  void buildSqlClauseImpl(const Node* n, std::string& out);
  void buildSqlClauseImpl(const PropertyNode* pn, std::string& out);
  void buildSqlClauseImpl(const BoolNode* bn, std::string& out);

  const LlamaParser& Parser;
};
