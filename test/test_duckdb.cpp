#include <catch2/catch_test_macros.hpp>

#include "throw.h"

#include "direntbatch.h"
#include "duckhash.h"
#include "duckinode.h"
#include "inode.h"
#include "llamaduck.h"

#include <cmath>
#include <duckdb.h>
#include <tuple>

TEST_CASE("testDuckDBVersion") {
  REQUIRE(std::string("v1.0.0") == duckdb_library_version());
}

TEST_CASE("TestMakeDuckDB") { 
  LlamaDB db;
  LlamaDBConnection conn(db);

  REQUIRE(DuckDirent::createTable(conn.get(), "dirent"));

  std::vector<Dirent> dirents = {
    {"/tmp/", "foo", "f~1", "File", "Allocated", 3, 2, 0, 0},
    {"/tmp/", "bar", "b~1", "File", "Deleted", 4, 2, 0, 0},
    {"/temp/", "bar", "b~2", "File", "Allocated", 6, 5, 0, 0}
  };

  DirentBatch batch;
  std::string query;;
  for (auto dirent : dirents) {
    batch.add(dirent);
  }
  REQUIRE(batch.size() == dirents.size());
  REQUIRE(batch.Buf.size() == 89);

  LlamaDBAppender appender(conn.get(), "dirent"); // need an appender object, too, which also doesn't jibe with smart pointers, and destroy must be called even if create returns an error
  // REQUIRE(state != DuckDBError);
  batch.copyToDB(appender.get());
  REQUIRE(appender.flush());

  duckdb_result result;
  auto state = duckdb_query(conn.get(), "SELECT * FROM dirent WHERE dirent.path = '/tmp/' and ((dirent.name = 'bar' and dirent.metaaddr = 4) or (dirent.shortname = 'f~1' and dirent.parentaddr = 2));", &result);
  //REQUIRE(std::string("") == duckdb_result_error(&result));
  REQUIRE(state != DuckDBError);
  REQUIRE(duckdb_row_count(&result) == 2);
  REQUIRE(duckdb_column_count(&result) == 10);
  unsigned int i = 0;
  REQUIRE(std::string("Id") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("Path") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("Name") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("ShortName") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("Type") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("Flags") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("MetaAddr") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("ParentAddr") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("MetaSeq") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("ParentSeq") == duckdb_column_name(&result, i));
}

template<size_t CurIndex, size_t N>
static constexpr auto findIndex(const char* col, const std::initializer_list<const char*>& colNames) {
  auto curCol = std::data(colNames)[CurIndex];
  auto curLen = std::char_traits<char>::length(curCol);
  if (curLen == std::char_traits<char>::length(col) && std::char_traits<char>::compare(col, curCol, curLen) == 0) {
    return CurIndex;
  }
  else if constexpr (CurIndex + 1 < N) {
    return findIndex<CurIndex + 1, N>(col, colNames);
  }
  else {
    return N;
  }
}

template<typename ColList, typename... Args>
class DuckDBRecord {
public:
  typedef std::tuple<Args...> ValuesType;

  std::tuple<Args...> Values;

  static constexpr auto colIndex(const char* col) {
    unsigned int i = 0;
    const auto nameLen = std::char_traits<char>::length(col);
    for (auto name : ColList::ColNames) {
      if (std::char_traits<char>::length(col) == nameLen && std::char_traits<char>::compare(col, name, nameLen) == 0) {
        break;
      }
      ++i;
    }
    return i;
  }

  template<typename ArgType> // ArgType needs to be a const char* but is a template type to fake out C++
  constexpr auto& operator[](const ArgType& col) {
    constexpr auto index(findIndex<0, 3>(col, ColList::ColNames));
    return std::get<index>(Values);
  }
/*
  constexpr const auto& operator[](const char* col) const {
    return std::get<colIndex(col)>(Values);
  }
*/
};

class DuckRecColumns {
public:
  static constexpr auto ColNames = {"path", "meta_addr", "parent_addr"};
};

/*
class DuckRec : public SchemaType<
                        std::string,
                        uint64_t,
                        uint64_t> {
 
public:
  enum {
    Path = 0,
    MetaAddr = 1,
    ParentAddr = 2
  };

  static constexpr auto ColNames = {"path", "meta_addr", "parent_addr"};

  static void createTable(duckdb_connection& dbconn) {  
    std::string query = "CREATE TABLE DuckRec (path VARCHAR, meta_addr UBIGINT, parent_addr UBIGINT);";
    THROW_IF(duckdb_query(dbconn, query.c_str(), nullptr) == DuckDBError, "Failed to create table");
  }
};

TEST_CASE("testTypesFiguring") {
//  DuckRec rec;

//  std::get<DuckRec::Path>(rec.Values) = "howdy";
//  REQUIRE("howdy" == std::get<DuckRec::Path>(rec.Values));

  REQUIRE(3 == DuckRecColumns::ColNames.size());

//  REQUIRE(DuckRec::colIndex("meta_addr") == 1);
//  static_assert(findIndex<0, 3>("meta_addr", DuckRecColumns::ColNames) == 1);
//  static_assert(DuckRec::colIndex("meta_addr") == 1);

  REQUIRE(std::string("VARCHAR") == duckdbType<std::string>());
  static_assert(std::char_traits<char>::compare(duckdbType<std::string>(), "VARCHAR", 7) == 0);

  REQUIRE(createQuery<DuckRec>("DuckRec") == "CREATE TABLE DuckRec (path VARCHAR, meta_addr UBIGINT, parent_addr UBIGINT);");

//  REQUIRE(rec["path"] == "howdy");
//  rec["path"] = "hello";
//  REQUIRE(rec["path"] == "hello");
}
*/


TEST_CASE("inodeWriting") {
  LlamaDB db;
  LlamaDBConnection conn(db);

  static_assert(DuckInode::ColNames.size() == 15);
  static_assert(DuckInode::colIndex("Id") == 0);
  static_assert(DuckInode::colIndex("Modified") == 13);
  REQUIRE(DuckInode::createTable(conn.get(), "inode"));

  Inode i1{"id 1", "File", "Allocated", 16, 32768, 12345, 500, 1000, "", 1, 37, "1978-04-01 12:32:25", "2024-08-22 14:45:00", "2024-08-22 22:42:23", "2024-07-13 02:12:59"};
  Inode i2{"id 2", "File", "Deleted", 17, 32768, 987654321098765432u, 501, 1001, "", 2, 38, "1978-04-01 12:32:25", "2024-08-22 14:45:00", "2024-08-22 22:42:23", "2024-07-13 02:12:59"};

  InodeBatch batch;
  batch.add(i1);
  REQUIRE(batch.Buf.size() == 101);
  batch.add(i2);
  REQUIRE(batch.Buf.size() == 200);
  REQUIRE(batch.size() == 2);

  LlamaDBAppender appender(conn.get(), "inode");
  REQUIRE(2 == batch.copyToDB(appender.get()));
  REQUIRE(appender.flush());

  duckdb_result result;
  auto state = duckdb_query(conn.get(), "SELECT * FROM inode;", &result);// WHERE inode.type = 'File' and inode.flags = 'Deleted';", &result);
  CHECK(state != DuckDBError);
  CHECK(duckdb_result_error(&result) == nullptr);
  CHECK(duckdb_row_count(&result) == 2);
  REQUIRE(duckdb_column_count(&result) == DuckInode::ColNames.size());
  unsigned int i = 0;
  REQUIRE(std::string("Id") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("Type") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("Flags") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("Addr") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("FsOffset") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("Filesize") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("Uid") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("Gid") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("LinkTarget") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("NumLinks") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("SeqNum") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("Created") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("Accessed") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("Modified") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("Metadata") == duckdb_column_name(&result, i));
  duckdb_destroy_result(&result);

  state = duckdb_query(conn.get(), "SELECT * FROM inode WHERE inode.id = 'id 1';", &result);//inode.type = 'File' and inode.flags = 'Deleted';", &result);
  CHECK(state != DuckDBError);
  CHECK(duckdb_result_error(&result) == nullptr);
  CHECK(duckdb_row_count(&result) == 1);
  duckdb_destroy_result(&result);

  state = duckdb_query(conn.get(), "SELECT * FROM inode WHERE inode.type = 'File' and inode.flags = 'Deleted';", &result);
  CHECK(state != DuckDBError);
  CHECK(duckdb_result_error(&result) == nullptr);
  CHECK(duckdb_row_count(&result) == 1);
  duckdb_destroy_result(&result);
}

TEST_CASE("testDuckHash") {
  LlamaDB db;
  LlamaDBConnection conn(db);

  static_assert(DuckHashRec::ColNames.size() == 6);
  REQUIRE(DuckHashRec::createTable(conn.get(), "hash"));

  HashRec h1{1, "an md5", "a sha1", "a sha256", "a blake3", "an ssdeep"};
  HashRec h2{2, "another md5", "another sha1", "another sha256", "another blake3", "another ssdeep"};

  HashBatch batch;
  batch.add(h1);
  REQUIRE(batch.Buf.size() == 42);
  batch.add(h2);
  REQUIRE(batch.Buf.size() == 112);
  REQUIRE(batch.size() == 2);

  LlamaDBAppender appender(conn.get(), "hash");
  REQUIRE(2 == batch.copyToDB(appender.get()));
  REQUIRE(appender.flush());

  duckdb_result result;
  auto state = duckdb_query(conn.get(), "SELECT * FROM hash;", &result);
  CHECK(state != DuckDBError);
  CHECK(duckdb_result_error(&result) == nullptr);
  CHECK(duckdb_row_count(&result) == 2);
  REQUIRE(duckdb_column_count(&result) == 6);
  unsigned int i = 0;
  REQUIRE(std::string("MetaAddr") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("MD5") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("SHA1") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("SHA256") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("Blake3") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("Ssdeep") == duckdb_column_name(&result, i));
  duckdb_destroy_result(&result);

  state = duckdb_query(conn.get(), "SELECT * FROM hash WHERE hash.metaaddr = 1;", &result);
  CHECK(state != DuckDBError);
  CHECK(duckdb_result_error(&result) == nullptr);
  CHECK(duckdb_row_count(&result) == 1);
  duckdb_destroy_result(&result);
}

