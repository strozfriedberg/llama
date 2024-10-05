#include <catch2/catch_test_macros.hpp>


#include "direntbatch.h"
#include "duckhash.h"
#include "duckinode.h"
#include "inode.h"
#include "llamaduck.h"

#include <duckdb.h>
#include <tuple>

#include <boost/pfr.hpp>

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

template<typename T>
struct DBType {
  typedef T BaseType;

  typedef decltype(boost::pfr::structure_to_tuple(T())) TupleType; // this requires default constructor... come back to it

  static constexpr auto& ColNames = T::ColNames;

  static constexpr auto NumCols = std::tuple_size_v<TupleType>;

  static_assert(ColNames.size() == NumCols, "The list of column names must match the number of fields in the tuple.");

  static constexpr auto colIndex(const char* col) {
    unsigned int i = 0;
    const auto nameLen = std::char_traits<char>::length(col);
    for (auto name : T::ColNames) {
      if (std::char_traits<char>::length(col) == nameLen && std::char_traits<char>::compare(col, name, nameLen) == 0) {
        break;
      }
      ++i;
    }
    return i;
  }

  static bool createTable(duckdb_connection& dbconn, const std::string& table) {
    auto result = duckdb_query(dbconn, createQuery<DBType<T>>(table.c_str()).c_str(), nullptr);
    return result != DuckDBError;
  }
};

struct DuckRecColumns {
public:
  static constexpr auto ColNames = {"path", "meta_addr", "parent_addr", "flags"};

  std::string path;
  uint64_t    meta_addr;
  uint64_t    parent_addr;
  std::string flags;
};

typedef DBType<DuckRecColumns> DuckRec;


template<typename T>
struct DBBatch {
  size_t size() const { return NumRows; }

  std::vector<char>    Buf; // strings stored in sequence here  
  std::vector<uint64_t> OffsetVals; // offsets to strings OR uint64_t values

  uint64_t NumRows = 0;

  size_t addString(uint64_t offset, const std::string& s) {
    OffsetVals.push_back(offset);
    std::copy_n(s.begin(), s.size() + 1, Buf.begin() + OffsetVals.back());
    return offset + s.size() + 1;
  }

  void clear() {
    Buf.clear();
    OffsetVals.clear();
    NumRows = 0;
  }

  template<typename Cur>
  void add(size_t offset, const Cur& cur) {
    if constexpr (std::is_convertible<Cur, std::string>()) {
      addString(offset, cur);
    }
    else if constexpr (std::is_integral_v<Cur>) {
      OffsetVals.push_back(cur);
    }
  }

  void add(const T& t) {
    auto&& tupes = boost::pfr::structure_tie(t);
    size_t startOffset = Buf.size();
    size_t totalSize = totalStringSize(tupes);
    Buf.resize(startOffset + totalSize);

    std::apply([&](auto&&... car) { // "cons car cdr"
      (add(startOffset, car), ...);
    }, tupes);

    ++NumRows;
  }

  template<size_t CurIndex>
  void appendRecord(duckdb_appender& appender, size_t index) {
    using ColumnType = typename std::tuple_element<CurIndex, typename DBType<T>::TupleType>::type;
    if constexpr (CurIndex > 0) {
      appendRecord<CurIndex - 1>(appender, index - 1);
    }
    if constexpr (std::is_integral_v<ColumnType>) {
      appendVal(appender, OffsetVals[index]);
    }
    else if constexpr (std::is_convertible_v<ColumnType, std::string>){
      appendVal(appender, Buf.data() + OffsetVals[index]);
    }
  }

  size_t copyToDB(duckdb_appender& appender) {
    size_t index = 0;
    for (size_t i = 0; i < NumRows; ++i) {
      duckdb_appender_begin_row(appender);

      appendRecord<DBType<T>::NumCols - 1>(appender, index + DBType<T>::NumCols - 1);

      duckdb_appender_end_row(appender);
      index += DBType<T>::NumCols;
    }
    return NumRows;
  }
};

TEST_CASE("testTypesFiguring") {
  REQUIRE(DuckRec::colIndex("meta_addr") == 1);
  REQUIRE(DuckRec::colIndex("parent_addr") == 2);
  REQUIRE(DuckRec::colIndex("path") == 0);
  REQUIRE(DuckRec::colIndex("flags") == 3);

  REQUIRE(std::string("VARCHAR") == duckdbType<std::string>());
  static_assert(std::char_traits<char>::compare(duckdbType<std::string>(), "VARCHAR", 7) == 0);
  REQUIRE(std::string("UBIGINT") == duckdbType<uint64_t>());
  static_assert(std::char_traits<char>::compare(duckdbType<uint64_t>(), "UBIGINT", 7) == 0);

  REQUIRE(createQuery<DuckRec>("DuckRec") == "CREATE TABLE DuckRec (path VARCHAR, meta_addr UBIGINT, parent_addr UBIGINT, flags VARCHAR);");

  REQUIRE(4 == DuckRecColumns::ColNames.size());
  REQUIRE(4 == DuckRec::ColNames.size());
  REQUIRE(4 == DuckRec::NumCols);

  DuckRecColumns drc{"/a/path", 21, 17, "File"};

  static_assert(std::is_same<decltype(boost::pfr::structure_to_tuple(drc)), std::tuple<std::string, uint64_t, uint64_t, std::string>>::value);
  REQUIRE(std::make_tuple("/a/path", 21, 17, "File") == boost::pfr::structure_to_tuple(drc));

  static_assert(std::is_same<DuckRec::TupleType, std::tuple<std::string, uint64_t, uint64_t, std::string>>::value);

  DBBatch<DuckRecColumns> batch;
  
  REQUIRE(13 == totalStringSize(boost::pfr::structure_to_tuple(drc)));

  batch.add(drc);
  REQUIRE(batch.size() == 1);
  REQUIRE(batch.Buf.size() == 13);
  REQUIRE(batch.OffsetVals.size() == 4);

  drc = DuckRecColumns{"/another/path", 22, 18, "Deleted"};

  batch.add(drc);
  REQUIRE(batch.size() == 2);
  REQUIRE(batch.Buf.size() == 35);
  REQUIRE(batch.OffsetVals.size() == 8);

  LlamaDB db;
  LlamaDBConnection conn(db);

  REQUIRE(DuckRec::createTable(conn.get(), "duckrec"));

  duckdb_result result;
  auto state = duckdb_query(conn.get(), "SELECT * FROM duckrec;", &result);
  CHECK(state != DuckDBError);
  CHECK(duckdb_result_error(&result) == nullptr);
  CHECK(duckdb_row_count(&result) == 0);
  duckdb_destroy_result(&result);

  LlamaDBAppender appender(conn.get(), "duckrec");
  REQUIRE(2 == batch.copyToDB(appender.get()));
  REQUIRE(appender.flush());

  state = duckdb_query(conn.get(), "SELECT * FROM duckrec;", &result);
  CHECK(state != DuckDBError);
  CHECK(duckdb_result_error(&result) == nullptr);
  CHECK(duckdb_row_count(&result) == 2);
  duckdb_destroy_result(&result);
}

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

