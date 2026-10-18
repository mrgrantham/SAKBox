// GDom's JSON database, GJsonAsADatabase.
#include <gtest/gtest.h>

#include <WClasses/GDom.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace GClasses;

namespace {
// The database after the commands, and whether one of them threw
std::string jsonAfter(const std::vector<const char *> &cmds,
                      bool *pThrew = nullptr) {
  const char *tmp = getenv("TEST_TMPDIR");
  std::filesystem::path dir =
      std::filesystem::path(tmp ? tmp : "/tmp") / "jaad_test";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  {
    std::ofstream f(dir / "db.json");
    f << "{}"; // an empty database
  }
  GJsonAsADatabase db((dir.string() + "/").c_str());
  GDom response;
  for (const char *cmd : cmds) {
    try {
      db.apply("db.json", cmd, &response);
    } catch (const std::exception &) {
      if (!pThrew)
        throw;
      *pThrew = true;
    }
  }
  const GDomNode *root = db.apply("db.json", "", &response);
  std::ostringstream os;
  if (root)
    root->writeJson(os);
  return os.str();
}
} // namespace

// (Through "+=", which uses findNode; "=" goes through findLValue)
TEST(JsonAsADatabaseTest, MissingFieldsAreCreatedByWhatComesNext) {
  // Nothing after the field: "+=" makes it a list to append to
  EXPECT_EQ(jsonAfter({".c += 2"}), "{\"c\":[2]}");
  // '.' after it: an object
  EXPECT_EQ(jsonAfter({".e.f += 4"}), "{\"e\":{\"f\":[4]}}");
  // '[' after it: a list. (Indexing the new, empty list then fails, so
  // this leaves an empty list behind and throws.)
  bool threw = false;
  EXPECT_EQ(jsonAfter({".h[0] += 6"}, &threw), "{\"h\":[]}");
  EXPECT_TRUE(threw);
}

// Assigning ("=") a field that didn't exist yet went wrong in findLValue:
// with more path after it (".a.b = 1"), it called add on the null pointer
// it had just got back, crashing; as the last field (".a = 1") it returned
// that null, which apply took to mean "no document" and replaced the whole
// database with the value. Indexing one past the end of a list with more
// path after it (".l[1].x = 2") appended a node, then looked up the index
// in that new node instead of the list.
TEST(JsonAsADatabaseTest, AssigningCreatesWhatsMissing) {
  EXPECT_EQ(jsonAfter({".a = 1"}), "{\"a\":1}");
  EXPECT_EQ(jsonAfter({".a = 1", ".b = 2"}), "{\"a\":1,\"b\":2}");
  EXPECT_EQ(jsonAfter({".a.b = 1"}), "{\"a\":{\"b\":1}}");
  EXPECT_EQ(jsonAfter({".a[0] = 5"}), "{\"a\":[5]}");
  EXPECT_EQ(jsonAfter({".l += 1", ".l[1].x = 2"}), "{\"l\":[1,{\"x\":2}]}");
  // Existing fields and elements are still replaced
  EXPECT_EQ(jsonAfter({".a = 1", ".a = 3"}), "{\"a\":3}");
  EXPECT_EQ(jsonAfter({".l += 1", ".l[0] = 9"}), "{\"l\":[9]}");
}
