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
